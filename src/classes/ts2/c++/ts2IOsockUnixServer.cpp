/*
 * ts2IOsockUnixServer — AF_UNIX stream server.  Windows-port design memo.
 *
 * The AF_UNIX sibling of ts2IOsockTCPserver.  Same thin-subclass shape: it
 * supplies the address family (AF_UNIX) and a sockaddr_un built from a
 * filesystem path.  The one thing TCP does not have is the *access-point file*
 * lifecycle, so this class overrides two extra hooks:
 *   pre_bind()   -> unlink a stale socket file (bind fails EADDRINUSE otherwise)
 *   post_close() -> unlink the socket file on teardown (else it lingers)
 * Everything else (socket/listen/accept/AcceptEx/teardown) is inherited from
 * ts2IOsockServer.  AF_UNIX is available on Windows 10 1803+/11 via afunix.h.
 */
#ifdef _WIN32
#include	<winsock2.h>
#include	<afunix.h>
#include	<io.h>			/* _unlink */
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif
#define UNLINK_PATH(p)	_unlink(p)
#else
#include	<sys/socket.h>
#include	<sys/un.h>
#include	<unistd.h>
#define UNLINK_PATH(p)	unlink(p)
#endif
#include	<string.h>
#include	"_ts2/c++/ts2IOsockUnixServer_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsockUnixServer,ts2/c++/ts2IOsockServer)

#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief AF_UNIX (Unix ドメイン) サーバ。/ AF_UNIX (Unix domain) server.
 * @details ts2IOsockServer の薄いサブクラス。AF_UNIX + パスから sockaddr_un を作り、
 * ソケットファイルの unlink (bind 前の stale 除去 / teardown 時の削除) を追加。
 * @code server = thNEW(ts2IOsockUnixServer,(ifThis, thNEW(stdString,("/tmp/s.sock")))); @endcode
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @param path ソケットファイルパス。@param p_listen listen backlog (既定10)。 */
	ts2IOsockUnixServer_(
		sPtr<tinyState> parent,
		sPtr<stdString> path,
		int p_listen=10);

	virtual int  sock_family();
	virtual int  build_addr(struct sockaddr_storage * sa);
	virtual int  backlog();
	virtual void pre_bind();
	virtual void post_close();
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
class stdString;
TS_END_INTERFACE

#endif


ts2IOsockUnixServer_::ts2IOsockUnixServer_(TS_ARGS0)
        : ts2IOsockServer_(parent)
{
    TS_CPARGS0
}

int
ts2IOsockUnixServer_::sock_family()
{
	return AF_LOCAL;
}
int
ts2IOsockUnixServer_::backlog()
{
	return p_listen;
}
int
ts2IOsockUnixServer_::build_addr(struct sockaddr_storage * sa)
{
struct sockaddr_un * p = (struct sockaddr_un*)sa;
	memset(p,0,sizeof(*p));
	p->sun_family = AF_LOCAL;
	strncpy(p->sun_path,path->get_str(),sizeof(p->sun_path)-1);
	return sizeof(struct sockaddr_un);
}
void
ts2IOsockUnixServer_::pre_bind()		/* remove a stale socket file before bind */
{
	UNLINK_PATH(path->get_str());
}
void
ts2IOsockUnixServer_::post_close()		/* remove the socket file on teardown */
{
	UNLINK_PATH(path->get_str());
}


/* INI/FIN chain into the base (which does the real work). */
TS_STATE(INI_START)			{ return rDO|INI_ts2IOsockUnixServer_START; }
TS_STATE(INI_ts2IOsockUnixServer_START)	{ return rDO|INI_ts2IOsockServer_START; }
TS_STATE(FIN_START)			{ return rDO|FIN_ts2IOsockUnixServer_START; }
TS_STATE(FIN_ts2IOsockUnixServer_START)	{ return rDO|FIN_ts2IOsockServer_START; }
