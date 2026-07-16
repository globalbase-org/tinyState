

#include	"_ts2/c++/ts2IOsockTCPconnect_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsockTCPconnect,ts2/c++/ts2IOdescriptor)


#if 0

TS_BEGIN_IMPLEMENT


/**
 * @brief TCP クライアント接続を確立する ts2IO 実装。/ ts2IO that establishes a TCP client connection.
 * @details
 * `INI_START` (TS_THREAD) で `getaddrinfo()` → `socket()` → `connect()` の一連のシーケンスを実行し、
 * 接続後に `ts2IOdescriptor` として使用可能になる。
 * EAI_AGAIN / EAGAIN の場合は自動的にリトライする。
 * エラー時は `eai_err` / `err` / `errpos` にエラー情報が入り、`ACT_ERROR` 状態に遷移する。
 *
 * @par 使用例 / Example
 * @code{.cpp}
 * struct sockaddr_in resolved;
 * conn = thNEW(ts2IOsockTCPconnect, (ifThis, &resolved, "example.com", 80));
 * @endcode
 * / Runs `getaddrinfo()` → `socket()` → `connect()` in `INI_START` (TS_THREAD),
 * then becomes usable as `ts2IOdescriptor`. Retries on EAI_AGAIN/EAGAIN automatically.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief ホスト名 (C 文字列) を指定して TCP 接続を開始する。/ Start TCP connection to a host name (C string).
	 * @param parent      親 tinyState。/ Parent tinyState.
	 * @param resolve     接続後に実際のアドレスが書き込まれる `sockaddr_in`。/ Output: resolved address after connection.
	 * @param target_name 接続先ホスト名または IP アドレス。/ Host name or IP address to connect to.
	 * @param port        接続先ポート番号。/ Destination port number.
	 */
	ts2IOsockTCPconnect_(
		sPtr<tinyState> parent,
		struct sockaddr_in * 	resolve,
		const char * target_name,
		int port);
	/** @brief ホスト名 (stdString) を指定して TCP 接続を開始する。/ Start TCP connection to a host name (stdString).
	 * @param parent       親 tinyState。/ Parent tinyState.
	 * @param resolve      接続後に実際のアドレスが書き込まれる `sockaddr_in`。/ Output: resolved address after connection.
	 * @param target_name2 接続先ホスト名または IP アドレス。/ Host name or IP address to connect to.
	 * @param port         接続先ポート番号。/ Destination port number.
	 */
	ts2IOsockTCPconnect_(
		sPtr<tinyState> parent,
		struct sockaddr_in * 	resolve,
		sPtr<stdString> target_name2,
		int port);

	/** @brief `getaddrinfo()` が返したエラーコード (`EAI_*`)。エラーなし時は 0。
	 *  / Error code from `getaddrinfo()` (`EAI_*`); 0 on success. */
	int eai_err;
	/** @brief エラーが発生したシステムコール名 ("getaddrinfo", "connect" 等)。
	 *  / Name of the system call where the error occurred ("getaddrinfo", "connect", etc.). */
	const char * 		errpos;
private:
protected:
	TS_DEFARGS
	int sfd;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE

#endif


ts2IOsockTCPconnect_::ts2IOsockTCPconnect_(TS_ARGS0)
        : ts2IOdescriptor_(parent)
{
    TS_CPARGS0
}

ts2IOsockTCPconnect_::ts2IOsockTCPconnect_(TS_ARGS1)
        : ts2IOdescriptor_(parent)
{
    TS_CPARGS1
	target_name = target_name2->get_str();
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/



/*******************************************
	STATE MACHINE
********************************************/

TS_THREAD(INI_START)
{
struct addrinfo hints;
struct addrinfo *result, *rp;
struct sockaddr_in target_addr;
int s;

	sfd = -1;

	/* Obtain address(es) matching host/port */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;    /* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* Stream socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0;          /* Any protocol */
	for ( ; ; ) {
		s = ::getaddrinfo(target_name,0, &hints, &result);
		switch ( s ){
		case 0:
			break;
		case EAI_AGAIN:
			sleep(1);
			continue;
#ifndef _WIN32
		/* winsock getaddrinfo returns its error directly (no EAI_SYSTEM/errno);
		   such errors fall through to default: below. */
		case EAI_SYSTEM:
			if ( errno == EINTR )
				continue;
			if ( errno == EAGAIN ) {
				sleep(1);
				continue;
			}
			errpos = "getaddrinfo";
			eai_err = s;
			err = errno;
			return rDO|ACT_ERROR;
#endif
		default:
			errpos = "getaddrinfo";
			eai_err = s;
			return rDO|ACT_ERROR;
		}
		break;
	}


	/* getaddrinfo() returns a list of address structures.
	   Try each address until we successfully connect(2).
	   If socket(2) (or connect(2)) fails, we (close the socket
	   and) try the next address. */

	for (rp = result; rp  ; rp = rp->ai_next) {
		sfd = soSOCKET(rp->ai_family, rp->ai_socktype,
			rp->ai_protocol);
		if ( sfd >= 0 )
			break;
	}

	if (rp == NULL) {               /* No address succeeded */
		::freeaddrinfo(result);           /* No longer needed */
		errpos = "socket::no address suceeded";
		return rDO|ACT_ERROR_PREV;
	}
	memcpy((struct sockaddr*)&target_addr,rp->ai_addr,sizeof(target_addr));
	target_addr.sin_port = htons(port);
	freeaddrinfo(result);           /* No longer needed */

	for ( ; ; ) {
		s = ::connect(sfd,(struct sockaddr*)&target_addr,sizeof(target_addr));
		if ( s < 0 ) {
		int er = errno;
			switch ( er ) {
			case EINTR:
				continue;
			case EAGAIN:
				sleep(1);
				continue;
			case ETIMEDOUT:
				err = errno;
				return rDO|ACT_ERROR_PREV;
			}
			err = errno;
			errpos = "connect";
			return ACT_ERROR_PREV;
		}
		break;
	}
	memcpy(resolve,&target_addr,sizeof(*resolve));
#ifdef _WIN32
	fd = (HANDLE)(INT_PTR)sfd;
#else
	fd = sfd;
#endif
	return rDO|INI_ts2IOdescriptor_START;
}


TS_THREAD(ACT_ERROR_PREV)
{
	soCLOSE(sfd);
  	return rDO|ACT_ERROR;
}
