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
 * (step④).  docs/COOKBOOK.md §6.7.
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
#include	"std2/ts_rio.h"		/* RIO ABI (ENHANCED sockets) */
#include	"ts2/c++/sThreadMutex.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/stdInterval.h"	/* RIO idle keep-alive timer (Phase B) */


/* ---- datagram record ring (step④) ----------------------------------
 * A small ring of fixed-capacity slots that preserves datagram boundaries and
 * the peer address.  ONE overlapped op is in flight per direction (a single
 * drecv_ov / dsend_ov owned by ts2IOsocket); the ring gives read-ahead /
 * write-behind depth.  Crucially the kernel only ever touches a descriptor-owned
 * slot, never the caller's buffer — the v2 lifetime guarantee, now for UDP too.
 * Own sThreadMutex (taken by both the pool thread and the state machine).
 * SLOTS/CAP are tunable; CAP must cover the largest datagram (UDP max 65507).  */
#define TS2IO_DGRAM_CAP		(65536u)
#define TS2IO_DGRAM_SLOTS	(4)
#define TS2IO_RIO_MAX_SLOTS	(256)	/* ceiling for set_rio_ring_slots (bounds RQ/CQ/buffer memory) */
#define TS2IO_RIO_DRAIN_CHUNK	(256)	/* RIODequeueCompletion batch per drain loop (stack array) */

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
	int		recv_depth;	/* cap on concurrently-posted RIO receives (0 = up to n);
					   knob for measuring Phase A (depth 1) vs Phase B (depth n) */
	int		errf;
	/* ENHANCED (RIO) registration: each slot's data buffer + the sl[] array
	   (addr regions) are registered so RIO can reference them by BufferId.  Held
	   NULL/invalid until rio_register(); Phase A. */
	RIO_BUFFERID *	databid;	/* per-slot registered data buffer id */
	RIO_BUFFERID	addrbid;	/* registered sl[] array (addr regions) id */
public:
	ts2io_dgram_ring() : sl(0), n(0), cap(0), head(0), fill(0), posted(0), recv_depth(0), errf(0),
			databid(0), addrbid(RIO_INVALID_BUFFERID) {}

	/* Cap concurrently-posted RIO receives at `d` (0 = up to n).  A measurement knob:
	   depth 1 reproduces Phase A single-posted, depth n is Phase B keep-N-posted. */
	void set_recv_depth(int d) { sThreadMutexHandle __h(mtx); recv_depth = d; }
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

	/* --- ENHANCED (RIO) path (Phase A) -------------------------------
	 * The SAME record ring drives RIO: one op in flight per direction (the
	 * `posted` flag), reserve/complete unchanged.  Only the transport differs —
	 * RIOReceiveEx/RIOSendEx into registered slot buffers instead of
	 * WSARecvFrom/WSASendTo.  register once (after alloc), then rio_recv_reserve
	 * / rio_send_reserve hand out the slot's registered RIO_BUFs.  Must be called
	 * under the same single-op discipline as recv_reserve/send_reserve.        */
	int rio_register(RIO_EXTENSION_FUNCTION_TABLE * t) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || databid )		/* need slots; idempotent */
			return sl ? 0 : -1;
		databid = (RIO_BUFFERID*)::calloc(n,sizeof(RIO_BUFFERID));
		if ( ! databid )
			return -1;
		for ( int i = 0 ; i < n ; i++ ) {
			databid[i] = t->RIORegisterBuffer(sl[i].buf,(DWORD)cap);
			if ( databid[i] == RIO_INVALID_BUFFERID )
				return -1;
		}
		/* one registration covering the contiguous sl[] array → each slot's
		   sockaddr_storage is a sub-slice by offset (landmine (a): the addr
		   RIO_BUF Length must be sizeof(sockaddr_storage)=128, never the real 16,
		   or RIOReceiveEx/SendEx returns WSAEINVAL). */
		addrbid = t->RIORegisterBuffer((PCHAR)sl,(DWORD)((size_t)n * sizeof(slot)));
		if ( addrbid == RIO_INVALID_BUFFERID )
			return -1;
		return 0;
	}
	void rio_deregister(RIO_EXTENSION_FUNCTION_TABLE * t) {
		sThreadMutexHandle __h(mtx);
		if ( databid ) {
			for ( int i = 0 ; i < n ; i++ )
				if ( databid[i] != RIO_INVALID_BUFFERID )
					t->RIODeregisterBuffer(databid[i]);
			::free(databid); databid = 0;
		}
		if ( addrbid != RIO_INVALID_BUFFERID ) {
			t->RIODeregisterBuffer(addrbid); addrbid = RIO_INVALID_BUFFERID;
		}
	}
	/* reserve the NEXT free recv slot for a RIOReceiveEx (data + addr RIO_BUFs).
	   Phase B: `posted` is a COUNT of outstanding receives — the ring keeps every
	   free slot posted (ready + posted <= n).  Call in a loop to post all free slots.
	   Layout: [head,head+fill)=READY, [head+fill,head+fill+posted)=POSTED, rest FREE. */
	int rio_recv_reserve(RIO_BUF & data,RIO_BUF & addr) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || ! databid || errf || (fill + posted) >= n ) return 0;
		if ( recv_depth > 0 && posted >= recv_depth ) return 0;	/* depth knob (measurement) */
	int slot = (head + fill + posted) % n;
		data.BufferId = databid[slot]; data.Offset = 0; data.Length = (ULONG)cap;
		addr.BufferId = addrbid;
		addr.Offset = (ULONG)((char*)&sl[slot].addr - (char*)sl);
		addr.Length = (ULONG)sizeof(sl[slot].addr);		/* 128, landmine (a) */
		posted++;
		return 1;
	}
	/* RIO recv completion (FIFO: the oldest posted receive completes first, so the
	   completing slot is (head+fill)).  RIO does not report the peer addr length, so
	   derive it from the family the kernel wrote into the slot. */
	void rio_recv_complete(DWORD bytes,int res) {
		sThreadMutexHandle __h(mtx);
		if ( posted > 0 ) posted--;			/* one outstanding receive done */
		if ( res == 0 ) {
		slot & s = sl[(head+fill)%n];
			s.len = bytes;
			s.alen = (s.addr.ss_family == AF_INET6)
				? (INT)sizeof(struct sockaddr_in6) : (INT)sizeof(struct sockaddr_in);
			fill++;
		}
		else if ( res == ERROR_OPERATION_ABORTED ) { /* teardown */ }
		else errf = res;
	}
	/* reserve the next produced-but-not-yet-posted send slot for a RIOSendEx (Phase B2:
	   pipeline — `posted` is a COUNT of sends in flight; [head,head+posted)=POSTED
	   (draining), [head+posted,head+fill)=READY).  Call in a loop to post all ready
	   records.  hasaddr=1 if the record carries a destination; addr Length is 128. */
	int rio_send_reserve(RIO_BUF & data,RIO_BUF & addr,int & hasaddr) {
		sThreadMutexHandle __h(mtx);
		if ( ! sl || ! databid || errf || posted >= fill ) return 0;
	int idx = (head + posted) % n;
	slot & s = sl[idx];
		data.BufferId = databid[idx]; data.Offset = 0; data.Length = (ULONG)s.len;
		hasaddr = s.alen > 0 ? 1 : 0;
		addr.BufferId = addrbid;
		addr.Offset = (ULONG)((char*)&sl[idx].addr - (char*)sl);
		addr.Length = (ULONG)sizeof(sl[idx].addr);		/* 128, landmine (a) */
		posted++;
		return 1;
	}
	/* RIO send completion (FIFO: oldest posted at head completes first).  Consume the
	   completed slot whatever the result (the send is done, in flight no more). */
	void rio_send_complete(DWORD bytes,int res) {
		(void)bytes;
		sThreadMutexHandle __h(mtx);
		if ( posted > 0 ) posted--;
		if ( res != 0 && res != ERROR_OPERATION_ABORTED ) errf = res;
		if ( fill > 0 ) { head = (head+1)%n; fill--; }
	}
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
	/** @brief ENHANCED(RIO) 受信の同時 post 深さを制限する (計測用ノブ)。0=無制限(=n)。
	 *  / Cap concurrently-posted RIO receives (measurement knob); 0 = up to the ring size.
	 *  depth 1 = Phase A single-posted, depth n = Phase B keep-N-posted.  No effect on the
	 *  plain overlapped path or on non-Windows arches. */
	void set_rio_recv_depth(int depth);
	/** @brief ENHANCED(RIO) リングのスロット数を設定する (計測用ノブ)。既定=TS2IO_DGRAM_SLOTS(4)。
	 *  posted receive の上限＝スロット数なので、深くすると1通知あたりの datagram 数が増え、通知/再武装
	 *  コストが償却される。ensure_rio(遅延生成)の前に呼ぶこと。plain / 非Windows では無効。 */
	void set_rio_ring_slots(int slots);
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

	/** @brief RIO のアイドル解放タイムアウトを設定する (マイクロ秒)。/ Set the RIO idle
	 *  keep-alive timeout (microseconds).  After the last recvfrom/sendto and with no
	 *  reader/writer parked, the socket holds a reactor keep-alive for this long, then
	 *  releases it so an idle app can shut down.  Receives stay posted (drop-avoidance);
	 *  a new request re-acquires the keep-alive cheaply.  Default TS2IO_RIO_IDLE_US. */
	void		set_rio_idle_us(int us);
	/* --- ENHANCED (RIO) datagram path (Phase B: keep every recv slot posted so
	   a datagram is never dropped in the completion→re-post window; a RIO socket does
	   no overlapped I/O).  Completions arrive on this socket's own RIO_CQ, delivered into
	   the state machine by the modern threadpool (RIO_EVENT_COMPLETION + CreateThreadpoolWait
	   → rio_tpwait_cb → rio_drain → wakeup → re-arm), COOKBOOK §6.7.  The always-posted
	   receives do NOT hold a reactor keep-alive; a
	   single "active" refio does, held while a reader/writer is parked OR within the
	   idle timeout of the last request — so an idle socket reclaims (base rule). */
	int		enhanced;	/* runtime: 1 = datagram data I/O via RIO, 0 = plain overlapped.
					   ts2IOsockUDP sets 1 (RIO default); degraded to 0 at first I/O
					   if RIO is unavailable (Win7 / wine / TS2_DISABLE_RIO). */
	int		rio_ready;	/* CQ/RQ/buffers built + wait registered */
	int		rio_armed;	/* 1 while a RIONotify is outstanding (avoid WSAEALREADY) */
	int		rio_active;	/* 1 while we hold the single "active" keep-alive refio */
	int		rio_idle_us;	/* idle keep-alive timeout (µs); see set_rio_idle_us */
	int		rio_recv_depth;	/* concurrently-posted recv cap (0=n); measurement knob */
	int		rio_slots;	/* RIO ring slot count = posted-depth ceiling; measurement knob */
	INTEGER64	rio_deadline;	/* release the active refio once now() passes this (µs) */
	INTEGER64	rio_timer_deadline;	/* when the last-armed idle timer will fire (µs) */
	RIO_EXTENSION_FUNCTION_TABLE *	rio_t;	/* process-global RIO fn table (borrowed) */
	RIO_CQ		rio_cq;		/* this socket's completion queue (EVENT-signalled) */
	RIO_RQ		rio_rq;		/* this socket's request queue on rio_cq */
	HANDLE		rio_event;	/* CQ notification event (auto-reset) */
	PTP_WAIT	rio_tpwait;	/* CreateThreadpoolWait handle (modern pool) — the delivery bridge */
	int		rio_closing;	/* teardown guard: stop the callback re-arming SetThreadpoolWait */
	int		ensure_rio();	/* build CQ/RQ/buffers + start delivery; 0 ok, 1 RIO-unavailable (degrade), -1 error */
	int		rio_drain();	/* dequeue+process all ready completions into the rings; returns count */
	void		rio_on_notify();	/* callback body: drain + re-arm RIONotify + wakeup */
	void		rio_touch();	/* a request arrived: (re)acquire the active refio + push deadline */
	void		rio_idle_check();	/* release the active refio once idle past the timeout */
	int		rio_pump();	/* keep every recv slot posted + drain the send ring */
	void		rio_teardown();	/* drain wait + close CQ + deregister (mtx外/TS_THREAD) */
	static VOID CALLBACK rio_tpwait_cb(PTP_CALLBACK_INSTANCE,PVOID,PTP_WAIT,TP_WAIT_RESULT);	/* modern pool */

	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE

#endif


/* ENHANCED (RIO) state shared init; the object memory is zeroed by the tinyState
   allocator, but be explicit about the RIO handles.  enhanced is set later by the
   derived ts2IOsockUDP from its creation mode (default 0 = plain overlapped). */
/* Default RIO idle keep-alive timeout (µs): how long after the last recvfrom/sendto
   (with no reader/writer parked) the socket keeps the reactor alive before releasing
   it so an idle app can exit.  Short is fine — re-acquiring is cheap and the receives
   stay posted regardless.  Override per-socket with set_rio_idle_us(). */
#define TS2IO_RIO_IDLE_US	(2000)		/* 2 ms */

#define TS2IO_RIO_CTOR_INIT \
    drecvring = 0; \
    dsendring = 0; \
    enhanced = 0; \
    rio_ready = 0; \
    rio_armed = 0; \
    rio_active = 0; \
    rio_idle_us = TS2IO_RIO_IDLE_US; \
    rio_recv_depth = 0; \
    rio_slots = TS2IO_DGRAM_SLOTS; \
    rio_deadline = 0; \
    rio_timer_deadline = 0; \
    rio_t = 0; \
    rio_cq = RIO_INVALID_CQ; \
    rio_rq = RIO_INVALID_RQ; \
    rio_event = NULL; \
    rio_tpwait = NULL; \
    rio_closing = 0;

ts2IOsocket_::ts2IOsocket_(TS_ARGS0)
        : ts2IOdescriptor_(parent)
{
    TS_CPARGS0
    TS2IO_RIO_CTOR_INIT
}

ts2IOsocket_::ts2IOsocket_(TS_ARGS1)
        : ts2IOdescriptor_(parent,_fd)
{
    TS_CPARGS1
    TS2IO_RIO_CTOR_INIT
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/

/* Datagram send/recv now run on the SAME internal-ring + threadpool machinery as
   the base stream path (step④): sendto copies the caller's datagram into a
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
	if ( dsendring->alloc(enhanced?rio_slots:TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( enhanced ) {						/* datagram default: RIO, degrade to plain if unavailable */
		int r = ensure_rio();
		if ( r > 0 )		enhanced = 0;			/* RIO unavailable here (Win7 / wine) → plain */
		else if ( r < 0 )	return -1;			/* genuine error (state already set) */
		else			rio_touch();			/* RIO active; no threadpool I/O, mark keep-alive */
	}
	if ( ! enhanced && ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
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
	if ( drecvring->alloc(enhanced?rio_slots:TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( enhanced ) {						/* datagram default: RIO, degrade to plain if unavailable */
		int r = ensure_rio();
		if ( r > 0 )		enhanced = 0;			/* RIO unavailable here (Win7 / wine) → plain */
		else if ( r < 0 )	return -1;			/* genuine error (state already set) */
		else			rio_touch();			/* RIO active; no threadpool I/O, mark keep-alive */
	}
	if ( ! enhanced && ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
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
 * sendto/recvfrom (step④).  Windows has no batched datagram syscall, so a
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
	if ( dsendring->alloc(enhanced?rio_slots:TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( enhanced ) {						/* datagram default: RIO, degrade to plain if unavailable */
		int r = ensure_rio();
		if ( r > 0 )		enhanced = 0;			/* RIO unavailable here (Win7 / wine) → plain */
		else if ( r < 0 )	return -1;			/* genuine error (state already set) */
		else			rio_touch();			/* RIO active; no threadpool I/O, mark keep-alive */
	}
	if ( ! enhanced && ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
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
	if ( drecvring->alloc(enhanced?rio_slots:TS2IO_DGRAM_SLOTS,TS2IO_DGRAM_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( enhanced ) {						/* datagram default: RIO, degrade to plain if unavailable */
		int r = ensure_rio();
		if ( r > 0 )		enhanced = 0;			/* RIO unavailable here (Win7 / wine) → plain */
		else if ( r < 0 )	return -1;			/* genuine error (state already set) */
		else			rio_touch();			/* RIO active; no threadpool I/O, mark keep-alive */
	}
	if ( ! enhanced && ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
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
	if ( ! enhanced )
		ts2IOdescriptor_::pump_io();	/* base: stream waiters + demand-gated stream read (op-tied refio) */
	/* Wake satisfied datagram waiters FIRST — demand-gated: a recv is posted only for
	   a still-parked reader, so no speculative recv (WSARecvFrom/RIOReceiveEx) lingers
	   pinned and blocks shutdown (keep-alive fix). */
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
	if ( enhanced ) {
		rio_pump();			/* RIOReceiveEx (demand-gated) / RIOSendEx; op-tied refio inside */
	} else if ( tpio ) {
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
	ENHANCED (RIO) PATH  (Phase A)
********************************************/

/* Process-global RIO extension function table.  The pointers are valid process-
   wide once fetched from any RIO socket, so fetch once (lazily, under app-mtx via
   ensure_rio) and share.  Windows-port throughput step #2. */
static RIO_EXTENSION_FUNCTION_TABLE	g_ts2_rio_fn;
static int				g_ts2_rio_fn_ready = 0;

static RIO_EXTENSION_FUNCTION_TABLE *
ts2_rio_table(SOCKET s)
{
	if ( g_ts2_rio_fn_ready )
		return &g_ts2_rio_fn;
GUID g = WSAID_MULTIPLE_RIO;
DWORD got = 0;
	::memset(&g_ts2_rio_fn,0,sizeof(g_ts2_rio_fn));
	g_ts2_rio_fn.cbSize = sizeof(g_ts2_rio_fn);
	if ( WSAIoctl(s,SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER,&g,sizeof(g),
			&g_ts2_rio_fn,sizeof(g_ts2_rio_fn),&got,NULL,NULL) != 0 )
		return NULL;
	g_ts2_rio_fn_ready = 1;
	return &g_ts2_rio_fn;
}

/* Build this socket's RIO state: fetch the fn table, alloc + register both rings,
   create an EVENT-signalled CQ + a threadpool-wait bridge + this socket's RQ, then
   arm the first notification.  Called at ENHANCED INI (app-mtx) so a datagram
   arriving before the app's first recv is not dropped — the pump posts a receive as
   soon as ACT_START runs.  Idempotent.  Returns 0 = ok, 1 = RIO unavailable (Win7 / wine —
   caller degrades to plain, no error state), -1 = genuine error (err/state set). */
int
ts2IOsocket_::ensure_rio()
{
	if ( rio_ready )
		return 0;
	if ( fd == INVALID_HANDLE_VALUE ) { err = EBADF; state = TS2IO_ERROR; return -1; }
	rio_t = ts2_rio_table((SOCKET)fd);
	if ( ! rio_t ) return 1;		/* RIO unavailable (Win7 / wine) → caller degrades to plain, NOT an error */
	/* both rings exist + registered up front (recv must be postable immediately). */
	if ( ! drecvring ) drecvring = new ts2io_dgram_ring();
	if ( ! dsendring ) dsendring = new ts2io_dgram_ring();
	if ( drecvring->alloc(rio_slots,TS2IO_DGRAM_CAP) < 0
			|| dsendring->alloc(rio_slots,TS2IO_DGRAM_CAP) < 0 ) {
		err = ENOMEM; state = TS2IO_ERROR; return -1;
	}
	if ( drecvring->rio_register(rio_t) < 0 || dsendring->rio_register(rio_t) < 0 ) {
		err = WSAGetLastError(); state = TS2IO_ERROR; return -1;
	}
	drecvring->set_recv_depth(rio_recv_depth);		/* apply the measurement knob */
	/* Completion delivery: event-signalled CQ + CreateThreadpoolWait (the same modern
	   threadpool as the base CreateThreadpoolIo) + RIONotify; the callback re-arms via
	   SetThreadpoolWait.  This modern-pool bridge roughly doubled loopback throughput
	   over the legacy RegisterWaitForSingleObject it replaced (measured 2026-07-21). */
	rio_event = CreateEventW(NULL,FALSE,FALSE,NULL);		/* auto-reset */
	if ( ! rio_event ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	{
	RIO_NOTIFICATION_COMPLETION nc;
		::memset(&nc,0,sizeof(nc));
		nc.Type              = RIO_EVENT_COMPLETION;
		nc.Event.EventHandle = rio_event;
		nc.Event.NotifyReset = TRUE;
		rio_cq = rio_t->RIOCreateCompletionQueue((DWORD)(2*rio_slots + 16),&nc);
	}
	if ( rio_cq == RIO_INVALID_CQ ) { err = WSAGetLastError(); state = TS2IO_ERROR; return -1; }
	rio_rq = rio_t->RIOCreateRequestQueue((SOCKET)fd,
			(ULONG)rio_slots,1, (ULONG)rio_slots,1,
			rio_cq,rio_cq,(PVOID)this);
	if ( rio_rq == RIO_INVALID_RQ ) { err = WSAGetLastError(); state = TS2IO_ERROR; return -1; }
	rio_ready = 1;
	rio_tpwait = CreateThreadpoolWait(rio_tpwait_cb,this,NULL);
	if ( ! rio_tpwait ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	SetThreadpoolWait(rio_tpwait,rio_event,NULL);		/* arm for the first signal */
	if ( rio_t->RIONotify(rio_cq) == 0 )			/* NO_ERROR: one notification armed */
		rio_armed = 1;
	/* Phase B: the pump keeps every free recv slot posted (rio_pump), so a datagram is
	   not dropped in the completion→re-post window.  These receives do NOT hold refio
	   (that would pin an idle socket alive — RIO has no per-op cancel); the single
	   "active" refio + rio_idle_check govern lifetime.  ensure_rio is called lazily on
	   the first recvfrom/sendto, so a never-used enhanced socket builds nothing and
	   auto-destructs (ts2IO lifecycle contract).  Still: a datagram arriving before the
	   first recvfrom (no receive posted yet) is dropped — inherent to lazy activation. */
	wakeup();					/* let ACT_START run (post receives + send) */
	return 0;
}

/* A request (recvfrom/sendto/…) arrived: (re)acquire the single "active" reactor
   keep-alive and push the idle deadline out.  The always-posted receives do NOT hold
   refio themselves — this one refio (held while active) is what keeps an actively-used
   RIO socket alive; rio_idle_check drops it once idle. */
void
ts2IOsocket_::rio_touch()
{
	rio_deadline = stdInterval::now() + (INTEGER64)rio_idle_us;
	if ( ! rio_active ) {
		if ( io.is_notNull() ) io->addRefio(ifThis);
		rio_active = 1;
	}
}

/* Drop the active keep-alive once the socket is idle: no reader/writer parked AND the
   idle timeout has elapsed since the last request.  While a reader/writer is parked we
   keep the refio unconditionally (base rule: an outstanding request keeps us alive).
   Runs in the pump (app-mtx); ev-blind, so it re-arms an idle timer via now()/deadline
   comparison rather than reacting to the timer event — an extra timer tick just re-runs
   this harmlessly. */
void
ts2IOsocket_::rio_idle_check()
{
	if ( ! rio_active )
		return;
	{
	int busy = read_waiters->count || write_waiters->count;
	INTEGER64 now = stdInterval::now();
		if ( ! busy && now >= rio_deadline ) {
			if ( io.is_notNull() ) io->delRefio(ifThis);	/* idle → let the app shut down */
			rio_active = 0;
			return;
		}
		/* still active: ensure a wakeup lands near the deadline so we re-check.  Re-arm
		   only when the previously-armed timer has (about) expired, to bound timer churn. */
		if ( ! busy && now >= rio_timer_deadline ) {
			stdInterval::wait(ifThis,(INTEGER64)rio_idle_us,TSE_TIMER);
			rio_timer_deadline = now + (INTEGER64)rio_idle_us;
		}
	}
}

void
ts2IOsocket_::set_rio_idle_us(int us)
{
	rio_idle_us = us > 0 ? us : TS2IO_RIO_IDLE_US;
}

/* Measurement knob: cap the number of concurrently-posted RIO receives.  Stored now and
   applied when ensure_rio builds the ring (lazy), or immediately if the ring already
   exists.  depth 1 = Phase A single-posted; depth >= TS2IO_DGRAM_SLOTS (or 0) = Phase B. */
void
ts2IOsocket_::set_rio_recv_depth(int depth)
{
	rio_recv_depth = depth;
	if ( drecvring )
		drecvring->set_recv_depth(depth);
}

/* Measurement knob: RIO ring slot count = the ceiling on concurrently-posted receives/sends.
   Deeper ring → more datagrams harvested per RIONotify → the notification + re-arm cost is
   amortized over more work.  Must be set before ensure_rio builds the ring (lazy).  Clamped
   to [1, TS2IO_RIO_MAX_SLOTS]; 0/negative resets to the default. */
void
ts2IOsocket_::set_rio_ring_slots(int slots)
{
	rio_slots = (slots > 0) ? (slots > TS2IO_RIO_MAX_SLOTS ? TS2IO_RIO_MAX_SLOTS : slots)
				: TS2IO_DGRAM_SLOTS;
}

/* pump (app-mtx): keep EVERY free recv slot posted (Phase B — a datagram is never
   dropped in the completion→re-post window), and drain the send ring.  The posted
   receives do NOT hold a keep-alive refio (that would pin an idle socket alive and
   block shutdown, since RIO has no per-op cancel); the single active refio +
   rio_idle_check govern lifetime instead.  Completions arrive via rio_drain. */
int
ts2IOsocket_::rio_pump()
{
	if ( ! rio_ready )
		return 0;
	if ( drecvring ) {					/* keep every free recv slot posted */
	RIO_BUF db, ab;
		while ( drecvring->rio_recv_reserve(db,ab) ) {
			if ( ! rio_t->RIOReceiveEx(rio_rq,&db,1,NULL,&ab,NULL,NULL,0,
					(PVOID)(INT_PTR)0 /* dir=recv */) ) {
				drecvring->rio_recv_complete(0,WSAGetLastError());
				break;				/* stop posting on failure */
			}
		}
	}
	if ( dsendring ) {					/* B2: pipeline — post every ready send */
	RIO_BUF db, ab; int hasaddr = 0;
		while ( dsendring->rio_send_reserve(db,ab,hasaddr) ) {
			if ( ! rio_t->RIOSendEx(rio_rq,&db,1,NULL,(hasaddr?&ab:NULL),NULL,NULL,0,
					(PVOID)(INT_PTR)1 /* dir=send */) ) {
				dsendring->rio_send_complete(0,WSAGetLastError());
				break;				/* stop posting on failure */
			}
		}
	}
	rio_idle_check();					/* release the active refio once idle */
	return 0;
}

/* CQ drain (foreign delivery thread): dequeue every ready completion into the rings under
   their own mutex; returns the number processed.  Phase B: the always-posted receives do
   NOT hold a keep-alive refio, so there is no per-op delRefio here — `self` is kept alive
   across the callback by the OWNER's reference (the ts2IO lifecycle contract: a RIO socket
   that did I/O must be destroy()'d, and teardown drains the delivery thread before freeing,
   so no completion touches a freed object).  RequestContext is the direction (0=recv,1=send);
   receives complete FIFO so rio_recv_complete converts the oldest posted slot. */
int
ts2IOsocket_::rio_drain()
{
RIO_EXTENSION_FUNCTION_TABLE * t = rio_t;
	if ( ! t )
		return 0;
	{
	RIORESULT res[TS2IO_RIO_DRAIN_CHUNK];	/* drain up to CHUNK per RIODequeueCompletion; loop until empty */
	const ULONG cap = (ULONG)(sizeof(res)/sizeof(res[0]));
	int total = 0;
		for ( ;; ) {
		ULONG got = t->RIODequeueCompletion(rio_cq,res,cap);
			if ( got == 0 || got == RIO_CORRUPT_CQ )
				break;
			for ( ULONG i = 0 ; i < got ; i++ ) {
			int dir = (int)((INT_PTR)res[i].RequestContext & 1);
				if ( dir == 0 ) drecvring->rio_recv_complete(res[i].BytesTransferred,(int)res[i].Status);
				else            dsendring->rio_send_complete(res[i].BytesTransferred,(int)res[i].Status);
			}
			total += (int)got;
			if ( got < cap )
				break;
		}
		return total;
	}
}

/* modern-notify body: drain, re-arm RIONotify, wakeup (pump re-posts + wakes waiters,
   inline under app-mtx).  COOKBOOK §6.7. */
void
ts2IOsocket_::rio_on_notify()
{
	if ( ! rio_t )
		return;
	rio_armed = 0;					/* the delivered notification is consumed */
	rio_drain();
	if ( rio_t->RIONotify(rio_cq) == 0 )		/* re-arm the CQ→event notification */
		rio_armed = 1;
	wakeup();
}

/* modern CreateThreadpoolWait callback (one-shot): drain, then re-arm the wait for the next
   signal unless we are tearing down (rio_closing).  Same modern threadpool as the base
   CreateThreadpoolIo path — the default RIO completion delivery. */
VOID CALLBACK
ts2IOsocket_::rio_tpwait_cb(PTP_CALLBACK_INSTANCE,PVOID ctx,PTP_WAIT,TP_WAIT_RESULT)
{
ts2IOsocket_ * self = (ts2IOsocket_*)ctx;
	self->rio_on_notify();
	if ( ! self->rio_closing )
		SetThreadpoolWait(self->rio_tpwait,self->rio_event,NULL);	/* re-arm */
}

/* RIO teardown (mtx外/TS_THREAD): drain the delivery so no completion can fire, then close
   the CQ and deregister buffers before the base frees the rings / closes the socket.  Set
   rio_closing (the callback stops re-arming), then WaitForThreadpoolWaitCallbacks(TRUE) waits
   for a running callback + cancels a pending (possibly just-re-armed) wait.  Runs mtx-released
   (TS_THREAD) so the wait can't deadlock the wakeup app-mtx.  COOKBOOK §6.7 drain table. */
void
ts2IOsocket_::rio_teardown()
{
	if ( rio_tpwait ) {
		rio_closing = 1;				/* stop the callback re-arming */
		WaitForThreadpoolWaitCallbacks(rio_tpwait,TRUE);	/* wait running + cancel pending */
		CloseThreadpoolWait(rio_tpwait);
		rio_tpwait = NULL;
	}
	/* Release the single "active" keep-alive if still held (Phase B: the always-posted
	   receives never held refio, so there is no per-op pin to balance here — just the
	   one active refio, if a request was recent when destroy hit). */
	if ( rio_active ) {
		if ( io.is_notNull() ) io->delRefio(ifThis);
		rio_active = 0;
	}
	if ( rio_t ) {
		if ( drecvring ) drecvring->rio_deregister(rio_t);
		if ( dsendring ) dsendring->rio_deregister(rio_t);
		if ( rio_cq != RIO_INVALID_CQ ) {
			rio_t->RIOCloseCompletionQueue(rio_cq);
			rio_cq = RIO_INVALID_CQ;
		}
	}
	rio_rq = RIO_INVALID_RQ;					/* released with the socket */
	if ( rio_event ) { CloseHandle(rio_event); rio_event = NULL; }
	rio_ready = 0;
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
	/* ENHANCED (RIO): do NOT build RIO here.  Under the demand-gated model nothing is
	   posted until the first recvfrom/sendto (each calls ensure_rio() lazily), so an
	   eager build would only waste a CQ/RQ + buffer registration on a socket that may
	   never be used — and it would break the "a never-called ts2IO auto-destructs"
	   contract (see ts2IO doxygen): a never-recvfrom'd enhanced socket must build
	   nothing and reclaim like any plain socket.  (rio_teardown is a no-op when
	   ensure_rio never ran, so FIN stays clean.) */
	return rDO|INI_ts2IOdescriptor_START;
}

TS_STATE(FIN_START)
{
	return rDO|FIN_ts2IOsocket_START;
}
/* ENHANCED teardown runs in a thread (mtx released) because draining the delivery
   (WaitForThreadpoolWaitCallbacks) blocks for an in-flight completion; then chain to the
   base FIN thread, which (tpio is
   NULL for a RIO socket) just closes the socket and frees the now-safe rings. */
TS_THREAD(FIN_ts2IOsocket_START)
{
	if ( enhanced )
		rio_teardown();
	return rDO|FIN_ts2IOdescriptor_START;
}
