/*
 * hwSrvTest — exercises the refactored server hierarchy (Windows-port design memo): one test that
 * runs either a ts2IOsockTCPserver (AF_INET) or a ts2IOsockUnixServer (AF_UNIX),
 * both thin subclasses of the shared ts2IOsockServer base (overlapped AcceptEx
 * on Windows / non-blocking accept on POSIX).  The matching client is a
 * ts2IOsockTCPconnect or a ts2IOunixClient.  PING/PONG both ways verifies
 * accept() + read_c/write_c over the accepted stream on each transport.
 *
 * mode 0 = tcp (port), mode 1 = unix (socket path).
 */
#include	"_ts2/c++/hwSrvTest_.h"
#include	<string.h>
#ifdef _WIN32
#include	<winsock2.h>
#else
#include	<netinet/in.h>
#endif

CLASS_TINYSTATE(hw/c++/hwSrvTest,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockTCPserver.h"
#include	"ts2/c++/ts2IOsockUnixServer.h"
#include	"ts2/c++/ts2IOsockTCPconnect.h"
#include	"ts2/c++/ts2IOunixClient.h"
#include	"ts2/c++/ts2IOsockServer.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsSignal.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwSrvTest_(sPtr<tinyState> parent, int mode, int port, sPtr<stdString> path);
protected:
	TS_DEFARGS
	sPtr<tinyState>		sig_pipe;
	sPtr<ts2IOsockServer>	server;
	sPtr<ts2IO>		client;
	sPtr<ts2IO>		accepted;
	struct sockaddr_in	resolve;
	struct sockaddr		peer;
	char			wbuf[8];
	char			rbuf[8];
	int			errp;
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
class ts2IOsockServer;
class stdString;
TS_END_INTERFACE
#endif

hwSrvTest_::hwSrvTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

TS_STATE(INI_START)
{
#ifndef _WIN32
	sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
#endif
	if ( mode == 0 )
		server = server.d_cast(thNEW(ts2IOsockTCPserver,(ifThis,port)));
	else
		server = server.d_cast(thNEW(ts2IOsockUnixServer,(ifThis,path)));
	return ACT_WAIT_READY;
}

TS_STATE(ACT_WAIT_READY)
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( server->err != 0 ) {
		::printf("[srvtest] server setup failed at %s err=%d\n",server->errpos,server->err);
		return rDO|ACT_CLEANUP;
	}
	if ( mode == 0 )
		client = thNEW(ts2IOsockTCPconnect,(ifThis,&resolve,"127.0.0.1",port));
	else
		client = thNEW(ts2IOunixClient,(ifThis,path));
	return rDO|ACT_ACCEPT;
}

TS_STATE(ACT_ACCEPT)
{
	accepted = server->accept(&errp,&peer,ifThis);		/* yields until connect */
	if ( accepted == thNULL ) {
		::printf("[srvtest] accept failed err=%d\n",errp);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_WAIT_CONN;
}

TS_STATE(ACT_WAIT_CONN)		/* client past INI (connected + IOCP-associated) */
{
	if ( client == thNULL || C_TEST(client->tinyState::state(),C_ZOM) ) {
		::printf("[srvtest] client connect failed\n");
		return rDO|ACT_CLEANUP;
	}
	if ( C_TEST(client->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,1000,TSE_TIMER);
		return ACT_WAIT_CONN_TICK;
	}
	return rDO|ACT_WRITE_PING;
}
TS_STATE(ACT_WAIT_CONN_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_CONN;
}

TS_STATE(ACT_WRITE_PING)
{
	memcpy(wbuf,"PING",4);
	client->write_c(wbuf,4);
	return rDO|ACT_READ_PING;
}
TS_STATE(ACT_READ_PING)
{
	if ( accepted->read_c(rbuf,4) != 4 ) { ::printf("[srvtest] read PING short\n"); return rDO|ACT_CLEANUP; }
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PING",4) != 0 ) { ::printf("[srvtest] BAD PING got '%s'\n",rbuf); return rDO|ACT_CLEANUP; }
	return rDO|ACT_WRITE_PONG;
}
TS_STATE(ACT_WRITE_PONG)
{
	memcpy(wbuf,"PONG",4);
	accepted->write_c(wbuf,4);
	return rDO|ACT_READ_PONG;
}
TS_STATE(ACT_READ_PONG)
{
	if ( client->read_c(rbuf,4) != 4 ) { ::printf("[srvtest] read PONG short\n"); return rDO|ACT_CLEANUP; }
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PONG",4) != 0 ) { ::printf("[srvtest] BAD PONG got '%s'\n",rbuf); return rDO|ACT_CLEANUP; }
	::printf("[srvtest] %s loopback OK (PING/PONG)\n", mode == 0 ? "tcp" : "unix");
	return rDO|ACT_CLEANUP;
}

TS_STATE(ACT_CLEANUP)
{
	if ( client != thNULL )		client->destroy();
	if ( accepted != thNULL )	accepted->destroy();
	if ( server != thNULL )		server->destroy();
	if ( sig_pipe != thNULL )	sig_pipe->destroy();
	client = thNULL;
	accepted = thNULL;
	server = thNULL;
	sig_pipe = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
