/*
 * ts2IOsockTCPserver — AF_INET stream server.  Windows-port design memo.
 *
 * A thin subclass of ts2IOsockServer: it supplies only the address family and
 * the bind address (a TCP port on INADDR_ANY).  All the socket/listen/accept
 * machinery — non-blocking accept + fwIO(select) on POSIX, overlapped AcceptEx
 * on Windows — lives in ts2IOsockServer (arch-split base).  ts2IOsockUnixServer
 * is the AF_UNIX sibling.
 */
#ifdef _WIN32
#include	<winsock2.h>
#include	<ws2tcpip.h>
#else
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<arpa/inet.h>
#endif
#include	<string.h>
#include	"_ts2/c++/ts2IOsockTCPserver_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsockTCPserver,ts2/c++/ts2IOsockServer)

#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief TCP (AF_INET) サーバ。/ TCP (AF_INET) server.
 * @details ts2IOsockServer の薄いサブクラス。AF と bind アドレス (INADDR_ANY:port) のみ供給。
 * accept() 等は基底を継承。@code server = thNEW(ts2IOsockTCPserver,(ifThis, 8080)); @endcode
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @param port 待ち受けポート。@param p_listen listen backlog (既定10)。 */
	ts2IOsockTCPserver_(
		sPtr<tinyState> parent,
		int port,
		int p_listen=10);

	virtual int sock_family();
	virtual int build_addr(struct sockaddr_storage * sa);
	virtual int backlog();
	virtual int reuse_addr();
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
TS_END_INTERFACE

#endif


ts2IOsockTCPserver_::ts2IOsockTCPserver_(TS_ARGS0)
        : ts2IOsockServer_(parent)
{
    TS_CPARGS0
}

int
ts2IOsockTCPserver_::sock_family()
{
	return AF_INET;
}
int
ts2IOsockTCPserver_::backlog()
{
	return p_listen;
}
int
ts2IOsockTCPserver_::reuse_addr()
{
	return 1;
}
int
ts2IOsockTCPserver_::build_addr(struct sockaddr_storage * sa)
{
struct sockaddr_in * p = (struct sockaddr_in*)sa;
	memset(p,0,sizeof(*p));
	p->sin_family = AF_INET;
	p->sin_port = htons(port);
	p->sin_addr.s_addr = htonl(INADDR_ANY);
	return sizeof(struct sockaddr_in);
}


/* INI/FIN chain into the base (which does the real work). */
TS_STATE(INI_START)			{ return rDO|INI_ts2IOsockTCPserver_START; }
TS_STATE(INI_ts2IOsockTCPserver_START)	{ return rDO|INI_ts2IOsockServer_START; }
TS_STATE(FIN_START)			{ return rDO|FIN_ts2IOsockTCPserver_START; }
TS_STATE(FIN_ts2IOsockTCPserver_START)	{ return rDO|FIN_ts2IOsockServer_START; }
