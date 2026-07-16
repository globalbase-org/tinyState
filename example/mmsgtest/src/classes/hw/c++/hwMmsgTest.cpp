/*
 * hwMmsgTest — batched-datagram (recvmmsg/sendmmsg) runtime test.  Windows-port design memo
 * throughput step #1 (see socket-ipc-throughput-roadmap).
 *
 * Two ts2IOsockUDP sockets exchange a BATCH of NMSG datagrams each way over
 * 127.0.0.1: A sends NMSG via sendmmsg, B collects them via recvmmsg, then the
 * reverse.  On Linux this drives the native ::sendmmsg/::recvmmsg one-syscall
 * path; on the MinGW winsock port it drives the per-datagram WSASendMsg/
 * WSARecvMsg fallback — same interface, so this one example verifies both.
 *
 * recvmmsg follows POSIX "get >=1, don't block for the rest", so a single call
 * may return fewer than requested; the receiver LOOPS recvmmsg into the tail of
 * the vector until it has collected all NMSG (each call is its own 1-state-1-I/O
 * state, resuming its pending op exactly like recvfrom).  Payloads are verified
 * as a multiset so UDP reordering can't cause a false failure.
 *
 * All message vectors / buffers / source-address slots are MEMBERS so they
 * survive the yield across the (overlapped, on Windows) op — the addr/msg
 * lifetime contract, Windows-port design memo.
 */
#include	"std2/ts_mmsg.h"	/* struct mmsghdr (POSIX native / MinGW shim) */
#define NMSG	4		/* must precede _.h: member arrays are smsg[NMSG] etc. */
#include	"_ts2/c++/hwMmsgTest_.h"
#include	<string.h>
#include	<stdio.h>

CLASS_TINYSTATE(hw/c++/hwMmsgTest,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockUDP.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsSignal.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwMmsgTest_(sPtr<tinyState> parent, int portA, int portB);
protected:
	TS_DEFARGS
	sPtr<tinyState>		sig_pipe;
	sPtr<ts2IOsockUDP>	udpA;
	sPtr<ts2IOsockUDP>	udpB;

	struct mmsghdr		smsg[NMSG];	/* send vector */
	struct iovec		siov[NMSG];
	char			sbuf[NMSG][8];
	struct mmsghdr		rmsg[NMSG];	/* recv vector */
	struct iovec		riov[NMSG];
	char			rbuf[NMSG][8];
	struct sockaddr		rname[NMSG];	/* recv source addrs (persist over yield) */

	struct sockaddr_in	destB;		/* 127.0.0.1:portB */
	struct sockaddr		destA;		/* A as seen by B (reverse dest) */
	int			destAlen;

	int			sent;
	int			rcvd;
	int			ready;
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IOsockUDP;
TS_END_INTERFACE
#endif

hwMmsgTest_::hwMmsgTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

/* fill the send vector: NMSG datagrams "MSG0".."MSG3" toward (name,namelen). */
static void
build_send(struct mmsghdr * smsg,struct iovec * siov,char sbuf[][8],
		struct sockaddr * name,int namelen)
{
	for ( int i = 0; i < NMSG; i++ ) {
		::snprintf(sbuf[i],sizeof(sbuf[i]),"MSG%d",i);
		siov[i].iov_base = sbuf[i];
		siov[i].iov_len  = 4;
		memset(&smsg[i],0,sizeof(smsg[i]));
		smsg[i].msg_hdr.msg_name    = name;
		smsg[i].msg_hdr.msg_namelen = namelen;
		smsg[i].msg_hdr.msg_iov     = &siov[i];
		smsg[i].msg_hdr.msg_iovlen  = 1;
	}
}

/* fill the recv vector: NMSG slots, each with its own buffer + source-addr. */
static void
build_recv(struct mmsghdr * rmsg,struct iovec * riov,char rbuf[][8],
		struct sockaddr * rname)
{
	for ( int i = 0; i < NMSG; i++ ) {
		riov[i].iov_base = rbuf[i];
		riov[i].iov_len  = sizeof(rbuf[i]);
		memset(&rmsg[i],0,sizeof(rmsg[i]));
		rmsg[i].msg_hdr.msg_name    = &rname[i];
		rmsg[i].msg_hdr.msg_namelen = sizeof(struct sockaddr);
		rmsg[i].msg_hdr.msg_iov     = &riov[i];
		rmsg[i].msg_hdr.msg_iovlen  = 1;
	}
}

/* verify the NMSG received payloads are exactly {MSG0..MSG3} (multiset). */
static int
verify_batch(struct mmsghdr * rmsg,char rbuf[][8])
{
int seen[NMSG] = {0,0,0,0};
	for ( int i = 0; i < NMSG; i++ ) {
		if ( rmsg[i].msg_len != 4 )
			return 0;
		if ( memcmp(rbuf[i],"MSG",3) != 0 )
			return 0;
	int d = rbuf[i][3] - '0';
		if ( d < 0 || d >= NMSG || seen[d] )
			return 0;
		seen[d] = 1;
	}
	return 1;
}

TS_STATE(INI_START)
{
#ifndef _WIN32
	sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
#endif
	ready = 0;
	udpA = thNEW(ts2IOsockUDP,(ifThis,portA));
	udpB = thNEW(ts2IOsockUDP,(ifThis,portB));
	return ACT_WAIT_READY;
}

TS_STATE(ACT_WAIT_READY)		/* both sockets bound (2 wakeups) */
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( ++ready < 2 )
		return 0;
	if ( udpA->err != 0 || udpB->err != 0 ) {
		::printf("[mmsgtest] bind failed (A err=%d, B err=%d)\n",udpA->err,udpB->err);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_WAIT_ASSOC;
}

TS_STATE(ACT_WAIT_ASSOC)		/* both past INI (io set + IOCP-associated) */
{
	if ( C_TEST(udpA->tinyState::state(),C_INI) ||
			C_TEST(udpB->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,1000,TSE_TIMER);
		return ACT_WAIT_ASSOC_TICK;
	}
	return rDO|ACT_PREP_AB;
}
TS_STATE(ACT_WAIT_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_ASSOC;
}

/* ---- A --sendmmsg NMSG--> B ---- */
TS_STATE(ACT_PREP_AB)
{
	memset(&destB,0,sizeof(destB));
	destB.sin_family = AF_INET;
	destB.sin_port = htons(portB);
	destB.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	build_send(smsg,siov,sbuf,(struct sockaddr*)&destB,sizeof(destB));
	build_recv(rmsg,riov,rbuf,rname);
	sent = rcvd = 0;
	return rDO|ACT_SEND_AB;
}
TS_STATE(ACT_SEND_AB)
{
int n = udpA->sendmmsg(&smsg[sent],(unsigned)(NMSG-sent),0);
	if ( n < 0 ) {
		::printf("[mmsgtest] sendmmsg A->B failed (err=%d)\n",udpA->err);
		return rDO|ACT_CLEANUP;
	}
	sent += n;
	if ( sent < NMSG )
		return rDO|ACT_SEND_AB;
	return rDO|ACT_RECV_AB;
}
TS_STATE(ACT_RECV_AB)
{
int n = udpB->recvmmsg(&rmsg[rcvd],(unsigned)(NMSG-rcvd),0,NULL);
	if ( n < 0 ) {
		::printf("[mmsgtest] recvmmsg B failed (err=%d)\n",udpB->err);
		return rDO|ACT_CLEANUP;
	}
	rcvd += n;
	if ( rcvd < NMSG )
		return rDO|ACT_RECV_AB;
	if ( !verify_batch(rmsg,rbuf) ) {
		::printf("[mmsgtest] A->B batch verify FAILED\n");
		return rDO|ACT_CLEANUP;
	}
	/* remember A's address (source of the datagrams B just got) for the reply */
	memcpy(&destA,&rname[0],sizeof(destA));
	destAlen = (int)rmsg[0].msg_hdr.msg_namelen;
	if ( destAlen <= 0 )
		destAlen = sizeof(struct sockaddr);
	return rDO|ACT_PREP_BA;
}

/* ---- B --sendmmsg NMSG--> A ---- */
TS_STATE(ACT_PREP_BA)
{
	build_send(smsg,siov,sbuf,&destA,destAlen);
	build_recv(rmsg,riov,rbuf,rname);
	sent = rcvd = 0;
	return rDO|ACT_SEND_BA;
}
TS_STATE(ACT_SEND_BA)
{
int n = udpB->sendmmsg(&smsg[sent],(unsigned)(NMSG-sent),0);
	if ( n < 0 ) {
		::printf("[mmsgtest] sendmmsg B->A failed (err=%d)\n",udpB->err);
		return rDO|ACT_CLEANUP;
	}
	sent += n;
	if ( sent < NMSG )
		return rDO|ACT_SEND_BA;
	return rDO|ACT_RECV_BA;
}
TS_STATE(ACT_RECV_BA)
{
int n = udpA->recvmmsg(&rmsg[rcvd],(unsigned)(NMSG-rcvd),0,NULL);
	if ( n < 0 ) {
		::printf("[mmsgtest] recvmmsg A failed (err=%d)\n",udpA->err);
		return rDO|ACT_CLEANUP;
	}
	rcvd += n;
	if ( rcvd < NMSG )
		return rDO|ACT_RECV_BA;
	if ( !verify_batch(rmsg,rbuf) ) {
		::printf("[mmsgtest] B->A batch verify FAILED\n");
		return rDO|ACT_CLEANUP;
	}
	::printf("[mmsgtest] batch OK (%d msgs each way via mmsg)\n",NMSG);
	return rDO|ACT_CLEANUP;
}

TS_STATE(ACT_CLEANUP)
{
	if ( udpA != thNULL )		udpA->destroy();
	if ( udpB != thNULL )		udpB->destroy();
	if ( sig_pipe != thNULL )	sig_pipe->destroy();
	udpA = thNULL;
	udpB = thNULL;
	sig_pipe = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
