/*
 * hwRioBench — datagram throughput / burst-loss harness for the MinGW winsock port,
 * throughput roadmap #1/#2 (see socket-ipc-throughput-roadmap / Windows-port memo).
 *
 * Two ROLES run as SEPARATE PROCESSES (loopback = two processes on one host, or two
 * hosts over a real NIC):
 *
 *   role 1 (server) — bind <port>, loop recvmmsg counting DATA datagrams until the
 *                     client's END sentinel arrives, then report received / expected
 *                     / loss and the receive rate.  ENHANCED(RIO): set_rio_recv_depth
 *                     picks the concurrently-posted receive count — depth 1 = Phase A
 *                     single-posted, depth 4 = Phase B keep-N-posted.  That knob is the
 *                     whole point: RIO delivers ONLY into an already-posted receive (no
 *                     kernel receive buffer), so a burst arriving in the completion→
 *                     re-post window is dropped when only one receive is posted.  Plain
 *                     (non-ENHANCED) is kernel-buffered so it never shows this — hence we
 *                     compare enhanced depth 1 vs depth 4, not plain vs enhanced.
 *
 *   role 0 (client) — bind ephemeral, blast N DATA datagrams of <size> bytes toward
 *                     <ip>:<port> in sendmmsg batches of <batch>, then send a run of END
 *                     sentinels (spaced out, so at least one lands while the server is
 *                     parked with a receive posted), and report sent + send rate.
 *
 * Protocol (datagram byte 0 = kind):
 *   'D' + seq(be32) + padding   — a data datagram
 *   'E' + N(be32)               — end sentinel (N = total data datagrams the client sent)
 *
 * All mmsg vectors / iovecs / buffers / source-addr slots are MEMBERS so they survive
 * the yield across the (overlapped / RIO) op — the addr/buffer lifetime contract.  Each
 * sendmmsg/recvmmsg is its own ev-independent state (one batched op per state).
 */
#include	"std2/ts_mmsg.h"	/* struct mmsghdr (POSIX native / MinGW shim) */
#define VMAX	256		/* max mmsg vector length (member arrays; must precede _.h) */
#define VBATCH_DEF	16	/* effective vector length if the vbatch arg is 0/omitted */
#define MAXSZ	2048		/* per-datagram buffer cap (members are [.][MAXSZ]) */
#define END_REPEAT	20	/* END sentinels sent, spaced END_GAP_MS apart */
#define END_GAP_MS	20
#include	"_ts2/c++/hwRioBench_.h"
#include	<string.h>
#include	<stdio.h>
#ifndef _WIN32
#include	<netinet/in.h>	/* sockaddr_in, htons */
#include	<arpa/inet.h>	/* inet_addr */
#endif

CLASS_TINYSTATE(hw/c++/hwRioBench,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockUDP.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/tsSignal.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief role 0 = client (blast N), role 1 = server (count + report loss).
	 *  vbatch = effective sendmmsg/recvmmsg vector length (mmsg messages handed per call). */
	/* NOTE: `mode` is retained for CLI arg-position stability but is now IGNORED — the
	   datagram path is automatic (RIO by default, plain via env TS2_DISABLE_RIO). */
	hwRioBench_(sPtr<tinyState> parent, int role, int port, int mode, int depth,
			sPtr<stdString> host, int N, int size, int batch, int vbatch);
protected:
	TS_DEFARGS
	sPtr<tinyState>		sig_pipe;
	sPtr<ts2IOsockUDP>	udp;
	char			hostbuf[128];

	struct sockaddr_in	dest;		/* client: <ip>:<port> */

	struct mmsghdr		smsg[VMAX];	/* client send vector */
	struct iovec		siov[VMAX];
	char			sbuf[VMAX][MAXSZ];
	char			ebuf[8];	/* END sentinel payload */

	struct mmsghdr		rmsg[VMAX];	/* server recv vector */
	struct iovec		riov[VMAX];
	char			rbuf[VMAX][MAXSZ];
	struct sockaddr		rname[VMAX];

	int			vb;		/* effective mmsg vector length (clamped vbatch) */
	int			sent;		/* client: data datagrams handed to sendmmsg */
	int			end_n;		/* client: END sentinels sent so far */
	int			dlen;		/* client: data datagram length (>=5, <=MAXSZ) */
	int			rcvd;		/* server: DATA datagrams counted */
	int			expected;	/* server: N carried by the END sentinel */
	INTEGER64		t0;		/* start of the data phase (µs) */
	INTEGER64		t1;		/* end of the data phase (µs) */
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IOsockUDP;
class stdString;
TS_END_INTERFACE
#endif

hwRioBench_::hwRioBench_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }


/* --- payload helpers --- */
static void put_be32(char * p,unsigned int v)
{
	p[0]=(char)((v>>24)&0xff); p[1]=(char)((v>>16)&0xff);
	p[2]=(char)((v>>8)&0xff);  p[3]=(char)(v&0xff);
}
static unsigned int get_be32(const char * p)
{
	return ((unsigned int)(unsigned char)p[0]<<24) | ((unsigned int)(unsigned char)p[1]<<16)
	     | ((unsigned int)(unsigned char)p[2]<<8)  |  (unsigned int)(unsigned char)p[3];
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
#ifndef _WIN32
	sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
#endif
	vb = (vbatch <= 0) ? VBATCH_DEF : (vbatch > VMAX ? VMAX : vbatch);
	sent = end_n = rcvd = expected = 0;
	t0 = t1 = 0;
	dlen = size < 5 ? 5 : (size > MAXSZ ? MAXSZ : size);

	if ( role == 1 ) {			/* server: bind the well-known port */
		udp = thNEW(ts2IOsockUDP,(ifThis,port));
		if ( batch > 0 )
			udp->set_rio_ring_slots(batch);	/* RIO ring depth (throughput knob); batch carries slots for srv */
		if ( depth > 0 )
			udp->set_rio_recv_depth(depth);	/* Phase A(1) vs Phase B(n) posted cap */
	} else {				/* client: ephemeral bind, resolve dest */
		udp = thNEW(ts2IOsockUDP,(ifThis,0));
		if ( batch > 0 )
			udp->set_rio_ring_slots(batch);	/* deeper send ring = faster blaster (throughput knob) */
		memset(hostbuf,0,sizeof(hostbuf));
		if ( host != thNULL )
			host->copyout(hostbuf,sizeof(hostbuf));
	}
	return ACT_WAIT_READY;
}

TS_STATE(ACT_WAIT_READY)			/* our socket bound (its INI wakes us) */
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( udp->err != 0 ) {
		::printf("[riobench] bind failed (err=%d)\n",udp->err);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_WAIT_ASSOC;
}
TS_STATE(ACT_WAIT_ASSOC)			/* past C_INI = io set + associated */
{
	if ( C_TEST(udp->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,20,TSE_TIMER);
		return ACT_WAIT_ASSOC_TICK;
	}
	return role == 1 ? (rDO|ACT_SRV_PREP) : (rDO|ACT_CLI_PREP);
}
TS_STATE(ACT_WAIT_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_ASSOC;
}


/* ---------------- server (role 1) ---------------- */

TS_STATE(ACT_SRV_PREP)
{
	for ( int i = 0 ; i < vb ; i++ ) {
		riov[i].iov_base = rbuf[i];
		riov[i].iov_len  = MAXSZ;
		memset(&rmsg[i],0,sizeof(rmsg[i]));
		rmsg[i].msg_hdr.msg_name    = &rname[i];
		rmsg[i].msg_hdr.msg_namelen = sizeof(struct sockaddr);
		rmsg[i].msg_hdr.msg_iov     = &riov[i];
		rmsg[i].msg_hdr.msg_iovlen  = 1;
	}
	::printf("[riobench] srv listening port=%d depth=%d slots=%d vbatch=%d\n",port,depth,batch,vb);
	return rDO|ACT_SRV_RECV;
}

TS_STATE(ACT_SRV_RECV)				/* recvmmsg; parks (yields) when the ring is empty */
{
	if ( is_destroyed() )
		return rDO|ACT_CLEANUP;
	{
	int n = udp->recvmmsg(rmsg,(unsigned)vb,0,NULL);
		if ( n < 0 )
			return rDO|ACT_CLEANUP;		/* socket torn down */
		for ( int i = 0 ; i < n ; i++ ) {
		char * p = rbuf[i];
			if ( rmsg[i].msg_len >= 5 && p[0] == 'E' ) {	/* END sentinel */
				expected = (int)get_be32(p+1);
				return rDO|ACT_SRV_DONE;
			}
			if ( rmsg[i].msg_len >= 5 && p[0] == 'D' ) {	/* data datagram */
			INTEGER64 now = stdInterval::now();
				if ( t0 == 0 ) t0 = now;
				t1 = now;
				rcvd++;
			}
		}
	}
	return rDO|ACT_SRV_RECV;
}

TS_STATE(ACT_SRV_DONE)
{
	{
	INTEGER64 us  = (t1 > t0) ? (t1 - t0) : 0;
	int       loss = expected - rcvd;
	double    secs = (double)us / 1e6;
	double    pps  = secs > 0 ? (double)rcvd / secs : 0.0;
		::printf("[riobench] srv done: received=%d expected=%d loss=%d (%.2f%%) "
			"elapsed=%.3fms rate=%.0f pkt/s\n",
			rcvd,expected,loss,
			expected > 0 ? 100.0*(double)loss/(double)expected : 0.0,
			(double)us/1000.0,pps);
	}
	return rDO|ACT_CLEANUP;
}


/* ---------------- client (role 0) ---------------- */

TS_STATE(ACT_CLI_PREP)
{
	memset(&dest,0,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port   = htons(port);
	dest.sin_addr.s_addr = inet_addr(hostbuf[0] ? hostbuf : "127.0.0.1");
	for ( int i = 0 ; i < vb ; i++ ) {
		memset(sbuf[i],0,(size_t)dlen);
		sbuf[i][0] = 'D';
		siov[i].iov_base = sbuf[i];
		siov[i].iov_len  = (size_t)dlen;
		memset(&smsg[i],0,sizeof(smsg[i]));
		smsg[i].msg_hdr.msg_name    = (struct sockaddr*)&dest;
		smsg[i].msg_hdr.msg_namelen = sizeof(dest);
		smsg[i].msg_hdr.msg_iov     = &siov[i];
		smsg[i].msg_hdr.msg_iovlen  = 1;
	}
	return rDO|ACT_CLI_SEND;
}

TS_STATE(ACT_CLI_SEND)				/* blast the burst; parks when the send ring is full */
{
	if ( sent >= N )
		return rDO|ACT_CLI_END_PREP;
	if ( t0 == 0 )
		t0 = stdInterval::now();
	{
	int room = N - sent;
	int cnt  = room < vb ? room : vb;
		for ( int i = 0 ; i < cnt ; i++ )		/* stamp seq for this window */
			put_be32(sbuf[i]+1,(unsigned)(sent+i));
	int n = udp->sendmmsg(smsg,(unsigned)cnt,0);
		if ( n < 0 ) {
			::printf("[riobench] sendmmsg failed (err=%d)\n",udp->err);
			return rDO|ACT_CLEANUP;
		}
		sent += n;
	}
	return rDO|ACT_CLI_SEND;
}

TS_STATE(ACT_CLI_END_PREP)
{
	t1 = stdInterval::now();
	ebuf[0] = 'E';
	put_be32(ebuf+1,(unsigned)N);
	return rDO|ACT_CLI_END_SEND;
}
TS_STATE(ACT_CLI_END_SEND)			/* one END sentinel per state (1 I/O per state) */
{
	if ( end_n >= END_REPEAT )
		return rDO|ACT_CLI_DONE;
	udp->sendto(ebuf,5,0,(struct sockaddr*)&dest,sizeof(dest));
	end_n++;
	stdInterval::wait(ifThis,END_GAP_MS,TSE_TIMER);
	return ACT_CLI_END_TICK;
}
TS_STATE(ACT_CLI_END_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_CLI_END_SEND;
}

TS_STATE(ACT_CLI_DONE)
{
	{
	INTEGER64 us  = (t1 > t0) ? (t1 - t0) : 0;
	double    secs = (double)us / 1e6;
	double    pps  = secs > 0 ? (double)sent / secs : 0.0;
		::printf("[riobench] cli done: sent=%d size=%d elapsed=%.3fms rate=%.0f pkt/s\n",
			sent,dlen,(double)us/1000.0,pps);
	}
	return rDO|ACT_CLEANUP;
}


/* ---------------- shared cleanup ---------------- */

TS_STATE(ACT_CLEANUP)
{
	if ( udp != thNULL )		udp->destroy();
	if ( sig_pipe != thNULL )	sig_pipe->destroy();
	udp = thNULL;
	sig_pipe = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
