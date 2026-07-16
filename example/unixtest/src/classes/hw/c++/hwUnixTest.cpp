/*
 * hwUnixTest — AF_UNIX (Unix domain socket) runtime test for the MinGW port
 * (Windows-port design memo).  Windows 10 1803+ / 11 support AF_UNIX via winsock (afunix.h).
 *
 * There is no AF_UNIX *server* tinyState class (only ts2IOunixClient), so this
 * test stands up a raw AF_UNIX listening socket itself (socket/bind/listen +
 * a blocking accept in a TS_THREAD — legit since it is a worker, not the
 * reactor), connects to it with ts2IOunixClient, wraps the accepted end in a
 * ts2IOsocket, and runs PING/PONG so read_c/write_c go over the AF_UNIX stream
 * on both ends (overlapped I/O through IOCP on Windows).
 *
 * The socket path is relative to cwd; both ends are in the same process so it
 * resolves identically.  Stale path is unlinked before bind and after teardown.
 */
#include	"_ts2/c++/hwUnixTest_.h"
#include	<string.h>

#ifdef _WIN32
#include	<winsock2.h>
#include	<afunix.h>
#include	<io.h>			/* _unlink */
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif
#define RAW_BAD		INVALID_SOCKET
#define RAW_CLOSE(s)	closesocket((SOCKET)(s))
#define RAW_UNLINK(p)	_unlink(p)
#define WRAP_FD(s)	((HANDLE)(INT_PTR)(s))
#else
#include	<sys/socket.h>
#include	<sys/un.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<errno.h>
#define RAW_BAD		(-1)
#define RAW_CLOSE(s)	close((int)(s))
#define RAW_UNLINK(p)	unlink(p)
#define WRAP_FD(s)	((int)(s))
#endif

CLASS_TINYSTATE(hw/c++/hwUnixTest,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOunixClient.h"
#include	"ts2/c++/ts2IOsocket.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsSignal.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwUnixTest_(sPtr<tinyState> parent, sPtr<stdString> path);
protected:
	TS_DEFARGS
	sPtr<tinyState>	sig_pipe;
	sPtr<ts2IO>	client;		/* ts2IOunixClient */
	sPtr<ts2IO>	accepted;	/* server end (raw accept wrapped in ts2IOsocket) */
	intptr_t	lsock;		/* listening AF_UNIX socket */
	intptr_t	asock;		/* accepted socket */
	char		wbuf[8];
	char		rbuf[8];
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
TS_END_INTERFACE
#endif

hwUnixTest_::hwUnixTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

TS_STATE(INI_START)
{
#ifndef _WIN32
	sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
#endif
	lsock = asock = RAW_BAD;
	RAW_UNLINK(path->get_str());			/* remove stale socket file */

	lsock = (intptr_t)::socket(AF_LOCAL,SOCK_STREAM,0);
	if ( lsock == (intptr_t)RAW_BAD ) {
		::printf("[unixtest] socket failed\n");
		return rDO|ACT_CLEANUP;
	}
struct sockaddr_un sun;
	memset(&sun,0,sizeof(sun));
	sun.sun_family = AF_LOCAL;
	strcpy(sun.sun_path,path->get_str());
	if ( ::bind((int)lsock,(struct sockaddr*)&sun,sizeof(sun)) < 0 ) {
		::printf("[unixtest] bind failed\n");
		return rDO|ACT_CLEANUP;
	}
	if ( ::listen((int)lsock,1) < 0 ) {
		::printf("[unixtest] listen failed\n");
		return rDO|ACT_CLEANUP;
	}
	/* non-blocking listen socket -> accept() is a poll in a TS_STATE.  A blocking
	   accept in a TS_THREAD *also works* (the worker pool starts at 2 and grows
	   ~1/10ms, so there is no worker-starvation deadlock) but on real Windows it
	   hangs ~5% of the time under load — the blocking accept occasionally misses
	   the connection-ready wake.  The poll re-checks and never misses.  Windows-port design memo */
#ifdef _WIN32
	{ u_long nb = 1; ioctlsocket((SOCKET)lsock,FIONBIO,&nb); }
#else
	fcntl((int)lsock,F_SETFL,fcntl((int)lsock,F_GETFL,0)|O_NONBLOCK);
#endif
	client = thNEW(ts2IOunixClient,(ifThis,path));
	return rDO|ACT_ACCEPT;
}

TS_STATE(ACT_ACCEPT)		/* poll accept (non-blocking) until the client connects */
{
	/* soACCEPT (not raw ::accept) so the fd is registered in sObject's
	   descriptor table — the accepted fd is handed to ts2IOsocket, whose POSIX
	   FIN closes it via __close and would otherwise panic "close not opened
	   fd".  (On Windows soACCEPT's tracking is a bounds-guarded no-op and the
	   descriptor FIN uses CloseHandle, so this is harmless there.)  Windows-port design memo */
	asock = (intptr_t)soACCEPT((int)lsock,(struct sockaddr*)NULL,(socklen_t*)NULL);
	if ( asock != (intptr_t)RAW_BAD )
		return rDO|ACT_WRAP;
#ifdef _WIN32
	if ( WSAGetLastError() == WSAEWOULDBLOCK )
#else
	if ( errno == EAGAIN || errno == EWOULDBLOCK )
#endif
	{
		stdInterval::wait(ifThis,5000,TSE_TIMER);	/* 5ms, retry */
		return ACT_ACCEPT_TICK;
	}
	::printf("[unixtest] accept failed\n");
	return rDO|ACT_CLEANUP;
}
TS_STATE(ACT_ACCEPT_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_ACCEPT;
}
TS_STATE(ACT_WRAP)
{
	if ( asock == (intptr_t)RAW_BAD ) {
		::printf("[unixtest] accept failed\n");
		return rDO|ACT_CLEANUP;
	}
	accepted = thNEW(ts2IOsocket,(ifThis,WRAP_FD(asock)));
	return rDO|ACT_WAIT_CLIENT;
}

TS_STATE(ACT_WAIT_CLIENT)	/* client + accepted past INI (connected + IOCP-associated) */
{
	if ( client == thNULL || C_TEST(client->tinyState::state(),C_ZOM) ) {
		::printf("[unixtest] client connect failed\n");
		return rDO|ACT_CLEANUP;
	}
	if ( C_TEST(client->tinyState::state(),C_INI) ||
			C_TEST(accepted->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,1000,TSE_TIMER);
		return ACT_WAIT_CLIENT_TICK;
	}
	return rDO|ACT_WRITE_PING;
}
TS_STATE(ACT_WAIT_CLIENT_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_WAIT_CLIENT;
}

TS_STATE(ACT_WRITE_PING)	/* client -> server */
{
	memcpy(wbuf,"PING",4);
	client->write_c(wbuf,4);
	return rDO|ACT_READ_PING;
}
TS_STATE(ACT_READ_PING)		/* server reads */
{
	if ( accepted->read_c(rbuf,4) != 4 ) { ::printf("[unixtest] read PING short\n"); return rDO|ACT_CLEANUP; }
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PING",4) != 0 ) { ::printf("[unixtest] BAD PING got '%s'\n",rbuf); return rDO|ACT_CLEANUP; }
	return rDO|ACT_WRITE_PONG;
}
TS_STATE(ACT_WRITE_PONG)	/* server -> client */
{
	memcpy(wbuf,"PONG",4);
	accepted->write_c(wbuf,4);
	return rDO|ACT_READ_PONG;
}
TS_STATE(ACT_READ_PONG)		/* client reads */
{
	if ( client->read_c(rbuf,4) != 4 ) { ::printf("[unixtest] read PONG short\n"); return rDO|ACT_CLEANUP; }
	rbuf[4] = 0;
	if ( memcmp(rbuf,"PONG",4) != 0 ) { ::printf("[unixtest] BAD PONG got '%s'\n",rbuf); return rDO|ACT_CLEANUP; }
	::printf("[unixtest] loopback OK (PING/PONG)\n");
	return rDO|ACT_CLEANUP;
}

TS_STATE(ACT_CLEANUP)
{
	if ( client != thNULL )		client->destroy();
	if ( accepted != thNULL )	accepted->destroy();
	if ( lsock != (intptr_t)RAW_BAD ) { RAW_CLOSE(lsock); lsock = (intptr_t)RAW_BAD; }
	RAW_UNLINK(path->get_str());
	if ( sig_pipe != thNULL )	sig_pipe->destroy();
	client = thNULL;
	accepted = thNULL;
	sig_pipe = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
