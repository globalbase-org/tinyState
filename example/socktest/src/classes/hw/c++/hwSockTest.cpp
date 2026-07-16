/*
 * hwSockTest — TCP loopback runtime test for the MinGW winsock port (Windows-port design memo).
 *
 * mode 0 (pingpong, default): server + immediate connect + accept + bidirectional
 *   PING/PONG (write_c/read_c on both the connected and the accepted socket).
 *   Used for the stress run.
 * mode 1 (reactor): the connect is DELAYED and driven by a separate timer object
 *   (hwSockPoke) that ticks a few times first.  While the main test is parked in
 *   accept(), those ticks fire — proving the reactor is NOT frozen (overlapped
 *   AcceptEx yields; a blocking accept would freeze the reactor and hang).
 * mode 2 (teardown): a helper destroys the server while accept is pending,
 *   exercising the CancelIoEx cancel path; accept must return cleanly (thNULL).
 *
 * Race note (mode 0): accept() can return before the client (conn) finished its
 * own INI (IOCP association); poll conn past C_INI before doing I/O on it.  Windows-port design memo.
 */
#include	"_ts2/c++/hwSockTest_.h"
#include	<string.h>

CLASS_TINYSTATE(hw/c++/hwSockTest,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockTCPserver.h"
#include	"ts2/c++/ts2IOsockTCPconnect.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsSignal.h"
#include	"hw/c++/hwSockPoke.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwSockTest_(sPtr<tinyState> parent, int port, int mode);
protected:
	TS_DEFARGS
	sPtr<tinyState>			sig_pipe;
	sPtr<tinyState>			sig_test;
	sPtr<ts2IOsockTCPserver>	server;
	sPtr<ts2IO>			conn;		/* client end (mode 0) */
	sPtr<ts2IO>			accepted;	/* server end */
	struct sockaddr_in		resolve;
	struct sockaddr			peer;
	char				wbuf[8];
	char				rbuf[8];
	int				errp;
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
class ts2IOsockTCPserver;
TS_END_INTERFACE
#endif

hwSockTest_::hwSockTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

TS_STATE(INI_START)
{
	/* mask SIGPIPE so a broken-pipe write during socket teardown does not kill
	   the process.  POSIX only — SIGPIPE is a POSIX concept with no Windows
	   analogue, so nothing to do there.  Windows-port design memo */
#ifndef _WIN32
	sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
#endif
	if ( mode == 3 ) {	/* isolated tsSignal create+destroy (tsSignalCore teardown) */
		sig_test = thNEW(tsSignal,(ifThis,SIGINT));
		stdInterval::wait(ifThis,100*1000,TSE_TIMER);
		return ACT_SIGTEST;
	}
	if ( mode == 4 )	/* pingpong WITH an active SIGINT handler (tsSignal + sockets) */
		sig_test = thNEW(tsSignal,(ifThis,SIGINT));
	server = thNEW(ts2IOsockTCPserver,(ifThis,port));
	return ACT_WAIT_READY;
}

TS_STATE(ACT_SIGTEST)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	::printf("[socktest] signal teardown: destroying tsSignal(SIGINT)\n");
	if ( sig_test != thNULL )
		sig_test->destroy();
	sig_test = thNULL;
	::printf("[socktest] signal teardown OK\n");
	return rDO|ACT_CLEANUP;
}

TS_STATE(ACT_WAIT_READY)
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( server->err != 0 ) {
		::printf("[socktest] server setup failed at %s err=%d\n",
			server->errpos,server->err);
		return rDO|FIN_START;
	}
	if ( mode == 0 || mode == 4 )
		conn = thNEW(ts2IOsockTCPconnect,(ifThis,&resolve,"127.0.0.1",port));
	else if ( mode == 1 )				/* delayed connect via poker */
		thNEW(hwSockPoke,(ifThis,0,port,thNULL));
	else						/* teardown: destroy server while accept pending */
		thNEW(hwSockPoke,(ifThis,1,port,server));
	return rDO|ACT_ACCEPT;
}

TS_STATE(ACT_ACCEPT)
{
	accepted = server->accept(&errp,&peer,ifThis);	/* yields (AcceptEx pending) */
	if ( accepted == thNULL ) {
		if ( mode == 2 ) {
			::printf("[socktest] teardown OK: accept cancelled by destroy (err=%d)\n",errp);
			return rDO|ACT_CLEANUP;
		}
		::printf("[socktest] accept failed err=%d\n",errp);
		return rDO|ACT_CLEANUP;
	}
	if ( mode == 1 ) {
		::printf("[socktest] reactor OK: accepted after delayed connect (reactor stayed live)\n");
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_WAIT_CONN;
}

TS_STATE(ACT_WAIT_CONN)
{
	if ( conn == thNULL || C_TEST(conn->tinyState::state(),C_ZOM) ) {
		::printf("[socktest] connect died before ready\n");
		return rDO|ACT_CLEANUP;
	}
	if ( !C_TEST(conn->tinyState::state(),C_INI) )
		return rDO|ACT_WRITE_PING;
	stdInterval::wait(ifThis,1000,TSE_TIMER);
	return ACT_WAIT_CONN_TICK;
}
TS_STATE(ACT_WAIT_CONN_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_CONN;
}

TS_STATE(ACT_WRITE_PING)		/* client -> server */
{
	memcpy(wbuf,"PING",4);
	conn->write_c(wbuf,4);
	return rDO|ACT_READ_PING;
}
TS_STATE(ACT_READ_PING)			/* server reads */
{
	if ( accepted->read_c(rbuf,4) != 4 ) {
		::printf("[socktest] read PING short\n");
		return rDO|ACT_CLEANUP;
	}
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PING",4) != 0 ) {
		::printf("[socktest] BAD PING got '%s'\n",rbuf);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_WRITE_PONG;
}
TS_STATE(ACT_WRITE_PONG)		/* server -> client */
{
	memcpy(wbuf,"PONG",4);
	accepted->write_c(wbuf,4);
	return rDO|ACT_READ_PONG;
}
TS_STATE(ACT_READ_PONG)			/* client reads */
{
	if ( conn->read_c(rbuf,4) != 4 ) {
		::printf("[socktest] read PONG short\n");
		return rDO|ACT_CLEANUP;
	}
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PONG",4) != 0 ) {
		::printf("[socktest] BAD PONG got '%s'\n",rbuf);
		return rDO|ACT_CLEANUP;
	}
	::printf("[socktest] loopback OK (PING/PONG)\n");
	return rDO|ACT_CLEANUP;
}

TS_STATE(ACT_CLEANUP)
{
	if ( conn != thNULL )		conn->destroy();
	if ( accepted != thNULL )	accepted->destroy();
	if ( server != thNULL )		server->destroy();
	if ( sig_pipe != thNULL )	sig_pipe->destroy();	/* release the SIGPIPE
					   handler so the reactor can drain and exit */
	if ( sig_test != thNULL )	sig_test->destroy();
	conn = thNULL;
	accepted = thNULL;
	server = thNULL;
	sig_pipe = thNULL;
	sig_test = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
