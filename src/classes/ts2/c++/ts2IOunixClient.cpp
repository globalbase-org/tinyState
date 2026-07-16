

#include	"_ts2/c++/ts2IOunixClient_.h"
#ifdef _WIN32
#include	<afunix.h>		/* winsock AF_UNIX / SOCKADDR_UN (Win10 1803+) */
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif
#else
#include	<sys/un.h>
#endif

CLASS_TINYSTATE(ts2/c++/ts2IOunixClient,ts2/c++/ts2IOdescriptor)


#if 0

TS_BEGIN_IMPLEMENT


/**
 * @brief Unix ドメインソケットに接続する ts2IO 実装。/ ts2IO that connects to a Unix domain socket.
 * @details
 * `INI_START` (TS_THREAD) で `socket(AF_LOCAL, SOCK_STREAM)` → `connect()` の順で接続し、
 * 成功すると `ts2IOdescriptor` として使用可能になる。
 * `destroy()` が呼ばれると `THR_CATCH_KILL` によって TS_THREAD 内の connect が中断される。
 * エラー時は `state = TS2IO_ERROR` になり `FIN_START` へ遷移する。
 * / Runs `socket(AF_LOCAL, SOCK_STREAM)` → `connect()` in `INI_START` (TS_THREAD).
 * If `destroy()` is called during connect, `THR_CATCH_KILL` safely aborts the TS_THREAD.
 * On error, sets `state = TS2IO_ERROR` and transitions to `FIN_START`.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief Unix ドメインソケットパスを指定して接続を開始する。/ Start connecting to a Unix domain socket path.
	 * @param parent   親 tinyState。/ Parent tinyState.
	 * @param filename 接続先のソケットファイルパス。/ Path to the Unix domain socket file.
	 */
	ts2IOunixClient_(
		sPtr<tinyState> parent,
		sPtr<stdString> filename);
private:
protected:
	sPtr<stdString> 	filename;
};

TS_END_IMPLEMENT

#endif


ts2IOunixClient_::ts2IOunixClient_(
		sPtr<tinyState> _parent,
		sPtr<stdString> filename)
        : ts2IOdescriptor_(_parent)
{
	this->filename = filename;
}

/*******************************************
	INSTANCE FUNCTIONS
********************************************/



/*******************************************
	STATE MACHINE
********************************************/


TS_THREAD(INI_START)
{
int er,er2;
struct sockaddr_un sun;
	memset(&sun,0,sizeof(sun));
	er = soSOCKET(AF_LOCAL,SOCK_STREAM,0);
	if ( er < 0 ) {
		err = errno;
		state = TS2IO_ERROR;
		return rDO|FIN_START;
	}
	sun.sun_family = AF_LOCAL;
	strcpy(sun.sun_path,filename->get_str());


	THR_CATCH_KILL(
		er2 = connect(er,(const struct sockaddr*)&sun,sizeof(sun));
	,
		er2 = -1;
		errno = EINTR;
	);
	if ( er2 < 0 || is_destroyed() ) {
		err = errno;
		state = TS2IO_ERROR;
		return rDO|FIN_START;
	}
#ifdef _WIN32
	fd = (HANDLE)(INT_PTR)er;
#else
	fd = er;
#endif
	return rDO|INI_ts2IOdescriptor_START;
}


