

#include	"_ts2/c++/ts2IOsockUDP_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsockUDP,ts2/c++/ts2IOsocket)


#if 0

TS_BEGIN_IMPLEMENT


/**
 * @brief UDP ソケットを管理する ts2IO 実装。/ ts2IO implementation managing a UDP socket.
 * @details
 * `INI_START` (TS_THREAD) で `socket(AF_INET, SOCK_DGRAM)` → `bind()` を実行し、
 * 完了後に `parent->wakeup()` を呼んで親を起こす。
 * バインド後の実際のアドレスは `addr` / `addrlen` メンバで取得できる。
 *
 * `conn` 引数を渡すと `connect()` も実行し、以降 `read()`/`write()` が使える。
 * `conn = NULL` の場合はコネクションレス UDP として `sendto()`/`recvfrom()` を使う。
 * エラー時は `err` / `errpos` を確認する。
 *
 * / Runs `socket()` → `bind()` → optional `connect()` in `INI_START` (TS_THREAD).
 * If `conn` is non-NULL, the socket is connected and `read()`/`write()` work;
 * otherwise use `sendto()`/`recvfrom()` (connectionless UDP).
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief コネクションレス UDP ソケットを作成する。/ Create a connectionless UDP socket.
	 * @param parent 親 tinyState。/ Parent tinyState.
	 * @param port   バインドするポート番号。0 で OS が自動割り当て。/ Port to bind; 0 for OS-assigned.
	 */
	ts2IOsockUDP_(
		sPtr<tinyState> parent,
		int port);
	/** @brief 特定アドレスに connect した UDP ソケットを作成する。/ Create a connected UDP socket.
	 * @param parent   親 tinyState。/ Parent tinyState.
	 * @param port     バインドするポート番号。/ Port to bind.
	 * @param conn     connect(2) に渡す接続先アドレス。/ Destination address for connect(2).
	 * @param conn_len アドレス長。/ Address length.
	 */
	ts2IOsockUDP_(
		sPtr<tinyState> parent,
		int port,
		struct sockaddr * conn,
		int conn_len);

	/** @brief バインド後の実際のソケットアドレス (getsockname で取得)。/ Actual socket address after bind (from getsockname). */
	struct sockaddr addr;
	/** @brief `addr` の長さ。/ Length of `addr`. */
	int addrlen;

	/** @brief 最後のエラーコード (errno 相当)。/ Last error code (errno). */
	int err;
	/** @brief エラーが発生したシステムコール名 ("socket", "bind" 等)。成功時は "OK"。
	 *  / Name of the failing system call ("socket", "bind", etc.); "OK" on success. */
	const char * errpos;
private:
protected:
	TS_DEFARGS
	int sock;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE

#endif


ts2IOsockUDP_::ts2IOsockUDP_(TS_ARGS0)
        : ts2IOsocket_(parent)
{
    TS_CPARGS0
}


ts2IOsockUDP_::ts2IOsockUDP_(TS_ARGS1)
        : ts2IOsocket_(parent)
{
    TS_CPARGS1
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/



/*******************************************
	STATE MACHINE
********************************************/

TS_THREAD(INI_START)
{
  	sock = soSOCKET(AF_INET,SOCK_DGRAM,0);
	if ( sock < 0 ) {
		errpos = "socket";
		err = errno;
		return rDO|ACT_ERROR;
	}

struct sockaddr_in sa;
int yes = 1;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	if ( port == 0 )
		yes = 0;
	if ( setsockopt(sock,
			SOL_SOCKET, SO_REUSEADDR, 
			(const char*)&yes,sizeof(yes)) < 0 ) {

		errpos = "setsockopt";
		err = errno;
		return rDO|ACT_ERROR_PREV;
	}
	if ( bind(sock, 
			(struct sockaddr*)&sa,
			sizeof(struct sockaddr_in)) < 0 ) {
		errpos = "bind";
		err = errno;
		return rDO|ACT_ERROR_PREV;
	}
	addrlen = sizeof(addr);
	if ( getsockname(sock,
			(struct sockaddr*)&addr,
			(socklen_t*)&addrlen) < 0 ) {
		errpos = "getsockname";
		err = errno;
		return rDO|ACT_ERROR_PREV;
	}
	if ( conn ) {
		if ( connect(sock,conn,conn_len) < 0 ) {
			errpos = "connect";
			err = errno;
			return rDO|ACT_ERROR_PREV;
		}
	}

#ifndef _WIN32
int flags;
	flags = fcntl(sock,F_GETFL,0);
	flags |= O_NONBLOCK;
	fcntl(sock,F_SETFL,flags);
	fd = sock;
#else
	fd = (HANDLE)(INT_PTR)sock;	/* overlapped socket; no non-blocking flag */
#endif
	errpos = "OK";
	err = 0;
	parent->wakeup();
	return rDO|INI_FINISH;
}
TS_STATE(INI_FINISH)
{
	return rDO|INI_ts2IOsocket_START;
}

TS_STATE(ACT_ERROR_PREV)
{
	soCLOSE(sock);
	return rDO|ACT_ERROR;
}

TS_STATE(FIN_START)
{
	return rDO|FIN_ts2IOsocket_START;
}
