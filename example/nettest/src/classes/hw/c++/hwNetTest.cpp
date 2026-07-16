/*
 * hwNetTest — real-network (non-loopback) TCP test for the Windows port (Windows-port design memo).
 *
 * Same PING/PONG as srvtest, but the server and client are SEPARATE processes
 * so they can run on different machines over the wire:
 *   - server: ts2IOsockTCPserver binds INADDR_ANY:port and accepts in a loop,
 *     reading "PING" and replying "PONG" on each accepted stream.
 *   - client: ts2IOsockTCPconnect resolves <host> via getaddrinfo, connects,
 *     sends "PING", expects "PONG".
 * This drives Windows AcceptEx (server) / connect (client) + overlapped
 * read_c/write_c across a real NIC rather than 127.0.0.1.
 */
#include	"_ts2/c++/hwNetTest_.h"
#include	<string.h>
#include	<stdio.h>
#ifdef _WIN32
#include	<winsock2.h>
#else
#include	<netinet/in.h>
#endif

CLASS_TINYSTATE(hw/c++/hwNetTest,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockTCPserver.h"
#include	"ts2/c++/ts2IOsockTCPconnect.h"
#include	"ts2/c++/ts2IOsockServer.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsSignal.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwNetTest_(sPtr<tinyState> parent, int is_server, sPtr<stdString> host, int port);
protected:
	TS_DEFARGS
	sPtr<tinyState>		sig_pipe;
	sPtr<ts2IOsockServer>	server;
	sPtr<ts2IO>		client;
	sPtr<ts2IO>		accepted;
	struct sockaddr_in	resolve;
	struct sockaddr		peer;
	char			hostbuf[128];
	char			wbuf[8];
	char			rbuf[8];
	int			errp;
	int			served;
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

hwNetTest_::hwNetTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

TS_STATE(INI_START)
{
#ifndef _WIN32
	sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
#endif
	served = 0;
	if ( is_server ) {
		server = server.d_cast(thNEW(ts2IOsockTCPserver,(ifThis,port)));
		return ACT_SRV_WAIT_READY;
	}
	memset(hostbuf,0,sizeof(hostbuf));
	host->copyout(hostbuf,sizeof(hostbuf));
	client = thNEW(ts2IOsockTCPconnect,(ifThis,&resolve,hostbuf,port));
	return rDO|ACT_CLI_WAIT_CONN;		/* enter now; the poll-timer drives re-entry */
}

/* ---------------- server ---------------- */
TS_STATE(ACT_SRV_WAIT_READY)
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( server->err != 0 ) {
		::printf("[nettest] server setup failed at %s err=%d\n",server->errpos,server->err);
		return rDO|ACT_CLEANUP;
	}
	::printf("[nettest] server listening on port %d (INADDR_ANY)\n",port);
	return rDO|ACT_ACCEPT;
}
TS_STATE(ACT_ACCEPT)
{
	accepted = server->accept(&errp,&peer,ifThis);		/* yields until a connection */
	if ( accepted == thNULL ) {
		::printf("[nettest] accept failed err=%d\n",errp);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_SRV_READ_PING;
}
TS_STATE(ACT_SRV_READ_PING)
{
	if ( accepted->read_c(rbuf,4) != 4 ) {
		::printf("[nettest] srv read PING short\n");
		return rDO|ACT_SRV_DONE;
	}
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PING",4) != 0 ) {
		::printf("[nettest] srv BAD PING '%s'\n",rbuf);
		return rDO|ACT_SRV_DONE;
	}
	return rDO|ACT_SRV_WRITE_PONG;
}
TS_STATE(ACT_SRV_WRITE_PONG)
{
	memcpy(wbuf,"PONG",4);
	accepted->write_c(wbuf,4);
	return rDO|ACT_SRV_DONE;
}
TS_STATE(ACT_SRV_DONE)
{
	served++;
	::printf("[nettest] served #%d\n",served);
	if ( accepted != thNULL )
		accepted->destroy();
	accepted = thNULL;
	return rDO|ACT_ACCEPT;		/* loop for the next client */
}

/* ---------------- client ---------------- */
TS_STATE(ACT_CLI_WAIT_CONN)
{
	if ( client == thNULL || C_TEST(client->tinyState::state(),C_ZOM) ) {
		::printf("[nettest] client connect failed\n");
		return rDO|ACT_CLEANUP;
	}
	if ( C_TEST(client->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,1000,TSE_TIMER);
		return ACT_CLI_WAIT_CONN_TICK;
	}
	return rDO|ACT_CLI_WRITE_PING;
}
TS_STATE(ACT_CLI_WAIT_CONN_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_CLI_WAIT_CONN;
}
TS_STATE(ACT_CLI_WRITE_PING)
{
	memcpy(wbuf,"PING",4);
	client->write_c(wbuf,4);
	return rDO|ACT_CLI_READ_PONG;
}
TS_STATE(ACT_CLI_READ_PONG)
{
	if ( client->read_c(rbuf,4) != 4 ) {
		::printf("[nettest] client read PONG short\n");
		return rDO|ACT_CLEANUP;
	}
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PONG",4) != 0 ) {
		::printf("[nettest] client BAD PONG '%s'\n",rbuf);
		return rDO|ACT_CLEANUP;
	}
	::printf("[nettest] client OK (PING/PONG over net to %s:%d)\n",hostbuf,port);
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
