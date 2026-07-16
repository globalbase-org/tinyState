/*
 * ts2IOsockServer — MinGW (overlapped AcceptEx via IOCP) stream-server base.
 * Windows-port design memo.
 *
 * All the address-family-independent server machinery lives here: socket /
 * listen / IOCP association / overlapped AcceptEx / the accept() yield-resume /
 * CancelIoEx teardown.  The address family and the endpoint (a TCP port vs a
 * filesystem path) are supplied by the subclass through virtual hooks:
 *   sock_family()  -> AF_INET / AF_UNIX
 *   build_addr()   -> fill the bind sockaddr, return its length
 *   backlog()      -> listen() backlog
 *   pre_bind()     -> before bind (AF_UNIX unlinks a stale socket file)
 *   post_close()   -> after the socket is closed (AF_UNIX unlinks the file)
 * so ts2IOsockTCPserver (AF_INET) and ts2IOsockUnixServer (AF_UNIX) are thin
 * subclasses that share this whole overlapped-accept path.  See the design note
 * on the POSIX/MinGW split in ts2IOsockTCPserver / ts2IOsockUnixServer.
 */

#include	<winsock2.h>
#include	<ws2tcpip.h>
#include	<mswsock.h>	/* NOTE: mswsock.h and afunix.h cannot coexist in one TU;
				   the base needs mswsock (AcceptEx) and does NOT need
				   afunix — sock_family() just returns an int.  Windows-port design memo */
#include	<windows.h>
#include	"_ts2/c++/ts2IOsockServer_.h"
#include	"ts2/c++/ts2IOdescriptor.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsockServer,ts2/c++/tinyState)

#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief AF 非依存のストリームサーバ基底 (MinGW, overlapped AcceptEx)。
 * @details socket/listen/accept/IOCP/teardown を持ち、AF 固有は virtual hook で供給。
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2IOsockServer_(sPtr<tinyState> parent);

	sPtr<ts2IO> accept(int * errp,struct sockaddr * peer,sPtr<tinyState>caller);

	int err;
	const char * errpos;

	/* AF-specific hooks (overridden by ts2IOsockTCPserver / ts2IOsockUnixServer) */
	virtual int  sock_family();
	virtual int  build_addr(struct sockaddr_storage * sa);
	virtual int  backlog();
	virtual int  reuse_addr();
	virtual void pre_bind();
	virtual void post_close();
protected:
	TS_DEFARGS

	SOCKET		sock;		/* listen socket */
	sPtr<fwIO>	io;
	int		fdid;		/* IOCP completion key / fwIO registration key */
	SOCKET		acceptSock;	/* pre-created socket for the in-flight AcceptEx */
	OVERLAPPED	accept_ov;
	int		accept_pending;
	void *		pAcceptEx;	/* LPFN_ACCEPTEX; kept as void* so the generated
					   implement header doesn't need mswsock.h (which the
					   framework's WIN32_LEAN_AND_MEAN includes.h excludes). */
	char		addrBuf[2*(sizeof(struct sockaddr_storage)+16)];
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
TS_END_INTERFACE

#endif


ts2IOsockServer_::ts2IOsockServer_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
	sock = INVALID_SOCKET;
	acceptSock = INVALID_SOCKET;
	fdid = ts2io_alloc_key();
	accept_pending = 0;
	pAcceptEx = NULL;
}

/* default hooks (a bare ts2IOsockServer is never instantiated; subclasses override) */
int  ts2IOsockServer_::sock_family()				{ return AF_INET; }
int  ts2IOsockServer_::build_addr(struct sockaddr_storage *)	{ return 0; }
int  ts2IOsockServer_::backlog()				{ return 10; }
int  ts2IOsockServer_::reuse_addr()				{ return 0; }
void ts2IOsockServer_::pre_bind()				{ }
void ts2IOsockServer_::post_close()				{ }


/*******************************************
	INSTANCE FUNCTIONS
********************************************/

sPtr<ts2IO>
ts2IOsockServer_::accept(int * errp,struct sockaddr * peer,sPtr<tinyState> caller)
{
	/* AF_UNIX: a plain non-blocking accept polled via a 5ms timer — NOT AcceptEx.
	   On Windows, AcceptEx on an AF_UNIX socket never delivers its IOCP completion
	   (the op finishes but no packet fires) AND the socket it produces inherits
	   the same broken behaviour, so overlapped read/write on the accepted socket
	   also hangs.  A regular accept() yields a fully working socket (verified:
	   200/200 vs AcceptEx's ~40% hang).  The listen socket is set non-blocking in
	   INI.  Windows-port design memo */
	if ( sock_family() != AF_INET ) {
	SOCKET a = ::accept(sock,NULL,NULL);
		if ( a != INVALID_SOCKET ) {
			(void)peer;
			if ( errp )
				*errp = 0;
			return thNEW(ts2IOdescriptor,(caller,(HANDLE)(INT_PTR)a));
		}
	int e = WSAGetLastError();
		if ( e == WSAEWOULDBLOCK ) {
			if ( is_destroyed() ) {
				if ( errp )
					*errp = e;
				return thNULL;
			}
			io->wait(sCallSection::key->caller(),5000,TSE_TIMER);	/* 5ms poll */
			throw sException([this](sPtr<tinyState> c){ return !io->is_wait(c); });
		}
		if ( errp )
			*errp = e;
		return thNULL;
	}

	/* resume: the AcceptEx we posted has completed (or been cancelled). */
	if ( accept_pending ) {
	DWORD got = 0;
		if ( GetOverlappedResult((HANDLE)sock,&accept_ov,&got,FALSE) ) {
			accept_pending = 0;
			setsockopt(acceptSock,SOL_SOCKET,SO_UPDATE_ACCEPT_CONTEXT,
				(const char*)&sock,sizeof(sock));
			if ( peer ) {
			socklen_t plen = sizeof(struct sockaddr_storage);
				getpeername(acceptSock,peer,&plen);
			}
		SOCKET a = acceptSock;
			acceptSock = INVALID_SOCKET;
			if ( errp )
				*errp = 0;
			return thNEW(ts2IOdescriptor,(caller,(HANDLE)(INT_PTR)a));
		}
	DWORD e = GetLastError();
		/* still pending -> keep waiting; but if torn down, the wake came from
		   FIN's io->abort (no real completion) and CancelIoEx may not be done
		   synchronously on real Windows -> return the abort, don't re-yield. */
		if ( e == ERROR_IO_INCOMPLETE && !is_destroyed() && sock != INVALID_SOCKET ) {
			io->read(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> c){ return !io->is_read(c); });
		}
		accept_pending = 0;
		if ( acceptSock != INVALID_SOCKET ) {
			closesocket(acceptSock);
			acceptSock = INVALID_SOCKET;
		}
		if ( errp )
			*errp = (int)e;
		return thNULL;
	}

	/* first call: post one overlapped AcceptEx (destroy-check + post are atomic
	   under app-mtx since both are non-blocking). */
	if ( is_destroyed() ) {
		if ( errp )
			*errp = WSAENOTSOCK;
		return thNULL;
	}
	acceptSock = socket(sock_family(),SOCK_STREAM,0);
	if ( acceptSock == INVALID_SOCKET ) {
		if ( errp )
			*errp = WSAGetLastError();
		return thNULL;
	}
	memset(&accept_ov,0,sizeof(accept_ov));
	{
	DWORD got = 0;
	DWORD alen = sizeof(struct sockaddr_storage) + 16;
	BOOL _axok = ((LPFN_ACCEPTEX)pAcceptEx)(sock,acceptSock,addrBuf,0,alen,alen,&got,&accept_ov);
		if ( _axok || WSAGetLastError() == ERROR_IO_PENDING ) {
			accept_pending = 1;
			io->read(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> c){ return !io->is_read(c); });
		}
	int e = WSAGetLastError();
		closesocket(acceptSock);
		acceptSock = INVALID_SOCKET;
		if ( errp )
			*errp = e;
		return thNULL;
	}
}


/*******************************************
	STATE MACHINE
********************************************/

TS_THREAD(INI_ts2IOsockServer_START)
{
	sock = socket(sock_family(),SOCK_STREAM,0);
	if ( sock == INVALID_SOCKET ) {
		errpos = "socket";
		err = WSAGetLastError();
		return rDO|FIN_ERROR;
	}
	if ( reuse_addr() ) {		/* TCP only — SO_REUSEADDR is WSAEOPNOTSUPP on AF_UNIX */
	BOOL yes = TRUE;
		setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&yes,sizeof(yes));
	}
	pre_bind();
struct sockaddr_storage sa;
int alen = build_addr(&sa);
	if ( alen <= 0 || bind(sock,(struct sockaddr*)&sa,alen) == SOCKET_ERROR ) {
		errpos = "bind";
		err = WSAGetLastError();
		return rDO|FIN_ERROR;
	}
	if ( ::listen(sock,backlog()) == SOCKET_ERROR ) {
		errpos = "listen";
		err = WSAGetLastError();
		return rDO|FIN_ERROR;
	}

	io = io.d_cast(application->fw());
	if ( sock_family() == AF_INET ) {
		/* TCP: overlapped AcceptEx via IOCP. */
		if ( io.is_notNull() )
			CreateIoCompletionPort((HANDLE)sock,(HANDLE)io->port(),(ULONG_PTR)fdid,0);
	GUID g = WSAID_ACCEPTEX;
	DWORD nb = 0;
		WSAIoctl(sock,SIO_GET_EXTENSION_FUNCTION_POINTER,&g,sizeof(g),
			&pAcceptEx,sizeof(pAcceptEx),&nb,NULL,NULL);
	}
	else {
		/* AF_UNIX: non-blocking listen socket, accept() is polled (no AcceptEx). */
	u_long nb = 1;
		ioctlsocket(sock,FIONBIO,&nb);
	}
	errpos = "OK";
	err = 0;
	parent->wakeup();
	return rDO|ACT_START;
}

TS_STATE(FIN_ERROR)
{
	parent->wakeup();
	return rDO|FIN_ERROR_WAIT;
}
TS_STATE(FIN_ERROR_WAIT)
{
	if ( is_destroyed() )
		return rDO|FIN_START;
	return 0;
}


TS_STATE(FIN_ts2IOsockServer_START)
{
	if ( accept_pending && sock != INVALID_SOCKET )
		CancelIoEx((HANDLE)sock,&accept_ov);
	if ( io.is_notNull() )
		io->abort(fdid);
	return rDO|FIN_CLOSE;
}
TS_THREAD(FIN_CLOSE)
{
	if ( acceptSock != INVALID_SOCKET ) {
		closesocket(acceptSock);
		acceptSock = INVALID_SOCKET;
	}
	if ( sock != INVALID_SOCKET ) {
		closesocket(sock);
		sock = INVALID_SOCKET;
	}
	post_close();
	io = thNULL;
	return rDO|FIN_TINYSTATE_START;
}
