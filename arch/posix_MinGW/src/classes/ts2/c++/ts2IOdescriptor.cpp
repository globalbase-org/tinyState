/*
 * ts2IOdescriptor — MinGW (HANDLE) implementation.  Windows-port design memo §C.
 *
 * Base read()/write() use an INTERNAL ring buffer driven by threadpool I/O
 * (CreateThreadpoolIo).  The kernel only ever touches the descriptor-owned ring
 * (rbuf/wbuf) — never the caller's buffer — so a caller that abandons a read and
 * frees its buffer can never be scribbled on by a late completion (the overlapped
 * lifetime hazard).  read()/write() copy synchronously between the caller buffer
 * and the ring and return partial (POSIX contract, 1 call = self-contained).
 *
 * Orchestration lives entirely in the object's own state machine (ACT_START =
 * pump): read()/write() and the io_cb completion callback all just poke the ring
 * and this->wakeup(); the pump posts the next I/O and wakes waiters.  The foreign
 * threadpool callback never touches application->mtx nor the state machine
 * directly — see docs/COOKBOOK.md §6.7.  Teardown drains outstanding callbacks
 * with WaitForThreadpoolIoCallbacks (TS_THREAD FIN) so the ring is freed only when
 * no callback can touch it.
 *
 * fwIO is retained only as `io`, used solely for reactor keep-alive
 * (addRefio/delRefio).  The old per-descriptor overlapped/IOCP members
 * (fdid/associated/read_pending/write_pending/ensure_assoc) are gone — sockets use
 * the threadpool record ring and ts2IOwinConsole bridges its readiness wait through
 * wakeup(), so nothing drives fwIO-overlapped ops through this class
 * any more.  (ts2IOsockServer's accept path keeps its OWN fdid + fwIO registration.)
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	<stdio.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<string.h>
#include	"ts2/c++/sThreadMutex.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/stdEvent.h"


/* ---- internal thread-safe byte ring ---------------------------------------
 * One instance per direction.  The Ring's own mutex (NOT application->mtx)
 * protects buf/head/valid/posted/eof/err; it is taken by both the pool thread
 * (io_cb -> complete_*) and the state-machine thread (read/write/pump).  The
 * region [head+valid, +posted) (read) / [head, +posted) (write) is OS-owned
 * while posted>0; nobody else touches it until complete_* clears posted.        */
class ts2io_ring {
	sThreadMutex	mtx;
	char *		buf;
	DWORD		cap, head, valid, posted;
	int		eofF, errF;
public:
	ts2io_ring() : buf(0), cap(0), head(0), valid(0), posted(0), eofF(0), errF(0) {}
	~ts2io_ring() { if ( buf ) ::free(buf); }

	int alloc(DWORD c) {
		sThreadMutexHandle __h(mtx);
		if ( buf ) return 0;
		buf = (char*)::malloc(c);
		if ( ! buf ) return -1;
		cap = c;
		return 0;
	}
	void free_buf() { sThreadMutexHandle __h(mtx); if ( buf ) { ::free(buf); buf = 0; } }

	/* --- READ ring: OS fills [(head+valid), +posted); consumer reads [head, +valid) --- */
	DWORD copy_out(void * dst, DWORD len) {
		sThreadMutexHandle __h(mtx);
		if ( ! buf || valid == 0 ) return 0;
		DWORD n = valid < len ? valid : len;
		DWORD first = cap - head; if ( first > n ) first = n;
		::memcpy(dst, buf + head, first);
		if ( n > first ) ::memcpy((char*)dst + first, buf, n - first);
		head = (head + n) % cap; valid -= n;
		return n;
	}
	int reserve_fill(char *& p, DWORD & n) {           /* 1 => issue ReadFile into p[0..n) */
		sThreadMutexHandle __h(mtx);
		if ( ! buf || posted || eofF || errF ) return 0;
		DWORD fill = (head + valid) % cap;
		DWORD cfree = cap - fill;                  /* contiguous-to-end */
		DWORD tfree = cap - valid;                 /* total free (posted==0 here) */
		if ( cfree > tfree ) cfree = tfree;
		if ( cfree == 0 ) return 0;
		p = buf + fill; n = cfree; posted = cfree;
		return 1;
	}
	void complete_fill(DWORD bytes, int res) {
		sThreadMutexHandle __h(mtx);
		posted = 0;
		if ( res == 0 ) { if ( bytes ) valid += bytes; else eofF = 1; }
		else if ( res == ERROR_OPERATION_ABORTED ) { /* teardown */ }
		else if ( res == ERROR_HANDLE_EOF || res == ERROR_BROKEN_PIPE ) eofF = 1;
		else errF = res;
	}

	/* --- WRITE ring: OS drains [head, +posted); producer appends into free --- */
	DWORD copy_in(const void * src, DWORD len) {
		sThreadMutexHandle __h(mtx);
		if ( ! buf ) return 0;
		DWORD tfree = cap - valid;
		DWORD n = tfree < len ? tfree : len;
		if ( n == 0 ) return 0;
		DWORD fill = (head + valid) % cap;
		DWORD first = cap - fill; if ( first > n ) first = n;
		::memcpy(buf + fill, src, first);
		if ( n > first ) ::memcpy(buf, (const char*)src + first, n - first);
		valid += n;
		return n;
	}
	int reserve_drain(char *& p, DWORD & n) {          /* 1 => issue WriteFile from p[0..n) */
		sThreadMutexHandle __h(mtx);
		if ( ! buf || posted || errF || valid == 0 ) return 0;
		DWORD cdata = cap - head; if ( cdata > valid ) cdata = valid;
		p = buf + head; n = cdata; posted = cdata;
		return 1;
	}
	void complete_drain(DWORD bytes, int res) {
		sThreadMutexHandle __h(mtx);
		posted = 0;
		if ( res == 0 ) { head = (head + bytes) % cap; valid -= bytes; }
		else if ( res == ERROR_OPERATION_ABORTED ) { /* teardown */ }
		else errF = res;
	}

	int has_data()  { sThreadMutexHandle __h(mtx); return valid > 0; }
	int has_space() { sThreadMutexHandle __h(mtx); return buf && (cap - valid) > 0; }
	int posted_now(){ sThreadMutexHandle __h(mtx); return posted != 0; }
	int is_eof()    { sThreadMutexHandle __h(mtx); return eofF; }
	int error()     { sThreadMutexHandle __h(mtx); return errF; }
};

#define TS2IO_RING_CAP	(64u * 1024u)


#include	"_ts2/c++/ts2IOdescriptor_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOdescriptor,ts2/c++/ts2IO)

/* process-wide fwIO/IOCP completion-key allocator.  Draws unique keys shared by the
   fwIO users that still register with the completion port (ts2IOsockServer's accept,
   ts2System's child-exit wait), so their keys can never collide. */
static volatile LONG	g_ts2io_fdid = 0;

/* shared key allocator (declared in ts2IOdescriptor.h). */
int ts2io_alloc_key() { return (int)InterlockedIncrement(&g_ts2io_fdid); }

#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief HANDLE を ts2IO に包む具象クラス (MinGW)。/ Concrete ts2IO wrapping a Win32 HANDLE.
 * @details base read/write は内部リング + threadpool I/O。datagram(ts2IOsocket)/console は
 * 旧 overlapped+fwIO 経路を継承利用。docs/COOKBOOK.md §6.7。
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2IOdescriptor_(
		sPtr<tinyState> parent);
	void inherit(
		sPtr<tinyState> parent);
	/** @brief 既存の HANDLE を渡して作成 (ts2System のパイプ端など)。 */
	ts2IOdescriptor_(
		sPtr<tinyState> parent,
		HANDLE _fd);
	void inherit(
		sPtr<tinyState> parent,
		HANDLE _fd);

	virtual int read(void * buf,int length);
	virtual int write(void * buf,int length);
	virtual INTEGER64 seek(INTEGER64 pos,int where);
protected:
	/** @brief 管理する HANDLE。/ The managed HANDLE. */
	HANDLE		fd;

	/* --- fwIO handle: used only for reactor keep-alive (addRefio/delRefio); no
	   descriptor drives fwIO-overlapped ops through this class any more. --- */
	sPtr<fwIO>	io;

	/* --- base internal-buffer + threadpool I/O path (pipe / TCP stream / datagram) --- */
	OVERLAPPED	read_ov;	/* base stream ReadFile overlapped */
	OVERLAPPED	write_ov;	/* base stream WriteFile overlapped */
	PTP_IO		tpio;
	int		closing;
	ts2io_ring *	rbuf;
	ts2io_ring *	wbuf;
	int		io_ref;		/* ts2IOwinConsole-only: 1 while its readiness wait holds a keep-alive refio (base/socket use the op-tied refio instead) */
	sPtr<stdQueue<tinyState> >	read_waiters;
	sPtr<stdQueue<tinyState> >	write_waiters;
	int		ensure_tpio();
	static VOID CALLBACK io_cb(PTP_CALLBACK_INSTANCE,PVOID,PVOID,ULONG,ULONG_PTR,PTP_IO);
	/* virtual hooks: subclasses (datagram sockets) share this threadpool + pump +
	   keep-alive machinery.  Base implements the byte-stream (rbuf/wbuf) path;
	   ts2IOsocket adds a datagram record-ring path over the SAME tpio / io_cb. */
	virtual int  pump_io();		/* post pending I/O + wake waiters; 1 if I/O outstanding/expected */
	virtual void on_complete(OVERLAPPED * ov,int res,DWORD bytes);	/* pool-thread completion routing */
	virtual void io_teardown_flush();	/* graceful: drain write-behind before close (mtx外/TS_THREAD) */
	virtual void io_free_buffers();		/* free rings AFTER callbacks are drained */
};

TS_END_IMPLEMENT

#endif


ts2IOdescriptor_::ts2IOdescriptor_(
		sPtr<tinyState> _parent)
        : ts2IO_(_parent)
{
	fd = INVALID_HANDLE_VALUE;
	tpio = NULL;
	closing = 0;
	rbuf = wbuf = NULL;
	io_ref = 0;
}

void
ts2IOdescriptor_::inherit(
	sPtr<tinyState> _parent)
{
	this->TS_BASECLASS::inherit(_parent);
}

ts2IOdescriptor_::ts2IOdescriptor_(
		sPtr<tinyState> _parent,HANDLE _fd)
        : ts2IO_(_parent)
{
	fd = _fd;
	tpio = NULL;
	closing = 0;
	rbuf = wbuf = NULL;
	io_ref = 0;
}

void
ts2IOdescriptor_::inherit(
	sPtr<tinyState> _parent,HANDLE _fd)
{
	this->TS_BASECLASS::inherit(_parent);
}


/* lazily bind fd to the threadpool for base read()/write() I/O completion. */
int
ts2IOdescriptor_::ensure_tpio()
{
	if ( tpio )
		return 0;
	if ( fd == INVALID_HANDLE_VALUE )
		return -1;
	tpio = CreateThreadpoolIo(fd, io_cb, this, NULL);
	return tpio ? 0 : -1;
}


/*******************************************
	INSTANCE FUNCTIONS  (base: internal ring + threadpool)
********************************************/

int
ts2IOdescriptor_::read(void * buf,int length)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) { err = EBADF; return -1; }
	if ( ! rbuf ) rbuf = new ts2io_ring();
	if ( rbuf->alloc(TS2IO_RING_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	{
	DWORD n = rbuf->copy_out(buf,(DWORD)length);
		if ( n > 0 ) { wakeup(); return (int)n; }	/* partial-immediate, self-contained */
	}
	if ( rbuf->is_eof() ) return 0;
	if ( rbuf->error() ) { err = rbuf->error(); state = TS2IO_ERROR; return -1; }
	{
	sPtr<tinyState> c = sCallSection::key->caller();
		while ( read_waiters->del([c](sPtr<tinyState> x){ return x == c ? 1 : 0; }).is_notNull() ) ;	/* dedup self */
		read_waiters->ins(MAX_INTEGER64, c);
	}
	wakeup();		/* park done → kick pump: it posts a ReadFile ONLY now (demand-gated, no speculative read-ahead that would pin us forever) */
	throw sException([this](sPtr<tinyState> caller){ return rbuf->has_data() || rbuf->is_eof() || rbuf->error(); });
}

int
ts2IOdescriptor_::write(void * buf,int length)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) { err = EBADF; return -1; }
	if ( ! wbuf ) wbuf = new ts2io_ring();
	if ( wbuf->alloc(TS2IO_RING_CAP) < 0 ) { err = ENOMEM; return -1; }
	if ( ensure_tpio() < 0 ) { err = (int)GetLastError(); state = TS2IO_ERROR; return -1; }
	if ( wbuf->error() ) { err = wbuf->error(); state = TS2IO_ERROR; return -1; }
	{
	DWORD n = wbuf->copy_in(buf,(DWORD)length);
		wakeup();				/* kick pump: (re)post write-behind */
		if ( n > 0 ) return (int)n;
	}
	{
	sPtr<tinyState> c = sCallSection::key->caller();
		while ( write_waiters->del([c](sPtr<tinyState> x){ return x == c ? 1 : 0; }).is_notNull() ) ;	/* dedup self */
		write_waiters->ins(MAX_INTEGER64, c);
	}
	throw sException([this](sPtr<tinyState> caller){ return wbuf->has_space() || wbuf->error(); });
}

/* threadpool completion (foreign pool thread).  Minimal: update the ring under its
   own mutex, drop the reactor keep-alive this op held, then wakeup().  All
   orchestration is in ACT_START.  COOKBOOK §6.7.  refio counts outstanding
   threadpool ops (one addRefio per StartThreadpoolIo in the pump), so exactly one
   delRefio here per completion — race-free, no per-pump reconcile. */
VOID CALLBACK
ts2IOdescriptor_::io_cb(PTP_CALLBACK_INSTANCE,PVOID ctx,PVOID ov,ULONG res,ULONG_PTR bytes,PTP_IO)
{
ts2IOdescriptor_ * self = (ts2IOdescriptor_*)ctx;
	self->on_complete((OVERLAPPED*)ov,(int)res,(DWORD)bytes);
	self->wakeup();				/* while still pinned by our fwIO ref below */
	if ( self->io.is_notNull() ) self->io->delRefio(self->ifp);	/* last stmt: may drop the final ref */
}

/* base completion routing (pool thread): stream rbuf/wbuf only. */
void
ts2IOdescriptor_::on_complete(OVERLAPPED * ov,int res,DWORD bytes)
{
	if ( ov == &read_ov )	rbuf->complete_fill(bytes,res);
	else			wbuf->complete_drain(bytes,res);
}

INTEGER64
ts2IOdescriptor_::seek(INTEGER64 pos,int where)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) { err = EBADF; return -1; }
	{
	LARGE_INTEGER li, out;
		li.QuadPart = pos;
		if ( ! SetFilePointerEx(fd, li, &out, (DWORD)where) ) {
			err = (int)GetLastError(); state = TS2IO_ERROR; return -1;
		}
		return (INTEGER64)out.QuadPart;
	}
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	return rDO|INI_ts2IOdescriptor_START;
}
TS_STATE(INI_ts2IOdescriptor_START)
{
	io = io.d_cast(application->fw());		/* retained overlapped path uses it */
	read_waiters  = thNEW(stdQueue<tinyState>,());
	write_waiters = thNEW(stdQueue<tinyState>,());
	state = TS2IO_ACTIVE;
	return rDO|INI_ts2IO_START;
}


/* base pump: post pending stream I/O (rbuf/wbuf) + wake stream waiters.  Returns 1
   if a completion is outstanding or a waiter is parked (→ hold reactor keep-alive).
   Called from ACT_START (app-mtx); ts2IOsocket overrides to add the datagram ring. */
int
ts2IOdescriptor_::pump_io()
{
	/* Wake satisfied waiters FIRST, so the read below is not re-posted for a reader
	   we are about to satisfy (which would leave a speculative ReadFile pending — and
	   with the fwIO pin, a perpetual pending read pins us alive forever → shutdown
	   hang).  Reads are demand-gated: post ReadFile only while a reader is parked. */
	if ( rbuf && read_waiters->count && (rbuf->has_data() || rbuf->is_eof() || rbuf->error()) ) {
	sPtr<stdQueue<tinyState> > wk = read_waiters;
		read_waiters = thNEW(stdQueue<tinyState>,());
		for ( sPtr<tinyState> c ; (c = wk->del()) != thNULL ; )
			c->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	if ( wbuf && write_waiters->count && (wbuf->has_space() || wbuf->error()) ) {
	sPtr<stdQueue<tinyState> > wk = write_waiters;
		write_waiters = thNEW(stdQueue<tinyState>,());
		for ( sPtr<tinyState> c ; (c = wk->del()) != thNULL ; )
			c->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	if ( tpio ) {
	char * p; DWORD n;
		if ( rbuf && read_waiters->count && rbuf->reserve_fill(p,n) ) {	/* demand-gated read */
			::memset(&read_ov,0,sizeof(read_ov));
			StartThreadpoolIo(tpio);
			if ( io.is_notNull() ) io->addRefio(ifThis);		/* op posted → keep reactor alive until io_cb */
		DWORD got = 0;
			if ( ! ReadFile(fd,p,n,&got,&read_ov) && GetLastError() != ERROR_IO_PENDING ) {
				CancelThreadpoolIo(tpio);
				if ( io.is_notNull() ) io->delRefio(ifThis);	/* no completion coming */
				rbuf->complete_fill(0,(int)GetLastError());
			}
		}
		if ( wbuf && wbuf->reserve_drain(p,n) ) {			/* writes drain fully then stop (no perpetual op) */
			::memset(&write_ov,0,sizeof(write_ov));
			StartThreadpoolIo(tpio);
			if ( io.is_notNull() ) io->addRefio(ifThis);
		DWORD got = 0;
			if ( ! WriteFile(fd,p,n,&got,&write_ov) && GetLastError() != ERROR_IO_PENDING ) {
				CancelThreadpoolIo(tpio);
				if ( io.is_notNull() ) io->delRefio(ifThis);
				wbuf->complete_drain(0,(int)GetLastError());
			}
		}
	}
	return 0;			/* return value unused: keep-alive is op-tied, not per-pump */
}


/* ACT_START = resting/pump state (INI lands here via ACT_PREV).  Idempotent: any
   wakeup() (from read/write or io_cb) re-evaluates via pump_io() — posts pending I/O
   and wakes ready waiters.  The reactor keep-alive is handled per-op (addRefio at
   each StartThreadpoolIo in the pump, delRefio at the matching io_cb completion), so
   there is no keep-alive bookkeeping here.  Runs under app-mtx (TS_STATE). */
TS_STATE(ACT_START)
{
	if ( is_destroyed() )
		return rDO|FIN_START;
	pump_io();			/* virtual: base=stream, ts2IOsocket adds datagram */
	return 0;			/* rest until next kick */
}


TS_STATE(FIN_START)
{
	return rDO|FIN_ts2IOdescriptor_START;
}

/* base graceful flush: push buffered write-behind out before close, so
   `write(); close();` doesn't silently drop it.  Cancel only a pending READ first
   — a pipe read would else never complete and hang the wait — then let an in-flight
   WRITE finish and drain the rest of wbuf synchronously through the same threadpool
   completion path.  ts2IOsocket overrides to also flush its datagram send-ring. */
void
ts2IOdescriptor_::io_teardown_flush()
{
	if ( ! tpio )
		return;
	CancelIoEx(fd,&read_ov);			/* read only; ERROR_NOT_FOUND if none */
	WaitForThreadpoolIoCallbacks(tpio,FALSE);	/* finish in-flight, no cancel */
	while ( wbuf && wbuf->has_data() && ! wbuf->error() ) {
	char * p; DWORD n;
		if ( ! wbuf->reserve_drain(p,n) )
			break;
		::memset(&write_ov,0,sizeof(write_ov));
		StartThreadpoolIo(tpio);
		if ( io.is_notNull() ) io->addRefio(ifThis);		/* op posted (balanced by io_cb below) */
	DWORD got = 0;
		if ( ! WriteFile(fd,p,n,&got,&write_ov) && GetLastError() != ERROR_IO_PENDING ) {
			CancelThreadpoolIo(tpio);
			if ( io.is_notNull() ) io->delRefio(ifThis);
			wbuf->complete_drain(0,(int)GetLastError());
			break;
		}
		WaitForThreadpoolIoCallbacks(tpio,FALSE);	/* io_cb -> complete_drain + delRefio */
	}
}

/* base ring free (called AFTER callbacks are drained).  ts2IOsocket overrides to
   also free its datagram rings, then chains here. */
void
ts2IOdescriptor_::io_free_buffers()
{
	if ( rbuf ) { delete rbuf; rbuf = NULL; }	/* ~ts2io_ring frees the buffer */
	if ( wbuf ) { delete wbuf; wbuf = NULL; }
}

/* blocking teardown (mtx released).  Drain threadpool callbacks so no io_cb can
   touch any ring after we free it (COOKBOOK §6.7). */
TS_THREAD(FIN_ts2IOdescriptor_START)
{
	state = TS2IO_CLOSE;
	closing = 1;
	if ( tpio ) {
		/* Drain outstanding ops: io_teardown_flush cancels the read side and drains
		   write-behind; CancelIoEx(NULL) aborts anything left.  Every outstanding op
		   completes here, so each addRefio taken at StartThreadpoolIo is matched by
		   its io_cb delRefio during this drain — no explicit keep-alive drop needed. */
		io_teardown_flush();		/* virtual: drain write-behind (stream + datagram) */
		CancelIoEx(fd,NULL);		/* abort anything left */
		WaitForThreadpoolIoCallbacks(tpio,TRUE);
		CloseThreadpoolIo(tpio); tpio = NULL;
	}
	if ( io.is_notNull() )
		io->detach(ifThis);		/* retained overlapped path cleanup */
	if ( fd != INVALID_HANDLE_VALUE ) {
		CloseHandle(fd); fd = INVALID_HANDLE_VALUE;
	}
	io_free_buffers();			/* virtual: free rings after drain */
	return rDO|FIN_ts2IOdescriptor_WAKE;
}

/* wake any leftover waiters (mtx held) so they re-run read/write and get EBADF. */
TS_STATE(FIN_ts2IOdescriptor_WAKE)
{
	if ( read_waiters.is_notNull() && read_waiters->count ) {
	sPtr<stdQueue<tinyState> > wk = read_waiters;
		read_waiters = thNEW(stdQueue<tinyState>,());
		for ( sPtr<tinyState> c ; (c = wk->del()) != thNULL ; )
			c->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	if ( write_waiters.is_notNull() && write_waiters->count ) {
	sPtr<stdQueue<tinyState> > wk = write_waiters;
		write_waiters = thNEW(stdQueue<tinyState>,());
		for ( sPtr<tinyState> c ; (c = wk->del()) != thNULL ; )
			c->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	return rDO|FIN_ts2IO_START;
}
