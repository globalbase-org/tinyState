/*
 * hwUdpTest — UDP datagram runtime test for the MinGW winsock port.
 *
 * Two ROLES exchange a fixed PING/PONG over 127.0.0.1:
 *   role 0 (client)    — owns the socket on `myport`, spawns a responder child on
 *                        `peerport`, waits a warm-up beat, then sendto PING /
 *                        recvfrom PONG and reports the result.
 *   role 1 (responder) — owns the socket on `myport`, loops recvfrom → sendto,
 *                        replying to the source address recvfrom gave it.
 *
 * WHY two tinyStates (was one before): the ENHANCED (RIO) datagram path is
 * single-posted in Phase A and RIO delivers ONLY into an already-posted receive —
 * a datagram arriving with no posted RIOReceiveEx is dropped (no kernel receive
 * buffer, unlike plain WSARecvFrom).  A single state machine that sends then
 * receives would transmit PING before its own recv is posted → RIO drops it.
 * Splitting sender/receiver lets the RESPONDER park its recvfrom (which posts the
 * RIO receive) BEFORE the client sends.  Plain (non-ENHANCED) sockets work either
 * way — the kernel buffers — so this restructure is transparent to them.
 *
 * The recvfrom source-address buffer (`from`/`flen`) is a MEMBER so it survives
 * the yield across the overlapped/RIO op (the addr-lifetime contract).  Each
 * sendto/recvfrom is its own ev-independent state (1 datagram op per state).
 */
#include	"_ts2/c++/hwUdpTest_.h"
#include	<string.h>

CLASS_TINYSTATE(hw/c++/hwUdpTest,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockUDP.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsSignal.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief client (role 0): bind myport, spawn a responder on peerport, PING→PONG.
	 *  drop!=0 = drop-demo: the responder DELAYS its first recvfrom (socket bound but
	 *  no RIO receive posted) while the client sends into that window — plain UDP is
	 *  saved by the kernel buffer, ENHANCED (RIO) drops the datagram (no posted recv)
	 *  and the exchange hangs.  Demonstrates the single-posted RIO window. */
	hwUdpTest_(sPtr<tinyState> parent, int myport, int peerport, int mode, int drop);
	/** @brief role-explicit ctor.  role 0 = client, role 1 = responder (bind myport,
	 *  loop recvfrom→sendto replying to the source).  See file header for why the
	 *  sender and receiver are separate tinyStates (RIO single-posted receive). */
	hwUdpTest_(sPtr<tinyState> parent, int myport, int peerport, int mode, int role, int drop);
protected:
	TS_DEFARGS
	sPtr<tinyState>		sig_pipe;
	sPtr<tinyState>		responder;	/* client: the child responder */
	sPtr<ts2IOsockUDP>	udp;		/* this role's socket (bound to myport) */
	struct sockaddr_in	dest;		/* client: 127.0.0.1:peerport */
	struct sockaddr		from;		/* recvfrom source (must persist over yield) */
	int			flen;
	char			wbuf[8];
	char			rbuf[8];
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IOsockUDP;
TS_END_INTERFACE
#endif

hwUdpTest_::hwUdpTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }	/* client (role 0) */
hwUdpTest_::hwUdpTest_(TS_ARGS1) : tinyState_(parent) { TS_CPARGS1 }	/* role-explicit */


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	udp = thNEW(ts2IOsockUDP,(ifThis,myport));	/* RIO auto (plain via TS2_DISABLE_RIO); `mode` now ignored */
	if ( role != 1 ) {
		/* client: SIGPIPE guard + spawn the responder on peerport */
#ifndef _WIN32
		sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
#endif
		responder = thNEW(hwUdpTest,(ifThis,peerport,myport,mode,1,drop));
	}
	return ACT_WAIT_READY;
}

/* Both roles: wait for our own socket to bind (its INI wakes us) and reach the
   active state (past C_INI = io set + associated) before any I/O. */
TS_STATE(ACT_WAIT_READY)
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( udp->err != 0 ) {
		::printf("[udptest] bind failed (err=%d)\n",udp->err);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_WAIT_ASSOC;
}
TS_STATE(ACT_WAIT_ASSOC)
{
	if ( C_TEST(udp->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,50,TSE_TIMER);
		return ACT_WAIT_ASSOC_TICK;
	}
	if ( role != 1 )
		return rDO|ACT_CLI_WARMUP;
	/* responder: drop-demo delays posting the first recv so the client's PING lands
	   with no RIO receive posted (dropped on ENHANCED, kernel-buffered on plain). */
	return drop ? (rDO|ACT_RESP_DELAY) : (rDO|ACT_RESP_RECV);
}
TS_STATE(ACT_WAIT_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_ASSOC;
}


/* ---- responder (role 1): recvfrom → reply to source, forever until destroyed ---- */

TS_STATE(ACT_RESP_DELAY)		/* drop-demo: bound but recv NOT posted for 1.5s */
{
	stdInterval::wait(ifThis,1500,TSE_TIMER);
	return ACT_RESP_DELAY_TICK;
}
TS_STATE(ACT_RESP_DELAY_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_RESP_RECV;
}

TS_STATE(ACT_RESP_RECV)
{
	if ( is_destroyed() )
		return rDO|ACT_CLEANUP;
	flen = sizeof(from);
	{
	int n = udp->recvfrom(0,rbuf,(int)sizeof(rbuf),0,&from,&flen);	/* parks until a datagram lands */
		if ( n < 0 )
			return rDO|ACT_CLEANUP;			/* socket torn down (EBADF) */
	}
	return rDO|ACT_RESP_REPLY;
}
TS_STATE(ACT_RESP_REPLY)
{
	udp->sendto(rbuf,4,0,&from,flen);			/* echo back to the source */
	return rDO|ACT_RESP_RECV;
}


/* ---- client (role 0): warm up, send PING, receive PONG ---- */

TS_STATE(ACT_CLI_WARMUP)		/* let the responder bind + park its recv (post it) */
{
	stdInterval::wait(ifThis,300,TSE_TIMER);
	return ACT_CLI_WARMUP_TICK;
}
TS_STATE(ACT_CLI_WARMUP_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_CLI_SEND;
}
TS_STATE(ACT_CLI_SEND)			/* -> responder */
{
	memset(&dest,0,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(peerport);
	dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	memcpy(wbuf,"PING",4);
	udp->sendto(wbuf,4,0,(struct sockaddr*)&dest,sizeof(dest));
	return rDO|ACT_CLI_RECV;
}
TS_STATE(ACT_CLI_RECV)			/* PONG back from the responder */
{
	flen = sizeof(from);
	if ( udp->recvfrom(0,rbuf,4,0,&from,&flen) != 4 ) {
		::printf("[udptest] recv PONG short\n");
		return rDO|ACT_CLEANUP;
	}
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PING",4) != 0 ) {			/* responder echoes our PING payload */
		::printf("[udptest] BAD reply got '%s'\n",rbuf);
		return rDO|ACT_CLEANUP;
	}
	::printf("[udptest] loopback OK (PING/PONG)\n");
	return rDO|ACT_CLEANUP;
}


/* ---- shared cleanup ---- */

TS_STATE(ACT_CLEANUP)
{
	if ( responder != thNULL )	responder->destroy();
	if ( udp != thNULL )		udp->destroy();
	if ( sig_pipe != thNULL )	sig_pipe->destroy();
	responder = thNULL;
	udp = thNULL;
	sig_pipe = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
