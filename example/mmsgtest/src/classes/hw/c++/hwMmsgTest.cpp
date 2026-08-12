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
 *
 * UDP may drop a datagram even on loopback (a full receive buffer is enough),
 * and a batch that loses one message would otherwise leave the receiver waiting
 * for a message that will never arrive — a silent HANG rather than a result.
 * Each receive phase therefore arms a deadline (RECV_TIMEOUT_US) and reports an
 * explicit TIMEOUT failure if the batch does not complete in time.  The process
 * exit status is non-zero for every failure, so a test loop can detect it.
 */
#include	"std2/ts_mmsg.h"	/* struct mmsghdr (POSIX native / MinGW shim) */
#define NMSG	4		/* must precede _.h: member arrays are smsg[NMSG] etc. */
#define RECV_TIMEOUT_US	2000000	/* per-direction batch deadline (2 s; loopback needs µs) */
#define READY_TIMEOUT_US 5000000 /* socket bind/associate deadline (5 s) */
#include	"_ts2/c++/hwMmsgTest_.h"
#include	<string.h>
#include	<stdio.h>

CLASS_TINYSTATE(hw/c++/hwMmsgTest,ts2/c++/tinyState)

/* Test verdict, read by main() for the process exit status (0 = pass). */
int	mmsgtest_failed = 0;

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockUDP.h"
#include	"ts2/c++/stdInterval.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwMmsgTest_(sPtr<tinyState> parent, int portA, int portB);
protected:
	TS_DEFARGS
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
	INTEGER64		deadline;	/* stdInterval::now() when the current phase gives up */
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

/* No tsSignal(SIGPIPE) here: tsApplication installs a process-wide no-op
 * SIGPIPE handler for the whole run.  Owning a second one and destroying it in
 * ACT_CLEANUP left SIGPIPE at SIG_DFL for the teardown that follows, so the
 * THR_KILL(SIGPIPE) burst killed the process (rc=141) in ~2.5% of runs even
 * though the test itself had already passed. */
TS_STATE(INI_START)
{
	udpA = thNEW(ts2IOsockUDP,(ifThis,portA));
	udpB = thNEW(ts2IOsockUDP,(ifThis,portB));
	deadline = stdInterval::now() + READY_TIMEOUT_US;
	return rDO|ACT_WAIT_READY;
}

/* Both sockets bound and past INI (io set + IOCP-associated).
 *
 * This POLLS the two sockets rather than counting their TSE_WAKEUPs.  Counting
 * "exactly 2 wakeups" looks natural but hangs about 1% of runs: when both binds
 * complete almost simultaneously the state function is re-run once, not twice,
 * so `ready` stops at 1 and the test waits forever for a wakeup that has already
 * been and gone.  Polling has no such assumption, and the deadline turns even a
 * socket that never becomes ready into an explicit failure. */
TS_STATE(ACT_WAIT_READY)
{
	if ( C_TEST(udpA->tinyState::state(),C_INI) ||
			C_TEST(udpB->tinyState::state(),C_INI) ) {
		if ( stdInterval::now() >= deadline ) {
			::printf("[mmsgtest] sockets not ready within %d ms"
					" (A state=0x%x, B state=0x%x)\n",
					READY_TIMEOUT_US/1000,
					(unsigned)udpA->tinyState::state(),
					(unsigned)udpB->tinyState::state());
			mmsgtest_failed = 1;
			return rDO|ACT_CLEANUP;
		}
		stdInterval::wait(ifThis,1000,TSE_TIMER);
		return ACT_WAIT_ASSOC_TICK;
	}
	if ( udpA->err != 0 || udpB->err != 0 ) {
		::printf("[mmsgtest] bind failed (A err=%d, B err=%d)\n",udpA->err,udpB->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_PREP_AB;
}
TS_STATE(ACT_WAIT_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_READY;
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
	/* Arm the deadline before the send so a lost datagram cannot hang the recv.
	 * TSE_TIMER2, not TSE_TIMER: the assoc poll above uses TSE_TIMER and `ev`
	 * still carries that event down the rDO chain into the recv state, where a
	 * type-only guard would fire the deadline immediately. */
	deadline = stdInterval::now() + RECV_TIMEOUT_US;
	stdInterval::wait(ifThis,RECV_TIMEOUT_US,TSE_TIMER2);
	return rDO|ACT_SEND_AB;
}
TS_STATE(ACT_SEND_AB)
{
int n = udpA->sendmmsg(&smsg[sent],(unsigned)(NMSG-sent),0);
	if ( n < 0 ) {
		::printf("[mmsgtest] sendmmsg A->B failed (err=%d)\n",udpA->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	sent += n;
	if ( sent < NMSG )
		return rDO|ACT_SEND_AB;
	return rDO|ACT_RECV_AB;
}
TS_STATE(ACT_RECV_AB)
{
	/* Deadline first: recvmmsg yields on EAGAIN and this state is re-run on
	 * whatever event wakes us, so the timer arrives here.  The guard only
	 * exits early — the I/O below stays unconditional (CLAUDE.md rule 5).
	 * The clock is checked as well as the type, so a stale `ev` inherited
	 * through an rDO chain can never be mistaken for our deadline. */
	if ( ev->type == TSE_TIMER2 && stdInterval::now() >= deadline ) {
		::printf("[mmsgtest] A->B batch TIMEOUT — got %d/%d msgs in %d ms"
				" (UDP drop?)\n",rcvd,NMSG,RECV_TIMEOUT_US/1000);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
int n = udpB->recvmmsg(&rmsg[rcvd],(unsigned)(NMSG-rcvd),0,NULL);
	if ( n < 0 ) {
		::printf("[mmsgtest] recvmmsg B failed (err=%d)\n",udpB->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	rcvd += n;
	if ( rcvd < NMSG )
		return rDO|ACT_RECV_AB;
	stdInterval::detach(ifThis);		/* batch complete — disarm */
	if ( !verify_batch(rmsg,rbuf) ) {
		::printf("[mmsgtest] A->B batch verify FAILED\n");
		mmsgtest_failed = 1;
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
	deadline = stdInterval::now() + RECV_TIMEOUT_US;
	stdInterval::wait(ifThis,RECV_TIMEOUT_US,TSE_TIMER2);
	return rDO|ACT_SEND_BA;
}
TS_STATE(ACT_SEND_BA)
{
int n = udpB->sendmmsg(&smsg[sent],(unsigned)(NMSG-sent),0);
	if ( n < 0 ) {
		::printf("[mmsgtest] sendmmsg B->A failed (err=%d)\n",udpB->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	sent += n;
	if ( sent < NMSG )
		return rDO|ACT_SEND_BA;
	return rDO|ACT_RECV_BA;
}
TS_STATE(ACT_RECV_BA)
{
	if ( ev->type == TSE_TIMER2 && stdInterval::now() >= deadline ) {
		::printf("[mmsgtest] B->A batch TIMEOUT — got %d/%d msgs in %d ms"
				" (UDP drop?)\n",rcvd,NMSG,RECV_TIMEOUT_US/1000);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
int n = udpA->recvmmsg(&rmsg[rcvd],(unsigned)(NMSG-rcvd),0,NULL);
	if ( n < 0 ) {
		::printf("[mmsgtest] recvmmsg A failed (err=%d)\n",udpA->err);
		mmsgtest_failed = 1;
		return rDO|ACT_CLEANUP;
	}
	rcvd += n;
	if ( rcvd < NMSG )
		return rDO|ACT_RECV_BA;
	stdInterval::detach(ifThis);
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
	if ( udpA != thNULL )		udpA->destroy();
	if ( udpB != thNULL )		udpB->destroy();
	udpA = thNULL;
	udpB = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
