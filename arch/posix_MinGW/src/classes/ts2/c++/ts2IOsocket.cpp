/*
 * ts2IOsocket — MinGW (winsock + overlapped) implementation.  Windows-port design memo §6/§7.
 *
 * Kept deriving from ts2IOdescriptor: on Windows ReadFile/WriteFile work on an
 * overlapped socket, so the base's overlapped read()/write() + IOCP association
 * + fdid are reused as-is for connected (TCP) streams.  This class only adds the
 * address-aware datagram path (sendto/recvfrom) that UDP needs, implemented with
 * overlapped WSASendTo/WSARecvFrom driven through the same IOCP as the base
 * (reusing the inherited write_ov/read_ov + write_pending/read_pending + fdid).
 *
 * (This refines Windows-port design memo §1b: that note assumed nothing was reusable from the
 * descriptor; in fact the whole overlapped/IOCP machinery is, so the POSIX
 * hierarchy is preserved and only the datagram calls + socket creation diverge.)
 *
 * FIRST CUT (2026-07-12): compiles.  The recvfrom source-address buffer lifetime
 * across the overlapped op is a known refinement (the addr/alen must persist to
 * completion); wired minimally here.  Run verification pending on the box.
 */

#include	<winsock2.h>
#include	<ws2tcpip.h>
#include	<mswsock.h>	/* LPFN_WSARECVMSG / WSAID_WSARECVMSG */
#include	<errno.h>
#include	"std2/ts_mmsg.h"	/* struct mmsghdr + TS2_MMSG_MAXIOV (precede _.h) */
#include	"_ts2/c++/ts2IOsocket_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsocket,ts2/c++/ts2IOdescriptor)

/* Translate a POSIX msghdr into a winsock WSAMSG (+ WSABUF scatter array).
   Returns -1 if the scatter/gather count exceeds the fallback cap. */
static int
ts2_to_wsamsg(WSAMSG * w,WSABUF * bufs,struct msghdr * m)
{
unsigned int n = (unsigned int)m->msg_iovlen;
	if ( n > TS2_MMSG_MAXIOV )
		return -1;
	for ( unsigned int k = 0; k < n; k++ ) {
		bufs[k].len = (ULONG)m->msg_iov[k].iov_len;
		bufs[k].buf = (CHAR*)m->msg_iov[k].iov_base;
	}
	w->name          = (LPSOCKADDR)m->msg_name;
	w->namelen       = (INT)m->msg_namelen;
	w->lpBuffers     = bufs;
	w->dwBufferCount = n;
	w->Control.buf   = (CHAR*)m->msg_control;
	w->Control.len   = (ULONG)m->msg_controllen;
	w->dwFlags       = (DWORD)m->msg_flags;
	return 0;
}

/* Write receive-side results (updated by WSARecvMsg) back into the msghdr. */
static void
ts2_from_wsamsg(struct msghdr * m,WSAMSG * w)
{
	m->msg_namelen    = (socklen_t)w->namelen;
	m->msg_controllen = (size_t)w->Control.len;
	m->msg_flags      = (int)w->dwFlags;
}

#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief ソケット向け sendto/recvfrom を追加した ts2IO 実装 (MinGW: overlapped winsock)。
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2IOsocket_(
		sPtr<tinyState> parent);
	ts2IOsocket_(
		sPtr<tinyState> parent,
		HANDLE _fd);

	int sendto(const void * buffer,int length,int flags,
			const struct sockaddr * address,int address_len);
	int recvfrom(
		int socket, char * buffer,int length, int flags,
		struct sockaddr * address,int * address_length);
	int sendmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags);
	int recvmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags,
			struct timespec * timeout);
protected:
	INT		recv_alen;	/* addr length buffer for overlapped WSARecvFrom */
	/* mmsg fallback: no true batch on Windows, so recv/sendmmsg loop one
	   datagram at a time (WSARecvMsg/WSASendMsg).  These persist the in-flight
	   translated message + batch cursor across the yield used to block for the
	   first datagram; a single ts2IO is driven by one state machine and obeys
	   "1 state = 1 I/O call", so recv and send never overlap on one object. */
	void *		pWSARecvMsg;	/* LPFN_WSARECVMSG (void* per pAcceptEx手口) */
	unsigned int	mmsg_i;		/* batch cursor across yields */
	int		mmsg_nb;	/* socket switched to non-blocking for drain */
	WSAMSG		mmsg_wsamsg;	/* in-flight WSAMSG (persists across yield) */
	WSABUF		mmsg_buf[TS2_MMSG_MAXIOV];	/* translated iov (persists) */
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE

#endif


ts2IOsocket_::ts2IOsocket_(TS_ARGS0)
        : ts2IOdescriptor_(parent)
{
    TS_CPARGS0
    pWSARecvMsg = 0;
    mmsg_i = 0;
    mmsg_nb = 0;
}

ts2IOsocket_::ts2IOsocket_(TS_ARGS1)
        : ts2IOdescriptor_(parent,_fd)
{
    TS_CPARGS1
    pWSARecvMsg = 0;
    mmsg_i = 0;
    mmsg_nb = 0;
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/

int
ts2IOsocket_::sendto(const void * buf,int length,int flags,
			const struct sockaddr * address,int address_len)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	if ( write_pending ) {
	DWORD got = 0, fl = 0;
		if ( WSAGetOverlappedResult((SOCKET)fd,&write_ov,&got,FALSE,&fl) ) {
			write_pending = 0;
			return (int)got;
		}
	int e = WSAGetLastError();
		if ( e == WSA_IO_INCOMPLETE ) {
			io->write(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_write(caller); });
		}
		write_pending = 0;
		err = e; state = TS2IO_ERROR; return -1;
	}
	ensure_assoc();
	memset(&write_ov,0,sizeof(write_ov));
	{
	WSABUF wb; wb.buf = (CHAR*)buf; wb.len = (ULONG)length;
	DWORD got = 0;
		if ( WSASendTo((SOCKET)fd,&wb,1,&got,(DWORD)flags,
				address,address_len,&write_ov,NULL) == 0 ||
				WSAGetLastError() == WSA_IO_PENDING ) {
			write_pending = 1;
			io->write(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_write(caller); });
		}
		err = WSAGetLastError(); state = TS2IO_ERROR; return -1;
	}
}

int
ts2IOsocket_::recvfrom(
		int socket, char * buffer,
		int length, int flags,
		struct sockaddr * address,
		int * address_length)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	if ( read_pending ) {
	DWORD got = 0, fl = 0;
		if ( WSAGetOverlappedResult((SOCKET)fd,&read_ov,&got,FALSE,&fl) ) {
			read_pending = 0;
			if ( address_length )
				*address_length = (int)recv_alen;
			return (int)got;
		}
	int e = WSAGetLastError();
		if ( e == WSA_IO_INCOMPLETE ) {
			io->read(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_read(caller); });
		}
		read_pending = 0;
		err = e; state = TS2IO_ERROR; return -1;
	}
	ensure_assoc();
	memset(&read_ov,0,sizeof(read_ov));
	{
	WSABUF wb; wb.buf = buffer; wb.len = (ULONG)length;
	DWORD got = 0, fl = (DWORD)flags;
		recv_alen = address_length ? (INT)*address_length : 0;
		if ( WSARecvFrom((SOCKET)fd,&wb,1,&got,&fl,
				address, address ? &recv_alen : NULL, &read_ov,NULL) == 0 ||
				WSAGetLastError() == WSA_IO_PENDING ) {
			read_pending = 1;
			io->read(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_read(caller); });
		}
		err = WSAGetLastError(); state = TS2IO_ERROR; return -1;
	}
}

/*
 * sendmmsg / recvmmsg — Windows has no batched datagram syscall, so this is the
 * fallback: loop the vector one datagram at a time through WSASendMsg /
 * WSARecvMsg.  Throughput is unchanged vs sendto/recvfrom (the accepted "step #1
 * = API 形合わせ" outcome); the batch win lands only on Linux's native path.
 *
 * POSIX semantics are preserved: fill/drain as many datagrams as are ready
 * WITHOUT blocking, blocking (overlapped op + yield) only when we have none yet.
 * The socket is flipped non-blocking so a synchronous WSARecvMsg/WSASendMsg
 * reports WSAEWOULDBLOCK instead of stalling; overlapped ops are unaffected by
 * that mode, so read_c/write_c/recvfrom keep working.  We never leave an
 * overlapped op we won't wait for (that would post a stray IOCP completion for
 * our fdid — the Windows-port design memo 追記21 key-collision hazard), so "don't block for the
 * rest" is done with synchronous non-blocking calls, never a cancelled op.
 */

int
ts2IOsocket_::sendmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	/* resume: the overlapped send issued to place the first datagram drained. */
	if ( write_pending ) {
	DWORD got = 0, fl = 0;
		if ( WSAGetOverlappedResult((SOCKET)fd,&write_ov,&got,FALSE,&fl) ) {
			write_pending = 0;
			msgvec[mmsg_i].msg_len = (unsigned int)got;
			mmsg_i++;
		} else {
		int e = WSAGetLastError();
			if ( e == WSA_IO_INCOMPLETE ) {
				io->write(sCallSection::key->caller(),fdid);
				throw sException([this](sPtr<tinyState> caller){ return !io->is_write(caller); });
			}
			write_pending = 0;
			if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
			err = e; state = TS2IO_ERROR; return -1;
		}
	}
	ensure_assoc();
	if ( !mmsg_nb && fd != INVALID_HANDLE_VALUE ) {
	u_long one = 1;
		ioctlsocket((SOCKET)fd,FIONBIO,&one);
		mmsg_nb = 1;
	}
	for ( ; mmsg_i < vlen; ) {
		if ( ts2_to_wsamsg(&mmsg_wsamsg,mmsg_buf,&msgvec[mmsg_i].msg_hdr) < 0 ) {
			err = EMSGSIZE; state = TS2IO_ERROR; return -1;
		}
	DWORD got = 0;
		if ( WSASendMsg((SOCKET)fd,&mmsg_wsamsg,(DWORD)flags,&got,NULL,NULL) == 0 ) {
			msgvec[mmsg_i].msg_len = (unsigned int)got;
			mmsg_i++;
			continue;
		}
	int e = WSAGetLastError();
		if ( e == WSAEINTR )
			continue;
		if ( e == WSAEWOULDBLOCK ) {
			if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
			/* nothing sent yet: issue one overlapped send and yield for it. */
			memset(&write_ov,0,sizeof(write_ov));
			if ( WSASendMsg((SOCKET)fd,&mmsg_wsamsg,(DWORD)flags,&got,&write_ov,NULL) == 0 ) {
				msgvec[mmsg_i].msg_len = (unsigned int)got;
				mmsg_i++;
				continue;
			}
			if ( WSAGetLastError() == WSA_IO_PENDING ) {
				write_pending = 1;
				io->write(sCallSection::key->caller(),fdid);
				throw sException([this](sPtr<tinyState> caller){ return !io->is_write(caller); });
			}
			err = WSAGetLastError(); state = TS2IO_ERROR; return -1;
		}
		if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
		err = e; state = TS2IO_ERROR; return -1;
	}
	{ unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
}

int
ts2IOsocket_::recvmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags,
			struct timespec * timeout)
{
	(void)timeout;	/* per-datagram fallback: no timed wait (drain is non-blocking) */
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	/* resume: the overlapped recv issued to block for the first datagram landed. */
	if ( read_pending ) {
	DWORD got = 0, fl = 0;
		if ( WSAGetOverlappedResult((SOCKET)fd,&read_ov,&got,FALSE,&fl) ) {
			read_pending = 0;
			msgvec[mmsg_i].msg_len = (unsigned int)got;
			ts2_from_wsamsg(&msgvec[mmsg_i].msg_hdr,&mmsg_wsamsg);
			mmsg_i++;
		} else {
		int e = WSAGetLastError();
			if ( e == WSA_IO_INCOMPLETE ) {
				io->read(sCallSection::key->caller(),fdid);
				throw sException([this](sPtr<tinyState> caller){ return !io->is_read(caller); });
			}
			read_pending = 0;
			if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
			err = e; state = TS2IO_ERROR; return -1;
		}
	}
	ensure_assoc();
	if ( pWSARecvMsg == 0 ) {
	GUID g = WSAID_WSARECVMSG;
	DWORD got = 0;
		if ( WSAIoctl((SOCKET)fd,SIO_GET_EXTENSION_FUNCTION_POINTER,&g,sizeof(g),
				&pWSARecvMsg,sizeof(pWSARecvMsg),&got,NULL,NULL) != 0 ) {
			err = WSAGetLastError(); state = TS2IO_ERROR; return -1;
		}
	}
	if ( !mmsg_nb && fd != INVALID_HANDLE_VALUE ) {
	u_long one = 1;
		ioctlsocket((SOCKET)fd,FIONBIO,&one);
		mmsg_nb = 1;
	}
	for ( ; mmsg_i < vlen; ) {
		if ( ts2_to_wsamsg(&mmsg_wsamsg,mmsg_buf,&msgvec[mmsg_i].msg_hdr) < 0 ) {
			err = EMSGSIZE; state = TS2IO_ERROR; return -1;
		}
		mmsg_wsamsg.dwFlags = (DWORD)flags;	/* recvmmsg per-call flags are input; msg_flags is output */
	DWORD got = 0;
		if ( ((LPFN_WSARECVMSG)pWSARecvMsg)((SOCKET)fd,&mmsg_wsamsg,&got,NULL,NULL) == 0 ) {
			msgvec[mmsg_i].msg_len = (unsigned int)got;
			ts2_from_wsamsg(&msgvec[mmsg_i].msg_hdr,&mmsg_wsamsg);
			mmsg_i++;
			continue;
		}
	int e = WSAGetLastError();
		if ( e == WSAEINTR )
			continue;
		if ( e == WSAEWOULDBLOCK ) {
			if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
			/* nothing yet: issue one overlapped recv and yield until it lands. */
			memset(&read_ov,0,sizeof(read_ov));
			if ( ((LPFN_WSARECVMSG)pWSARecvMsg)((SOCKET)fd,&mmsg_wsamsg,&got,&read_ov,NULL) == 0 ) {
				msgvec[mmsg_i].msg_len = (unsigned int)got;
				ts2_from_wsamsg(&msgvec[mmsg_i].msg_hdr,&mmsg_wsamsg);
				mmsg_i++;
				continue;
			}
			if ( WSAGetLastError() == WSA_IO_PENDING ) {
				read_pending = 1;
				io->read(sCallSection::key->caller(),fdid);
				throw sException([this](sPtr<tinyState> caller){ return !io->is_read(caller); });
			}
			err = WSAGetLastError(); state = TS2IO_ERROR; return -1;
		}
		if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
		err = e; state = TS2IO_ERROR; return -1;
	}
	{ unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	return rDO|INI_ts2IOsocket_START;
}
TS_STATE(INI_ts2IOsocket_START)
{
	return rDO|INI_ts2IOdescriptor_START;
}

TS_STATE(FIN_START)
{
	return rDO|FIN_ts2IOsocket_START;
}
TS_STATE(FIN_ts2IOsocket_START)
{
	return rDO|FIN_ts2IOdescriptor_START;
}
