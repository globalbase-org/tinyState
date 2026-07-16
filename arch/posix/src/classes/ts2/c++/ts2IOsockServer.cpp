/*
 * ts2IOsockServer — POSIX (non-blocking accept + fwIO/select) stream-server base.
 * Windows-port design memo.  (Windows counterpart: arch/posix_MinGW, overlapped AcceptEx.)
 *
 * Address-family-independent server machinery: socket / listen / non-blocking
 * accept with an fwIO(select) yield / teardown.  The AF and endpoint are given
 * by virtual hooks (sock_family / build_addr / backlog / pre_bind / post_close)
 * so ts2IOsockTCPserver (AF_INET) and ts2IOsockUnixServer (AF_UNIX, + socket-file
 * lifecycle) are thin subclasses sharing this whole accept path.
 */

#include	<sys/socket.h>
#include	<sys/un.h>
#include	<netinet/in.h>
#include	<fcntl.h>
#include	<errno.h>
#include	"_ts2/c++/ts2IOsockServer_.h"
#include	"ts2/c++/ts2IOdescriptor.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsockServer,ts2/c++/tinyState)

#if 0

TS_BEGIN_IMPLEMENT

class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2IOsockServer_(sPtr<tinyState> parent);

	sPtr<ts2IO> accept(int * errp,struct sockaddr * peer,sPtr<tinyState>caller);

	int err;
	const char * errpos;

	virtual int  sock_family();
	virtual int  build_addr(struct sockaddr_storage * sa);
	virtual int  backlog();
	virtual int  reuse_addr();
	virtual void pre_bind();
	virtual void post_close();
protected:
	TS_DEFARGS

	int		sock;
	sPtr<fwIO>	io;
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
	sock = -1;
}

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
socklen_t alen;
int a_sock;
	for ( ; ; ) {
		alen = sizeof(*peer);
		a_sock = soACCEPT(sock,peer,&alen);
		if ( a_sock < 0 ) {
			switch ( errno ) {
			case EINTR:
				continue;
			case EAGAIN:
				if ( is_destroyed() ) {		/* torn down while waiting */
					if ( errp )
						*errp = errno;
					return thNULL;
				}
			  	io->read(sCallSection::key->caller(),sock);
				throw sException([this](sPtr<tinyState> caller) {
					return !io->is_read(caller);
				});
			}
			if ( errp )
				*errp = errno;
			return thNULL;
		}
		if ( errp )
			*errp = 0;
		return thNEW(ts2IOdescriptor,(caller,a_sock));
	}
}


/*******************************************
	STATE MACHINE
********************************************/

TS_THREAD(INI_ts2IOsockServer_START)
{
  	sock = soSOCKET(sock_family(),SOCK_STREAM,0);
	if ( sock < 0 ) {
		errpos = "socket";
		err = errno;
		return rDO|FIN_ERROR;
	}
	if ( reuse_addr() ) {		/* TCP only */
	const int yes = 1;
		setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(const char*)&yes,sizeof(yes));
	}
	pre_bind();
struct sockaddr_storage sa;
int alen = build_addr(&sa);
	if ( alen <= 0 || bind(sock,(struct sockaddr*)&sa,alen) < 0 ) {
		errpos = "bind";
		err = errno;
		return rDO|FIN_ERROR;
	}
 	if ( ::listen(sock,backlog()) < 0 ) {
		errpos = "listen";
		err = errno;
		return rDO|FIN_ERROR;
	}
int flags;
	flags = fcntl(sock,F_GETFL,0);
	flags |= O_NONBLOCK;
	fcntl(sock,F_SETFL,flags);

	io = application->fw();
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
	if ( sock >= 0 && io != thNULL )
		io->abort(sock);
	return rDO|FIN_CLOSE;
}
TS_THREAD(FIN_CLOSE)
{
	if ( sock >= 0 )
		soCLOSE(sock);
	sock = -1;
	post_close();
	io = thNULL;
	return rDO|FIN_TINYSTATE_START;
}
