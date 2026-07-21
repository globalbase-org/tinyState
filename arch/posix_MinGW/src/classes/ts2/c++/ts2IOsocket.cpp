/*
 * ts2IOsocket — MinGW (winsock) implementation.  Windows-port design memo §6/§7.
 *
 * Deriving from ts2IOdescriptor: on Windows ReadFile/WriteFile work on an
 * overlapped socket, so the base's internal-ring + threadpool read()/write() is
 * reused as-is for connected (TCP) streams.  This class adds the address-aware
 * datagram path (sendto/recvfrom/sendmmsg/recvmmsg) that UDP needs, on the SAME
 * machinery: a datagram RECORD ring (ts2io_dgram_ring, boundaries + peer address
 * preserved) driven by WSASendTo/WSARecvFrom over the base's threadpool (tpio) via
 * the pump_io()/on_complete() hooks.  The kernel only ever touches a descriptor-
 * owned slot, never the caller's buffer — the v2 lifetime guarantee, for UDP too
 * docs/COOKBOOK.md §6.7.
 *
 * Sockets no longer touch fwIO's overlapped/IOCP path at all; the base keeps `io`
 * only for reactor keep-alive.
 */

#include	<winsock2.h>
#include	<ws2tcpip.h>
#include	<mswsock.h>	/* LPFN_WSARECVMSG / WSAID_WSARECVMSG */
#include	<errno.h>
#include	<stdlib.h>
#include	<string.h>
#include	"std2/ts_mmsg.h"	/* struct mmsghdr + TS2_MMSG_MAXIOV (precede _.h) */
#include	"ts2/c++/sThreadMutex.h"
#include	"ts2/c++/sThreadMutexHandle.h"


/* ---- datagram record ring ----------------------------------
 * A small ring of fixed-capacity slots that preserves datagram boundaries and
 * the peer address.  ONE overlapped op is in flight per direction (a single
 * drecv_ov / dsend_ov owned by ts2IOsocket); the ring gives read-ahead /
 * write-behind depth.  Crucially the kernel only ever touches a descriptor-owned
 * slot, never the caller's buffer — the v2 lifetime guarantee, now for UDP too.
 * Own sThreadMutex (taken by both the pool thread and the state machine).
 * SLOTS/CAP are tunable; CAP must cover the largest datagram (UDP max 65507).  */
#define TS2IO_DGRAM_CAP		(65536u)
#define TS2IO_DGRAM_SLOTS	(4)

class ts2io_dgram_ring {
	sThreadMutex	mtx;
	struct slot {
		char *	buf;
		DWORD	len;
		struct sockaddr_storage addr;
		INT	alen;
	} *		sl;
	int		n;
	DWORD		cap;
	int		head;		/* index of the oldest record */
	int		fill;		/* number of READY records */
	int		posted;		/* 1 while an OS op is in flight */
	int		errf;
public:
	ts2io_dgram_ring() : sl(0), n(0), cap(0), head(0), fill(0), posted(0), errf(0) {}
	~ts2io_dgram_ring() { free_all(); }

	int alloc(int n_,DWORD cap_) {
		sThreadMutexHandle __h(mtx);
		if ( sl ) return 0;
		sl = (slot*)::calloc(n_,sizeof(slot));
		if ( ! sl ) return -1;
		for ( int i = 0 ; i < n_ ; i++ ) {
			sl[i].buf = (char*)::malloc(cap_);
			if ( ! sl[i].buf ) return -1;
		}
		n = n_; cap = cap_;
		return 0;
	}
	void free_all() {
		sThreadMutexHandle __h(mtx);
		if ( sl ) {
			for ( int i = 0 ; i < n ; i++ ) if ( sl[i].buf ) ::free(sl[i].buf);
			::free(sl); sl = 0;
		}
	}

	/* --- RECV: OS fills slot (head+fill); app consumes slot head --- */
	int recv_reserve(char *& p,DWORD & c,struct sockaddr *& pa,INT *& pal) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || posted || errf || fill >= n ) return 0;
	slot & s = sl[(head+fill)%n];
		s.alen = (INT)sizeof(s.addr);
		p = s.buf; c = cap; pa = (struct sockaddr*)&s.addr; pal = &s.alen;
		posted = 1;
		return 1;
	}
	void recv_complete(DWORD bytes,int res) {
		sThreadMutexHandle __h(mtx);
		posted = 0;
		if ( res == 0 ) { sl[(head+fill)%n].len = bytes; fill++; }
		else if ( res == ERROR_OPERATION_ABORTED ) { /* teardown */ }
		else errf = res;
	}
	int recv_consume(void * dst,DWORD dlen,struct sockaddr * from,int * fromlen) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || fill == 0 ) return -1;		/* empty → caller parks */
	slot & s = sl[head];
	DWORD nb = s.len < dlen ? s.len : dlen;
		::memcpy(dst,s.buf,nb);
		if ( from && fromlen ) {
		int fl = (int)s.alen < *fromlen ? (int)s.alen : *fromlen;
			if ( fl > 0 ) ::memcpy(from,&s.addr,fl);
			*fromlen = (int)s.alen;
		}
		head = (head+1)%n; fill--;
		return (int)nb;
	}

	/* --- SEND: app fills slot (head+fill); OS drains slot head --- */
	int send_produce(const void * src,DWORD slen,const struct sockaddr * to,int tolen) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || fill >= n ) return -1;		/* full → caller parks */
	slot & s = sl[(head+fill)%n];
	DWORD nb = slen < cap ? slen : cap;
		::memcpy(s.buf,src,nb); s.len = nb;
		if ( to && tolen > 0 ) {
			s.alen = tolen <= (int)sizeof(s.addr) ? (INT)tolen : (INT)sizeof(s.addr);
			::memcpy(&s.addr,to,s.alen);
		} else	s.alen = 0;
		fill++;
		return (int)nb;
	}
	int send_reserve(char *& p,DWORD & len,struct sockaddr *& pa,INT & pal) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || posted || errf || fill == 0 ) return 0;
	slot & s = sl[head];
		p = s.buf; len = s.len; pa = (struct sockaddr*)&s.addr; pal = s.alen;
		posted = 1;
		return 1;
	}
	void send_complete(DWORD bytes,int res) {
		(void)bytes;
		sThreadMutexHandle __h(mtx);
		posted = 0;
		if ( res == 0 ) { head = (head+1)%n; fill--; }
		else if ( res == ERROR_OPERATION_ABORTED ) { /* teardown */ }
		else errf = res;
	}

	/* iov variants for recvmmsg/sendmmsg: gather scattered iov into one slot / scatter
	   a slot across iov.  msg_len is the full datagram length (POSIX truncation). */
	int send_produce_iov(const struct iovec * iov,int iovlen,const struct sockaddr * to,int tolen) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || fill >= n ) return -1;
	slot & s = sl[(head+fill)%n];
	DWORD off = 0;
		for ( int k = 0 ; k < iovlen && off < cap ; k++ ) {
		DWORD c = (DWORD)iov[k].iov_len; if ( c > cap - off ) c = cap - off;
			::memcpy(s.buf+off,iov[k].iov_base,c); off += c;
		}
		s.len = off;
		if ( to && tolen > 0 ) {
			s.alen = tolen <= (int)sizeof(s.addr) ? (INT)tolen : (INT)sizeof(s.addr);
			::memcpy(&s.addr,to,s.alen);
		} else	s.alen = 0;
		fill++;
		return (int)off;
	}
	int recv_consume_iov(const struct iovec * iov,int iovlen,struct sockaddr * from,int * fromlen) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || fill == 0 ) return -1;
	slot & s = sl[head];
	DWORD off = 0;
		for ( int k = 0 ; k < iovlen && off < s.len ; k++ ) {
		DWORD c = (DWORD)iov[k].iov_len; if ( c > s.len - off ) c = s.len - off;
			::memcpy(iov[k].iov_base,s.buf+off,c); off += c;
		}
		if ( from && fromlen ) {
		int fl = (int)s.alen < *fromlen ? (int)s.alen : *fromlen;
			if ( fl > 0 ) ::memcpy(from,&s.addr,fl);
			*fromlen = (int)s.alen;
		}
	DWORD total = s.len;			/* POSIX msg_len = full datagram size */
		head = (head+1)%n; fill--;
		return (int)total;
	}

	int has_ready()  { sThreadMutexHandle __h(mtx); return fill > 0; }
	int has_space()  { sThreadMutexHandle __h(mtx); return sl && fill < n; }
	int posted_now() { sThreadMutexHandle __h(mtx); return posted; }
	int error()      { sThreadMutexHandle __h(mtx); return errf; }
};


#include	"_ts2/c++/ts2IOsocket_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOsocket,ts2/c++/ts2IOdescriptor)

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
	/* --- datagram record-ring path (sendto/recvfrom over the base threadpool) --- */
	OVERLAPPED		drecv_ov;	/* single in-flight WSARecvFrom */
	OVERLAPPED		dsend_ov;	/* single in-flight WSASendTo */
	ts2io_dgram_ring *	drecvring;
	ts2io_dgram_ring *	dsendring;
	virtual int  pump_io();
	virtual void on_complete(OVERLAPPED * ov,int res,DWORD bytes);
	virtual void io_teardown_flush();
	virtual void io_free_buffers();

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
    drecvring = 0;
    dsendring = 0;
}

ts2IOsocket_::ts2IOsocket_(TS_ARGS1)
        : ts2IOdescriptor_(parent,_fd)
{
    TS_CPARGS1
    drecvring = 0;
    dsendring = 0;
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/

/* Datagram send/recv now run on the SAME internal-ring + threadpool machinery as
   the base stream path: sendto copies the caller's datagram into a
   descriptor-owned send-ring slot and returns immediately (partial contract), the
   ACT_START pump posts WSASendTo from the slot; recvfrom copies one buffered record
   out of the recv-ring, the pump keeps a WSARecvFrom posted into a free slot.  The
   kernel never touches the caller's buffer, so an abandoned op cannot scribble it. */
int
ts2IOsocket_::sendto(const void * buf,int length,int flags,
			const struct sockaddr * address,int address_len)
{
	(void)flags;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) { err = EBADF; return -1; }
	if ( ! dsendring ) dsendring = new ts2io_dgram_ring();
	if ( dsendring->alloc(TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	if ( dsendring->error() ) { err = dsendring->error(); state = TS2IO_ERROR; return -1; }
	{
	int n = dsendring->send_produce(buf,(DWORD)length,address,address_len);
		wakeup();				/* kick pump: post WSASendTo */
		if ( n >= 0 ) return n;			/* queued (partial-immediate) */
	}
	{
	sPtr<tinyState> c = sCallSection::key->caller();
		while ( write_waiters->del([c](sPtr<tinyState> x){ return x == c ? 1 : 0; }).is_notNull() ) ;
		write_waiters->ins(MAX_INTEGER64, c);
	}
	throw sException([this](sPtr<tinyState> caller){ return dsendring->has_space() || dsendring->error(); });
}

int
ts2IOsocket_::recvfrom(
		int socket, char * buffer,
		int length, int flags,
		struct sockaddr * address,
		int * address_length)
{
	(void)socket; (void)flags;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) { err = EBADF; return -1; }
	if ( ! drecvring ) drecvring = new ts2io_dgram_ring();
	if ( drecvring->alloc(TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	{
	int n = drecvring->recv_consume(buffer,(DWORD)length,address,address_length);
		if ( n >= 0 ) { wakeup(); return n; }	/* one datagram (0 = empty datagram) */
	}
	if ( drecvring->error() ) { err = drecvring->error(); state = TS2IO_ERROR; return -1; }
	{
	sPtr<tinyState> c = sCallSection::key->caller();
		while ( read_waiters->del([c](sPtr<tinyState> x){ return x == c ? 1 : 0; }).is_notNull() ) ;
		read_waiters->ins(MAX_INTEGER64, c);
	}
	wakeup();				/* park done → pump posts WSARecvFrom (demand-gated) */
	throw sException([this](sPtr<tinyState> caller){ return drecvring->has_ready() || drecvring->error(); });
}

/*
 * sendmmsg / recvmmsg — batched datagrams over the SAME record ring as
 * sendto/recvfrom.  Windows has no batched datagram syscall, so a
 * batch is just N ring ops: sendmmsg produces up to vlen datagrams into the send
 * ring (gathering each msg's iov), recvmmsg consumes up to vlen ready records into
 * each msg's iov.  POSIX "handle >=1, don't block for the rest" falls out naturally
 * — produce/consume until the ring is full/empty and return the count; block (park
 * + yield) only when none could be handled.  The pump (ACT_START) drives the actual
 * WSASendTo/WSARecvFrom, so the caller's buffers are never handed to the kernel.
 */

int
ts2IOsocket_::sendmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags)
{
	(void)flags;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) { err = EBADF; return -1; }
	if ( ! dsendring ) dsendring = new ts2io_dgram_ring();
	if ( dsendring->alloc(TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	if ( dsendring->error() ) { err = dsendring->error(); state = TS2IO_ERROR; return -1; }
	{
	unsigned int i = 0;
		for ( ; i < vlen ; i++ ) {
		int n = dsendring->send_produce_iov(msgvec[i].msg_hdr.msg_iov,
				(int)msgvec[i].msg_hdr.msg_iovlen,
				(struct sockaddr*)msgvec[i].msg_hdr.msg_name,
				(int)msgvec[i].msg_hdr.msg_namelen);
			if ( n < 0 ) break;			/* ring full: send the rest next call */
			msgvec[i].msg_len = (unsigned int)n;
		}
		wakeup();				/* kick pump: drain the send ring */
		if ( i > 0 ) return (int)i;		/* number of datagrams queued */
	}
	{
	sPtr<tinyState> c = sCallSection::key->caller();
		while ( write_waiters->del([c](sPtr<tinyState> x){ return x == c ? 1 : 0; }).is_notNull() ) ;
		write_waiters->ins(MAX_INTEGER64, c);
	}
	throw sException([this](sPtr<tinyState> caller){ return dsendring->has_space() || dsendring->error(); });
}

int
ts2IOsocket_::recvmmsg(struct mmsghdr * msgvec,unsigned int vlen,int flags,
			struct timespec * timeout)
{
	(void)flags; (void)timeout;			/* drain is non-blocking; no timed wait */
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) { err = EBADF; return -1; }
	if ( ! drecvring ) drecvring = new ts2io_dgram_ring();
	if ( drecvring->alloc(TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	{
	unsigned int i = 0;
		for ( ; i < vlen ; i++ ) {
		int namelen = (int)msgvec[i].msg_hdr.msg_namelen;
		int n = drecvring->recv_consume_iov(msgvec[i].msg_hdr.msg_iov,
				(int)msgvec[i].msg_hdr.msg_iovlen,
				(struct sockaddr*)msgvec[i].msg_hdr.msg_name,&namelen);
			if ( n < 0 ) break;			/* ring empty: get the rest next call */
			msgvec[i].msg_len = (unsigned int)n;
			msgvec[i].msg_hdr.msg_namelen = namelen;
		}
		if ( i > 0 ) { wakeup(); return (int)i; }	/* number of datagrams received */
	}
	if ( drecvring->error() ) { err = drecvring->error(); state = TS2IO_ERROR; return -1; }
	{
	sPtr<tinyState> c = sCallSection::key->caller();
		while ( read_waiters->del([c](sPtr<tinyState> x){ return x == c ? 1 : 0; }).is_notNull() ) ;
		read_waiters->ins(MAX_INTEGER64, c);
	}
	wakeup();				/* park done → pump posts WSARecvFrom (demand-gated) */
	throw sException([this](sPtr<tinyState> caller){ return drecvring->has_ready() || drecvring->error(); });
}


/* Extend the base pump (ACT_START) with the datagram record-ring: post a
   WSARecvFrom into a free recv slot / a WSASendTo from the oldest send record,
   and wake datagram waiters.  Reuses the base's read_waiters/write_waiters (a UDP
   socket never drives the stream rbuf/wbuf, so the base leaves them alone).
   Returns 1 if datagram I/O is outstanding, OR'd with the base's stream result. */
int
ts2IOsocket_::pump_io()
{
int a = ts2IOdescriptor_::pump_io();
	/* Wake satisfied datagram waiters FIRST (same reasoning as the base: a recv is
	   demand-gated so a speculative WSARecvFrom doesn't pin us alive forever). */
	if ( drecvring && read_waiters->count && (drecvring->has_ready() || drecvring->error()) ) {
	sPtr<stdQueue<tinyState> > wk = read_waiters;
		read_waiters = thNEW(stdQueue<tinyState>,());
		for ( sPtr<tinyState> cc ; (cc = wk->del()) != thNULL ; )
			cc->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	if ( dsendring && write_waiters->count && (dsendring->has_space() || dsendring->error()) ) {
	sPtr<stdQueue<tinyState> > wk = write_waiters;
		write_waiters = thNEW(stdQueue<tinyState>,());
		for ( sPtr<tinyState> cc ; (cc = wk->del()) != thNULL ; )
			cc->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	if ( tpio ) {
		if ( drecvring && read_waiters->count ) {			/* demand-gated recv */
		char * p; DWORD c; struct sockaddr * pa; INT * pal;
			if ( drecvring->recv_reserve(p,c,pa,pal) ) {
				::memset(&drecv_ov,0,sizeof(drecv_ov));
				StartThreadpoolIo(tpio);
				if ( io.is_notNull() ) io->addRefio(ifThis);		/* op posted → keep-alive until io_cb */
			WSABUF wb; wb.buf = p; wb.len = c;
			DWORD got = 0, fl = 0;
				if ( WSARecvFrom((SOCKET)fd,&wb,1,&got,&fl,pa,pal,&drecv_ov,NULL) != 0
						&& WSAGetLastError() != WSA_IO_PENDING ) {
					CancelThreadpoolIo(tpio);
					if ( io.is_notNull() ) io->delRefio(ifThis);
					drecvring->recv_complete(0,WSAGetLastError());
				}
			}
		}
		if ( dsendring ) {						/* sends drain fully then stop */
		char * p; DWORD len; struct sockaddr * pa; INT pal;
			if ( dsendring->send_reserve(p,len,pa,pal) ) {
				::memset(&dsend_ov,0,sizeof(dsend_ov));
				StartThreadpoolIo(tpio);
				if ( io.is_notNull() ) io->addRefio(ifThis);
			WSABUF wb; wb.buf = p; wb.len = len;
			DWORD got = 0;
				if ( WSASendTo((SOCKET)fd,&wb,1,&got,0,pal?pa:NULL,pal,&dsend_ov,NULL) != 0
						&& WSAGetLastError() != WSA_IO_PENDING ) {
					CancelThreadpoolIo(tpio);
					if ( io.is_notNull() ) io->delRefio(ifThis);
					dsendring->send_complete(0,WSAGetLastError());
				}
			}
		}
	}
	return 0;
}

/* pool-thread completion routing: datagram ov first, else the base (stream). */
void
ts2IOsocket_::on_complete(OVERLAPPED * ov,int res,DWORD bytes)
{
	if ( ov == &drecv_ov )		drecvring->recv_complete(bytes,res);
	else if ( ov == &dsend_ov )	dsendring->send_complete(bytes,res);
	else				ts2IOdescriptor_::on_complete(ov,res,bytes);
}

/* graceful teardown: flush the stream wbuf (base) then the datagram send-ring.
   The pending READ-side ops (a read-ahead WSARecvFrom that will never complete
   because no more datagrams arrive) MUST be cancelled BEFORE any drain-wait, else
   the base's WaitForThreadpoolIoCallbacks(FALSE) blocks on them forever.  So cancel
   drecv_ov up front, then let the base cancel read_ov + wait + drain wbuf. */
void
ts2IOsocket_::io_teardown_flush()
{
	if ( tpio )
		CancelIoEx(fd,&drecv_ov);		/* datagram recv: cancel before ANY wait */
	ts2IOdescriptor_::io_teardown_flush();
	if ( ! tpio )
		return;
	while ( dsendring && dsendring->has_ready() && ! dsendring->error() ) {
	char * p; DWORD len; struct sockaddr * pa; INT pal;
		if ( ! dsendring->send_reserve(p,len,pa,pal) )
			break;
		::memset(&dsend_ov,0,sizeof(dsend_ov));
		StartThreadpoolIo(tpio);
		if ( io.is_notNull() ) io->addRefio(ifThis);		/* op posted (balanced by io_cb below) */
	WSABUF wb; wb.buf = p; wb.len = len; DWORD got = 0;
		if ( WSASendTo((SOCKET)fd,&wb,1,&got,0,pal?pa:NULL,pal,&dsend_ov,NULL) != 0
				&& WSAGetLastError() != WSA_IO_PENDING ) {
			CancelThreadpoolIo(tpio);
			if ( io.is_notNull() ) io->delRefio(ifThis);
			dsendring->send_complete(0,WSAGetLastError());
			break;
		}
		WaitForThreadpoolIoCallbacks(tpio,FALSE);
	}
}

/* free datagram rings (after callbacks drained), then the base's stream rings. */
void
ts2IOsocket_::io_free_buffers()
{
	if ( drecvring ) { delete drecvring; drecvring = NULL; }
	if ( dsendring ) { delete dsendring; dsendring = NULL; }
	ts2IOdescriptor_::io_free_buffers();
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
