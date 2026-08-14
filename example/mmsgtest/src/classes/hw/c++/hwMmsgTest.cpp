/*
 * hwMmsgTest — batched-datagram (recvmmsg/sendmmsg) runtime test.  Windows-port design memo
 * throughput step #1 (see socket-ipc-throughput-roadmap).
 *
 * Two ROLES exchange a BATCH of NMSG datagrams each way over 127.0.0.1:
 *   role 0 (client)    — owns the socket on portA, spawns a responder child on portB, waits a
 *                        warm-up beat, then sendmmsg NMSG → recvmmsg NMSG (the echo) and
 *                        verifies what came back.
 *   role 1 (responder) — owns the socket on portB, LOOPS recvmmsg → sendmmsg, verifying each
 *                        batch it collects and replying to the address recvmmsg gave it.
 * On Linux this drives the native ::sendmmsg/::recvmmsg one-syscall path; on the MinGW
 * winsock port it drives the RIO / WSASendMsg-WSARecvMsg path — same interface, so this one
 * example verifies both.
 *
 * WHY two tinyStates (was one state machine driving two sockets): the receiving side
 * has to have a receive POSTED before the other side sends.  A RIO socket has no kernel
 * receive buffer — a datagram that arrives with no posted RIOReceiveEx is dropped — and RIO
 * is built lazily on the first recvmmsg/sendmmsg, so a single state machine that sent the
 * whole batch and only then called recvmmsg on the other socket lost every datagram and
 * timed out.  It failed 100% on a real RIO-capable Windows box while passing everywhere the
 * plain path is used (Linux, macOS, Cygwin, wine — where RIO is unavailable and the socket
 * degrades to kernel-buffered overlapped I/O), which is exactly why it went unnoticed.
 * Splitting sender and responder lets the responder park its recvmmsg — which posts the
 * receives — BEFORE the client sends.  udptest was restructured for the same reason.
 *
 * recvmmsg follows POSIX "get >=1, don't block for the rest", so a single call may return
 * fewer than requested; both roles LOOP recvmmsg into the tail of the vector until they have
 * collected all NMSG (each call is its own 1-state-1-I/O state, resuming its pending op
 * exactly like recvfrom).  Payloads are verified as a multiset so UDP reordering can't cause
 * a false failure.
 *
 * All message vectors / buffers / source-address slots are MEMBERS so they survive the yield
 * across the (overlapped, on Windows) op — the addr/msg lifetime contract, Windows-port
 * design memo.
 */
#include	"std2/ts_mmsg.h"	/* struct mmsghdr (POSIX native / MinGW shim) */
#define NMSG	4		/* must precede _.h: member arrays are smsg[NMSG] etc. */
#define RECV_TIMEOUT_US	2000000	/* per-direction batch deadline (2 s; loopback needs µs) */
#define READY_TIMEOUT_US 5000000 /* socket bind/associate deadline (5 s) */
#define WARMUP_US	300000	/* let the responder bind and park its recvmmsg (posts receives) */
#include	"_ts2/c++/hwMmsgTest_.h"
#include	<string.h>
#include	<stdio.h>

CLASS_TINYSTATE(hw/c++/hwMmsgTest,ts2/c++/tinyState)

int	mmsgtest_failed = 0;

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockUDP.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsSignal.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief client (role 0): bind portA, spawn a responder on portB, batch → echo → verify. */
	hwMmsgTest_(sPtr<tinyState> parent, int portA, int portB);
	/** @brief role-explicit ctor.  role 0 = client, role 1 = responder (bind portB, loop
	 *  recvmmsg → sendmmsg replying to the source).  See the file header for why the sender
	 *  and the receiver are separate tinyStates (a RIO receive must be posted first). */
	hwMmsgTest_(sPtr<tinyState> parent, int portA, int portB, int role);
protected:
	TS_DEFARGS
	sPtr<tinyState>		responder;	/* client: the child responder */
	sPtr<ts2IOsockUDP>	udp;		/* this role's socket */

	struct mmsghdr		smsg[NMSG];	/* send vector */
	struct iovec		siov[NMSG];
	char			sbuf[NMSG][8];
	struct mmsghdr		rmsg[NMSG];	/* recv vector */
	struct iovec		riov[NMSG];
	char			rbuf[NMSG][8];
	struct sockaddr		rname[NMSG];	/* recv source addrs (persist over yield) */

	struct sockaddr_in	destB;		/* client: 127.0.0.1:portB */
	struct sockaddr		peer;		/* responder: the source recvmmsg reported */
	int			peerlen;

	int			sent;
	int			rcvd;
	INTEGER64		deadline;	/* stdInterval::now() when the current phase gives up */
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IOsockUDP;
TS_END_INTERFACE
#endif

hwMmsgTest_::hwMmsgTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }	/* client (role 0) */
hwMmsgTest_::hwMmsgTest_(TS_ARGS1) : tinyState_(parent) { TS_CPARGS1 }	/* role-explicit */

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

/* No tsSignal(SIGPIPE) here: tsApplication installs a process-wide no-op
 * SIGPIPE handler for the whole run.  Owning a second one and destroying it in
 * ACT_CLEANUP left SIGPIPE at SIG_DFL for the teardown that follows, so the
 * THR_KILL(SIGPIPE) burst killed the process (rc=141) in ~2.5% of runs even
 * though the test itself had already passed. */
TS_STATE(INI_START)
{
	udp = thNEW(ts2IOsockUDP,(ifThis,(role == 1) ? portB : portA));
	if ( role != 1 )
		responder = thNEW(hwMmsgTest,(ifThis,portA,portB,1));
	deadline = stdInterval::now() + READY_TIMEOUT_US;
	return rDO|ACT_WAIT_READY;
}

/* Our own socket is bound and past INI (io set + IOCP-associated).
 *
 * This POLLS the socket rather than counting TSE_WAKEUPs.  Counting wakeups looks natural
 * but hangs about 1% of runs: when two of them complete almost simultaneously the state
 * function is re-run once, not twice, so the counter stops short and the test waits forever
 * for a wakeup that has already been and gone.  Polling has no such assumption, and the
 * deadline turns even a socket that never becomes ready into an explicit failure. */
TS_STATE(ACT_WAIT_READY)
{
	if ( C_TEST(udp->tinyState::state(),C_INI) ) {
		if ( stdInterval::now() >= deadline ) {
			::printf("[mmsgtest] socket not ready within %d ms (role=%d, state=0x%x)\n",
					READY_TIMEOUT_US/1000,role,
					(unsigned)udp->tinyState::state());
			mmsgtest_failed = 1;
			return rDO|ACT_CLEANUP;
		}
		stdInterval::wait(ifThis,1000,TSE_TIMER);
		return ACT_WAIT_ASSOC_TICK;
	}
	if ( udp->err != 0 ) {
		::printf("[mmsgtest] bind failed (role=%d, err=%d)\n",role,udp->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	return (role == 1) ? (rDO|ACT_RESP_PREP) : (rDO|ACT_CLI_WARMUP);
}
TS_STATE(ACT_WAIT_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_READY;
}


/* ---- role 1: responder — recvmmsg a batch, verify it, echo it back, repeat ---- */

TS_STATE(ACT_RESP_PREP)
{
	if ( is_destroyed() )
		return rDO|ACT_CLEANUP;
	build_recv(rmsg,riov,rbuf,rname);
	rcvd = 0;
	return rDO|ACT_RESP_RECV;
}
TS_STATE(ACT_RESP_RECV)			/* parks here — which is what posts the receives */
{
	if ( is_destroyed() )		/* the client is done: a parked recvmmsg is re-run on
					   destroy, and without this guard it just parks again */
		return rDO|ACT_CLEANUP;
int n = udp->recvmmsg(&rmsg[rcvd],(unsigned)(NMSG-rcvd),0,NULL);
	if ( n < 0 )
		return rDO|ACT_CLEANUP;			/* socket torn down (EBADF) */
	rcvd += n;
	if ( rcvd < NMSG )
		return rDO|ACT_RESP_RECV;
	if ( !verify_batch(rmsg,rbuf) ) {
		::printf("[mmsgtest] A->B batch verify FAILED\n");
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	/* reply to the source recvmmsg reported */
	memcpy(&peer,&rname[0],sizeof(peer));
	peerlen = (int)rmsg[0].msg_hdr.msg_namelen;
	if ( peerlen <= 0 )
		peerlen = sizeof(struct sockaddr);
	build_send(smsg,siov,sbuf,&peer,peerlen);
	sent = 0;
	return rDO|ACT_RESP_SEND;
}
TS_STATE(ACT_RESP_SEND)
{
int n = udp->sendmmsg(&smsg[sent],(unsigned)(NMSG-sent),0);
	if ( n < 0 ) {
		::printf("[mmsgtest] sendmmsg B->A failed (err=%d)\n",udp->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	sent += n;
	if ( sent < NMSG )
		return rDO|ACT_RESP_SEND;
	return rDO|ACT_RESP_PREP;			/* serve the next batch */
}


/* ---- role 0: client — warm up, sendmmsg the batch, collect the echo, verify ---- */

TS_STATE(ACT_CLI_WARMUP)		/* let the responder bind and park its recvmmsg */
{
	stdInterval::wait(ifThis,WARMUP_US,TSE_TIMER);
	return ACT_CLI_WARMUP_TICK;
}
TS_STATE(ACT_CLI_WARMUP_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_PREP_AB;
}

TS_STATE(ACT_PREP_AB)
{
	memset(&destB,0,sizeof(destB));
	destB.sin_family = AF_INET;
	destB.sin_port = htons(portB);
	destB.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	build_send(smsg,siov,sbuf,(struct sockaddr*)&destB,sizeof(destB));
	build_recv(rmsg,riov,rbuf,rname);
	sent = rcvd = 0;
	/* Arm the deadline before the send so a lost datagram cannot hang the recv.
	 * TSE_TIMER2, not TSE_TIMER: the assoc poll and the warm-up above use TSE_TIMER and
	 * `ev` still carries that event down the rDO chain into the recv state, where a
	 * type-only guard would fire the deadline immediately. */
	deadline = stdInterval::now() + RECV_TIMEOUT_US;
	stdInterval::wait(ifThis,RECV_TIMEOUT_US,TSE_TIMER2);
	return rDO|ACT_SEND_AB;
}
TS_STATE(ACT_SEND_AB)
{
int n = udp->sendmmsg(&smsg[sent],(unsigned)(NMSG-sent),0);
	if ( n < 0 ) {
		::printf("[mmsgtest] sendmmsg A->B failed (err=%d)\n",udp->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	sent += n;
	if ( sent < NMSG )
		return rDO|ACT_SEND_AB;
	return rDO|ACT_RECV_AB;
}
TS_STATE(ACT_RECV_AB)			/* the responder's echo (B --sendmmsg--> A) */
{
	/* Deadline first: recvmmsg yields on EAGAIN and this state is re-run on whatever
	 * event wakes us, so the timer arrives here.  The guard only exits early — the I/O
	 * below stays unconditional (CLAUDE.md rule 5).  The clock is checked as well as the
	 * type, so a stale `ev` inherited through an rDO chain can never be mistaken for our
	 * deadline. */
	if ( ev->type == TSE_TIMER2 && stdInterval::now() >= deadline ) {
		::printf("[mmsgtest] batch TIMEOUT — got %d/%d msgs back in %d ms"
				" (UDP drop?)\n",rcvd,NMSG,RECV_TIMEOUT_US/1000);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
int n = udp->recvmmsg(&rmsg[rcvd],(unsigned)(NMSG-rcvd),0,NULL);
	if ( n < 0 ) {
		::printf("[mmsgtest] recvmmsg A failed (err=%d)\n",udp->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	rcvd += n;
	if ( rcvd < NMSG )
		return rDO|ACT_RECV_AB;
	stdInterval::detach(ifThis);		/* batch complete — disarm */
	if ( !verify_batch(rmsg,rbuf) ) {
		::printf("[mmsgtest] B->A batch verify FAILED\n");
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	::printf("[mmsgtest] batch OK (%d msgs each way via mmsg)\n",NMSG);
	return rDO|ACT_CLEANUP;
}

TS_STATE(ACT_CLEANUP)
{
	stdInterval::detach(ifThis);	/* no stray TSE_TIMER during teardown */
	if ( responder != thNULL )	responder->destroy();
	if ( udp != thNULL )		udp->destroy();
	responder = thNULL;
	udp = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
