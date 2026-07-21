

#include	"std2/ts_mmsg.h"	/* _GNU_SOURCE + struct mmsghdr (must precede) */
#include	"_ts2/c++/ts2IOsocket_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsocket,ts2/c++/ts2IOdescriptor)


#if 0

TS_BEGIN_IMPLEMENT


/**
 * @brief ソケット向け sendto / recvfrom を追加した ts2IO 実装。/ ts2IO with sendto/recvfrom for sockets.
 * @details
 * `ts2IOdescriptor` に UDP / raw ソケット向けのアドレス付き送受信メソッドを追加する。
 * `sendto` / `recvfrom` はどちらも EAGAIN 時に sException で yield し、
 * ソケットが準備できると再開する。`ts2IOsockUDP` のベースクラス。
 * / Extends `ts2IOdescriptor` with address-aware send/receive for UDP/raw sockets.
 * Both methods yield on EAGAIN and resume when the socket is ready.
 * Base class of `ts2IOsockUDP`.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief ソケットなしで作成。fd はサブクラスが設定する。/ Create without socket; subclass sets fd. */
	ts2IOsocket_(
		sPtr<tinyState> parent);
	/** @brief 既存ソケット fd を渡して作成。/ Create with an existing socket fd. */
	ts2IOsocket_(
		sPtr<tinyState> parent,
		int _fd);

	/** @brief アドレスを指定してデータを送信する。EAGAIN 時は yield して再開。
	 *  / Send data to a specified address; yields on EAGAIN.
	 * @param buffer      送信バッファ。/ Source buffer.
	 * @param length      送信バイト数。/ Bytes to send.
	 * @param flags       sendto(2) フラグ。/ Flags for sendto(2).
	 * @param address     送信先アドレス。/ Destination address.
	 * @param address_len アドレス長。/ Address length.
	 * @return 送信バイト数。エラー時は負値。/ Bytes sent; negative on error.
	 */
	int sendto(const void * buffer,int length,int flags,
			const struct sockaddr * address,int address_len);
	/** @brief データを受信してアドレスも取得する。EAGAIN 時は yield して再開。
	 *  / Receive data and retrieve sender address; yields on EAGAIN.
	 * @param socket         ソケット fd (内部では `fd` を使用)。/ Socket fd (internally uses `fd`).
	 * @param buffer         受信バッファ。/ Destination buffer.
	 * @param length         バッファサイズ。/ Buffer size.
	 * @param flags          recvfrom(2) フラグ。/ Flags for recvfrom(2).
	 * @param address        送信元アドレスの格納先。/ Output: sender address.
	 * @param address_length アドレス長の格納先。/ Output: address length.
	 * @return 受信バイト数。エラー時は負値。/ Bytes received; negative on error.
	 */
	int recvfrom(
		int socket, char * buffer,int length, int flags,
		struct sockaddr * address,int * address_length);
	/** @brief 複数メッセージを一括送信する (Linux ネイティブ sendmmsg)。EAGAIN 時は yield。
	 *  / Send multiple messages in one call (native sendmmsg on Linux); yields on EAGAIN.
	 * @param msgvec 送信するメッセージベクタ。/ Message vector to send.
	 * @param vlen   ベクタ要素数。/ Number of elements in msgvec.
	 * @param flags  sendmmsg(2) フラグ。/ Flags for sendmmsg(2).
	 * @return 送信できたメッセージ数。エラー時は負値。/ Messages sent; negative on error.
	 */
	int sendmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags);
	/** @brief 複数メッセージを一括受信する (Linux ネイティブ recvmmsg)。データ待ちで yield。
	 *  / Receive multiple messages in one call (native recvmmsg on Linux); yields until ready.
	 * @param msgvec  受信バッファのメッセージベクタ。/ Message vector for received data.
	 * @param vlen    ベクタ要素数。/ Number of elements in msgvec.
	 * @param flags   recvmmsg(2) フラグ。/ Flags for recvmmsg(2).
	 * @param timeout 受信タイムアウト (NULL 可)。/ Receive timeout (may be NULL).
	 * @return 受信したメッセージ数。エラー時は負値。/ Messages received; negative on error.
	 */
	int recvmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags,
			struct timespec * timeout);
	/** @brief ENHANCED(RIO) 受信の同時 post 深さを制限する (計測用ノブ)。0=無制限。
	 *  / Cap concurrently-posted RIO receives (measurement knob).  Only the Windows (RIO)
	 *  overlay acts on it; on POSIX it is stored and ignored (already batched). */
	void set_rio_recv_depth(int depth);
	/** @brief ENHANCED(RIO) リングのスロット数 (計測用ノブ)。Windows RIO のみ作用、POSIX は保存のみ。 */
	void set_rio_ring_slots(int slots);
private:
protected:
	TS_DEFARGS
	/* batch cursor across yields — used only by the __CYGWIN__ per-datagram
	   fallback.  Linux (native recvmmsg/sendmmsg) and macOS (recvmsg_x/
	   sendmsg_x) each transfer the batch in one syscall, so they need no cursor. */
	unsigned int	mmsg_i;
	/* RIO recv-depth / ring-slots knob intent; stored and ignored on POSIX (RIO is
	   Windows-only; Linux/macOS are already batched, Cygwin uses the per-datagram fallback). */
	int		rio_recv_depth;
	int		rio_slots;
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
    mmsg_i = 0;
    rio_recv_depth = 0;
    rio_slots = 0;
}

ts2IOsocket_::ts2IOsocket_(TS_ARGS1)
        : ts2IOdescriptor_(parent,_fd)
{
    TS_CPARGS1
    mmsg_i = 0;
    rio_recv_depth = 0;
    rio_slots = 0;
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/

int
ts2IOsocket_::sendto(const void * buf,int length,int flags,
			const struct sockaddr * address,int address_len)
{
int er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; ; ) {
		er = ::sendto(fd,buf,length,flags,address,address_len);
		if ( er >= 0 )
			return er;
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
//::printf("EAGAIN--\n");
		  	io->write(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}

int
ts2IOsocket_::recvfrom(
		int socket, char * buffer,
		int length, int flags,
		struct sockaddr * address,
		int * address_length)
{
int er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; ; ) {
		er = ::recvfrom(fd,buffer,length,flags,address,(socklen_t*)address_length);
		if ( er >= 0 )
			return er;
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
//::printf("EAGAIN--\n");
		  	io->read(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}

#if !defined(__CYGWIN__) && !defined(__APPLE__)

/* Linux: one native batched syscall + EAGAIN yield (zero translation).
   Only Linux has the batched mmsg syscalls; Cygwin / macOS use the
   per-datagram recvmsg/sendmsg fallback below. */
int
ts2IOsocket_::sendmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags)
{
int er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; ; ) {
		er = ::sendmmsg(fd,msgvec,vlen,flags);
		if ( er >= 0 )
			return er;
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
		  	io->write(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}

int
ts2IOsocket_::recvmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags,
			struct timespec * timeout)
{
int er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; ; ) {
		er = ::recvmmsg(fd,msgvec,vlen,flags,timeout);
		if ( er >= 0 )
			return er;
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
		  	io->read(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}

#elif defined(__APPLE__)

/*
 * macOS/Darwin: batched datagram I/O via sendmsg_x / recvmsg_x (self-declared in
 * ts_mmsg.h — exported by libSystem, absent from the SDK headers).  Structurally
 * identical to the Linux native path above (one syscall; EAGAIN yields), with a
 * struct mmsghdr[] <-> struct msghdr_x[] translation.  Up to TS2_MMSG_MAXMSG are
 * processed per call; callers advance msgvec for longer vectors (the standard
 * mmsg partial-return contract, exactly as native recvmmsg returns "as many as
 * are ready").  No mmsg_i cursor: like Linux, one syscall either transfers >=1 or
 * yields on EAGAIN — the per-datagram cursor is only for the Cygwin loop below.
 */
int
ts2IOsocket_::sendmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags)
{
int er;
struct msghdr_x xv[TS2_MMSG_MAXMSG];
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	unsigned int n = vlen < TS2_MMSG_MAXMSG ? vlen : TS2_MMSG_MAXMSG;
	for ( unsigned int i = 0 ; i < n ; i++ ) {
		struct msghdr * h = &msgvec[i].msg_hdr;
		xv[i].msg_name       = h->msg_name;
		xv[i].msg_namelen    = h->msg_namelen;
		xv[i].msg_iov        = h->msg_iov;
		xv[i].msg_iovlen     = (int)h->msg_iovlen;
		xv[i].msg_control    = h->msg_control;
		xv[i].msg_controllen = h->msg_controllen;
		xv[i].msg_flags      = h->msg_flags;
		xv[i].msg_datalen    = 0;
	}
	for ( ; ; ) {
		er = ::sendmsg_x(fd,xv,n,flags);
		if ( er >= 0 ) {
			for ( int i = 0 ; i < er ; i++ )
				msgvec[i].msg_len = (unsigned int)xv[i].msg_datalen;
			return er;
		}
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
			io->write(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}

int
ts2IOsocket_::recvmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags,
			struct timespec * timeout)
{
int er;
struct msghdr_x xv[TS2_MMSG_MAXMSG];
	(void)timeout;	/* recvmsg_x has no per-call timeout; socket is O_NONBLOCK */
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	unsigned int n = vlen < TS2_MMSG_MAXMSG ? vlen : TS2_MMSG_MAXMSG;
	for ( unsigned int i = 0 ; i < n ; i++ ) {
		struct msghdr * h = &msgvec[i].msg_hdr;
		xv[i].msg_name       = h->msg_name;
		xv[i].msg_namelen    = h->msg_namelen;
		xv[i].msg_iov        = h->msg_iov;
		xv[i].msg_iovlen     = (int)h->msg_iovlen;
		xv[i].msg_control    = h->msg_control;
		xv[i].msg_controllen = h->msg_controllen;
		xv[i].msg_flags      = 0;
		xv[i].msg_datalen    = 0;
	}
	for ( ; ; ) {
		er = ::recvmsg_x(fd,xv,n,flags);
		if ( er >= 0 ) {
			for ( int i = 0 ; i < er ; i++ ) {
				struct msghdr * h = &msgvec[i].msg_hdr;
				h->msg_namelen    = xv[i].msg_namelen;
				h->msg_controllen = xv[i].msg_controllen;
				h->msg_flags      = xv[i].msg_flags;
				msgvec[i].msg_len = (unsigned int)xv[i].msg_datalen;
			}
			return er;
		}
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
			io->read(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}

#else /* __CYGWIN__ */

/*
 * Cygwin has no batched mmsg syscall, so loop ::sendmsg/::recvmsg one datagram
 * at a time (the socket is O_NONBLOCK, like the recvfrom path).  POSIX
 * semantics: fill/drain as many as are ready WITHOUT blocking, block (yield)
 * only when we have none yet.  mmsg_i persists across the yield; the state
 * function re-runs and re-enters here at the same cursor, exactly like the
 * partial-progress contract of recvfrom.
 */
int
ts2IOsocket_::sendmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags)
{
int er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; mmsg_i < vlen ; ) {
		er = ::sendmsg(fd,&msgvec[mmsg_i].msg_hdr,flags);
		if ( er >= 0 ) {
			msgvec[mmsg_i].msg_len = (unsigned int)er;
			mmsg_i++;
			continue;
		}
		if ( errno == EINTR )
			continue;
		if ( errno == EAGAIN ) {
			if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
			io->write(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
		return -1;
	}
	{ unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
}

int
ts2IOsocket_::recvmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags,
			struct timespec * timeout)
{
int er;
	(void)timeout;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; mmsg_i < vlen ; ) {
		er = ::recvmsg(fd,&msgvec[mmsg_i].msg_hdr,flags);
		if ( er >= 0 ) {
			msgvec[mmsg_i].msg_len = (unsigned int)er;
			mmsg_i++;
			continue;
		}
		if ( errno == EINTR )
			continue;
		if ( errno == EAGAIN ) {
			if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
			io->read(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		if ( mmsg_i > 0 ) { unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
		return -1;
	}
	{ unsigned int r = mmsg_i; mmsg_i = 0; return (int)r; }
}

#endif /* platform mmsg backend */


/* ENHANCED(RIO) recv-depth knob: stored and ignored on POSIX (Linux/macOS are already
   batched, Cygwin uses the per-datagram fallback).  Present so the shared interface is
   uniform across arches — only the Windows RIO overlay acts on it. */
void
ts2IOsocket_::set_rio_recv_depth(int depth)
{
	rio_recv_depth = depth;
}

void
ts2IOsocket_::set_rio_ring_slots(int slots)
{
	rio_slots = slots;
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
