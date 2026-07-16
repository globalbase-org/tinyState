/*
 * hwUdpTest — UDP datagram runtime test for the MinGW winsock port (Windows-port design memo).
 *
 * Two ts2IOsockUDP sockets exchange a fixed PING/PONG over 127.0.0.1, so both
 * sendto and recvfrom run on the MinGW overlapped path (WSASendTo/WSARecvFrom
 * driven through the same IOCP as the stream I/O).  The recvfrom source-address
 * buffers (fromA/fromB) are MEMBERS so they survive the yield across the
 * overlapped op (the addr-lifetime contract, Windows-port design memo).
 *
 * Each sendto/recvfrom is its own ev-independent state (1 datagram op per state,
 * member buffers) — the yield/resume re-runs the state and the *_pending check
 * resumes the same op, exactly like read_c/write_c.
 *
 * ts2IOsockUDP binds INADDR_ANY, so getsockname's addr is 0.0.0.0:port; the
 * destination is built as 127.0.0.1:port explicitly.  I/O only starts once each
 * socket is past INI (io set + IOCP-associated), polled via C_INI.
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
	hwUdpTest_(sPtr<tinyState> parent, int portA, int portB);
protected:
	TS_DEFARGS
	sPtr<tinyState>		sig_pipe;
	sPtr<ts2IOsockUDP>	udpA;
	sPtr<ts2IOsockUDP>	udpB;
	struct sockaddr_in	destB;		/* 127.0.0.1:portB */
	struct sockaddr		fromA;		/* recvfrom source (must persist over yield) */
	struct sockaddr		fromB;
	int			flenA;
	int			flenB;
	char			wbuf[8];
	char			rbuf[8];
	int			ready;
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IOsockUDP;
TS_END_INTERFACE
#endif

hwUdpTest_::hwUdpTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

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
		::printf("[udptest] bind failed (A err=%d, B err=%d)\n",udpA->err,udpB->err);
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
	return rDO|ACT_SEND_PING;
}
TS_STATE(ACT_WAIT_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_ASSOC;
}

TS_STATE(ACT_SEND_PING)			/* A -> B */
{
	memset(&destB,0,sizeof(destB));
	destB.sin_family = AF_INET;
	destB.sin_port = htons(portB);
	destB.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	memcpy(wbuf,"PING",4);
	udpA->sendto(wbuf,4,0,(struct sockaddr*)&destB,sizeof(destB));
	return rDO|ACT_RECV_PING;
}
TS_STATE(ACT_RECV_PING)			/* B receives */
{
	flenB = sizeof(fromB);
	if ( udpB->recvfrom(0,rbuf,4,0,&fromB,&flenB) != 4 ) {
		::printf("[udptest] recv PING short\n");
		return rDO|ACT_CLEANUP;
	}
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PING",4) != 0 ) {
		::printf("[udptest] BAD PING got '%s'\n",rbuf);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_SEND_PONG;
}
TS_STATE(ACT_SEND_PONG)			/* B -> A (reply to the source addr recvfrom gave us) */
{
	memcpy(wbuf,"PONG",4);
	udpB->sendto(wbuf,4,0,&fromB,flenB);
	return rDO|ACT_RECV_PONG;
}
TS_STATE(ACT_RECV_PONG)			/* A receives */
{
	flenA = sizeof(fromA);
	if ( udpA->recvfrom(0,rbuf,4,0,&fromA,&flenA) != 4 ) {
		::printf("[udptest] recv PONG short\n");
		return rDO|ACT_CLEANUP;
	}
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PONG",4) != 0 ) {
		::printf("[udptest] BAD PONG got '%s'\n",rbuf);
		return rDO|ACT_CLEANUP;
	}
	::printf("[udptest] loopback OK (PING/PONG)\n");
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
