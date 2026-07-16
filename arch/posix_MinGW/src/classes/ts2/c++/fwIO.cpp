/*
 * fwIO — MinGW (IOCP) implementation.  Windows-port design memo §A/B.
 *
 * The POSIX version (arch/posix/.../fwIO.cpp) uses select() + a self-pipe wake.
 * Windows has no readiness-wait for pipe/file HANDLEs, so this version uses an
 * I/O Completion Port (IOCP):
 *
 *   - IO objects (ts2IOdescriptor/ts2IOsocket, MinGW overlays) associate their
 *     HANDLE/SOCKET with this port via CreateIoCompletionPort(handle, port(),
 *     (ULONG_PTR)obj, 0) and issue *overlapped* reads/writes.  The completion is
 *     delivered here, keyed by the tinyState object.
 *   - wake (self-pipe replacement) = PostQueuedCompletionStatus(iocp, WAKE_KEY).
 *   - loop() = GetQueuedCompletionStatus with a timeout derived from the nearest
 *     interval; on a completion it moves the object's read/write registration to
 *     the ready set and dispatches events, exactly like the select loop does.
 *
 * FIRST CUT (2026-07-12, groundwork): the generic queue / interval / event
 * dispatch logic mirrors the POSIX version; completion dispatch is by object
 * key.  The overlapped read/write themselves live in the IO-object overlays
 * (ts2IOdescriptor/ts2IOsocket, not yet ported), so the completion path is not
 * yet exercised end-to-end — but the reactor compiles and its structure is the
 * final one.  Direction disambiguation (a completion that is both read & write)
 * will use the per-direction OVERLAPPED once the IO objects carry them.
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out, so
 * <winsock2.h> (pulled by std2/includes.h) stays the sole winsock. Windows-port design memo */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	<errno.h>
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/fwIO.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/stdEvent.h"
#include	"ts2/c++/tsApplication.h"
#include	"_ts2/c++/tinyState_.h"

INTEGER64
fwIO::start_time;

/* wake sentinel: PostQueuedCompletionStatus uses this key so loop() can tell a
   wake apart from a real IO completion (whose key is a tinyState object). */
static char             fwio_wake_sentinel;
#define WAKE_KEY        ((ULONG_PTR)&fwio_wake_sentinel)

/* process-global winsock init (once, before main via static ctor).  The socket
   classes need this before any socket() call.  Windows-port design memo. */
struct fwIOWinsockInit {
	fwIOWinsockInit()  { WSADATA w; WSAStartup(MAKEWORD(2,2), &w); }
};
static fwIOWinsockInit	g_fwio_winsock_init;

class fwIOptr : public stdObject {
public:
	sPtr<fwIOdata>	ptr;
	fwIOptr()	{}
	virtual ~fwIOptr() {
		ptr = thNULL;
	}
};

fwIO::fwIO(sPtr<tsApplication> app)
{
	iocp = (void*)CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	start_time = stdInterval::now();
	this->application = app;

	this->read_objs     = thNEW( stdQueue<fwIOdata>,());
	this->write_objs    = thNEW( stdQueue<fwIOdata>,());
	this->interval_objs = thNEW( stdQueue<fwIOdata>,());

	announceMaxfd = -1;
	refio = 0;
}


static int
_send_destroy_event(sPtr<fwIOdata>  d1,sPtr<stdObject>  d2)
{
	sPtr<stdEvent>  ev;
	ev = thNEW( stdEvent,(TSE_DESTROY,d1->obj,(INTEGER64)0));
	d1->obj->eventHandler(ev);
	return 0;
}

fwIO::~fwIO()
{
sPtr<stdQueue<fwIOdata> > r, w, iv;
	{
	sThreadMutexHandle __hdr(mu);
		r  = this->read_objs;
		w  = this->write_objs;
		iv = this->interval_objs;
		this->read_objs     = thNULL;
		this->write_objs    = thNULL;
		this->interval_objs = thNULL;
	}
	r->check(this,_send_destroy_event);
	w->check(this,_send_destroy_event);
	iv->check(this,_send_destroy_event);
	r  = thNULL;
	w  = thNULL;
	iv = thNULL;
	this->application = thNULL;
	if ( iocp )
		CloseHandle((HANDLE)iocp);
	iocp = NULL;
}


/* ---- wake (self-pipe replacement) ---- */

void *
fwIO::port()
{
	return iocp;
}

void
fwIO::wake()
{
	if ( wait_flag == 1 )
		return;
	wait_flag = 1;
	PostQueuedCompletionStatus((HANDLE)iocp, 0, WAKE_KEY, NULL);
}

/* names kept so the shared registration code paths read the same as POSIX */
void	fwIO::writePipe()	{ wake(); }
void	fwIO::readPipe()	{ /* IOCP: wake completions are consumed in loop() */ }


/* ---- dump / printf (same as POSIX, minus fd_set maskbits) ---- */

static int
_ts_io_dump(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
	printf("   %lli",dd->data);
	return dd->obj->printParent();
}

void
fwIO::dump(const char * msg)
{
	if ( stdFrameWork::trace_bit & FWTR_RWI )
		::printf("%s START\n",msg);
	if ( stdFrameWork::trace_bit & FWTR_READ ) {
		::printf("   ** READ **\n");
		read_objs->check(thNULL,_ts_io_dump);
	}
	if ( stdFrameWork::trace_bit & FWTR_WRITE ) {
		::printf("   ** WRITE **\n");
		write_objs->check(thNULL,_ts_io_dump);
	}
	if ( stdFrameWork::trace_bit & FWTR_INTERVAL ) {
		::printf("   ** INTERVAL **\n");
		interval_objs->check(thNULL,_ts_io_dump);
	}
	if ( stdFrameWork::trace_bit & FWTR_RWI )
		::printf("%s END\n",msg);
}

int
fwIO::printf(sPtr<tinyState>  caller,const char * fmt,...)
{
	int ret; va_list arg;
	va_start(arg,fmt);
	ret = v_printf(caller,fmt,arg);
	va_end(arg);
	return ret;
}
int	fwIO::v_printf(sPtr<tinyState>  caller,const char * fmt,va_list arg)	{ return v_printf(caller,fmt,arg); }
int
fwIO::printf_err(sPtr<tinyState>  caller,const char * fmt,...)
{
	int ret; va_list arg;
	va_start(arg,fmt);
	ret = v_printf(caller,fmt,arg);
	va_end(arg);
	return ret;
}
int	fwIO::v_printf_err(sPtr<tinyState>  caller,const char * fmt,va_list arg)	{ return v_printf(caller,fmt,arg); }


/* ---- registration API (generic; identical to POSIX except wake()) ----
 * On MinGW the second arg is an opaque key (the object associates its HANDLE
 * with the port keyed by itself); there is no FD_SETSIZE bound. */

int
fwIO::read(sPtr<tinyState>  THIS,int fd,int opt)
{
sPtr<fwIOdata>  dd;
sThreadMutexHandle __hdr(mu);
	dd = thNEW( fwIOdata,());
	dd->type = TSE_RETURN;
	dd->seq  = tinyState::getSeq();
	dd->obj  = THIS;
	dd->data = fd;
	dd->opt  = opt;
	this->read_objs->ins(fd,dd);
	wake();
	return dd->seq;
}

int
fwIO::write(sPtr<tinyState>  THIS,int fd,int opt)
{
sPtr<fwIOdata>  dd;
sThreadMutexHandle __hdr(mu);
	dd = thNEW( fwIOdata,());
	dd->type = TSE_RETURN;
	dd->seq  = tinyState::getSeq();
	dd->obj  = THIS;
	dd->data = fd;
	dd->opt  = opt;
	this->write_objs->ins(fd,dd);
	wake();
	return dd->seq;
}

int
fwIO::wait(sPtr<tinyState>  THIS,INTEGER64 tm,int type)
{
sPtr<fwIOdata>  dd;
sThreadMutexHandle __hdr(mu);
	dd = thNEW( fwIOdata,());
	dd->type = type;
	dd->seq  = tinyState::getSeq();
	dd->data = stdInterval::now() + tm;
	dd->obj  = THIS;
	this->interval_objs->ins(dd->data,dd);
	wake();
	return dd->seq;
}

int
fwIO::is_read(sPtr<tinyState> THIS)
{
sThreadMutexHandle __hdr(mu);
	return this->read_objs->check([THIS](sPtr<fwIOdata> dd){
			return dd->obj == THIS ? 1 : 0;
		}).is_notNull();
}
int
fwIO::is_write(sPtr<tinyState> THIS)
{
sThreadMutexHandle __hdr(mu);
	return this->write_objs->check([THIS](sPtr<fwIOdata> dd){
			return dd->obj == THIS ? 1 : 0;
		}).is_notNull();
}
int
fwIO::is_wait(sPtr<tinyState> THIS)
{
sThreadMutexHandle __hdr(mu);
	return this->interval_objs->check([THIS](sPtr<fwIOdata> dd){
			return dd->obj == THIS ? 1 : 0;
		}).is_notNull();
}

static int
_ts_io_detach(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
sPtr<tinyState>  THIS = sPtr<tinyState>::d_cast(d2);
	return dd->obj == THIS ? 1 : 0;
}
int
fwIO::detach(sPtr<tinyState>  THIS,int flags)
{
sThreadMutexHandle __hdr(mu);
	if ( flags & FWTR_READ )
		for ( ; this->read_objs->del(THIS,_ts_io_detach).is_notNull() ; );
	if ( flags & FWTR_WRITE )
		for ( ; this->write_objs->del(THIS,_ts_io_detach).is_notNull() ; );
	if ( flags & FWTR_INTERVAL )
		for ( ; this->interval_objs->del(THIS,_ts_io_detach).is_notNull() ; );
	wake();
	return 0;
}

void	fwIO::addRefio()	{ sThreadMutexHandle __hdr(mu); refio ++; }
void	fwIO::delRefio()	{ sThreadMutexHandle __hdr(mu); refio --; wake(); }

static int
_ts_io_abort(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
sPtr<fwIOdata>  ref;
	ref = sPtr<fwIOdata>::d_cast(d2);
	return ref->data == dd->data ? 1 : 0;
}
void
fwIO::abort(int fd)
{
sPtr<fwIOdata>  d, ret;
	d = thNEW( fwIOdata,(fd));
	for ( ; ; ) {
		{
		sThreadMutexHandle __hdr(mu);
			if ( !this->read_objs.is_notNull() )
				break;
			ret = this->read_objs->del(d,_ts_io_abort);
			if ( ret == thNULL )
				break;
		}
		ret->obj->eventHandler(thNEW( stdEvent,(ret->type,ret->obj,ret->seq,-1)));
	}
	for ( ; ; ) {
		{
		sThreadMutexHandle __hdr(mu);
			if ( !this->write_objs.is_notNull() )
				break;
			ret = this->write_objs->del(d,_ts_io_abort);
			if ( ret == thNULL )
				break;
		}
		ret->obj->eventHandler(thNEW( stdEvent,(ret->type,ret->obj,ret->seq,-1)));
	}
}


/* ---- interval helpers (same as POSIX) ---- */

static int
ts_io_get_interval_minimal(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
sPtr<fwIOptr>  idptr;
	idptr = sPtr<fwIOptr>::d_cast(d2);
	if ( idptr->ptr == thNULL )		{ idptr->ptr = dd; return 0; }
	if ( idptr->ptr->data > dd->data )	{ idptr->ptr = dd; return 0; }
	if ( idptr->ptr->data < dd->data )	return 1;
	return 0;
}

static int
ts_io_filter_zom(sPtr<fwIOdata>  dd)
{
	return C_TEST(dd->obj->state(),C_ZOM) ? 1 : 0;
}
static int
ts_io_filter_zom2(sPtr<fwIOdata>  dd)
{
	return C_TEST(dd->obj->state(),C_ZOM) ? 1 : -1;
}


/* ---- main loop (IOCP) ---- */

int
fwIO::loop(sPtr<tsApplication> app)
{
sPtr<fwIOptr>  idppptr;
	idppptr = thNEW( fwIOptr,());
sPtr<stdQueue<fwIOdata> >  result;
sPtr<fwIOdata>  ok;
int rret = 0;
DWORD bytes; ULONG_PTR key; OVERLAPPED * ov;

	wait_flag = 1;

	for ( ; ; ) {
	DWORD timeout_ms;
	int have_interval;
	sPtr<stdQueue<fwIOdata> > backup;
		backup = thNEW(stdQueue<fwIOdata>,());
		{
		sThreadMutexHandle __hdr(mu);
			/* ZOM: back up dying registrations, drop them (like POSIX) */
			read_objs->check([backup](sPtr<fwIOdata> elm){
				if ( C_TEST(elm->obj->state(),C_ZOM) ) backup->ins(MAX_INTEGER64,elm);
				return 0; });
			write_objs->check([backup](sPtr<fwIOdata> elm){
				if ( C_TEST(elm->obj->state(),C_ZOM) ) backup->ins(MAX_INTEGER64,elm);
				return 0; });
			interval_objs->check([backup](sPtr<fwIOdata> elm){
				if ( C_TEST(elm->obj->state(),C_ZOM) ) backup->ins(MAX_INTEGER64,elm);
				return 0; });
			this->read_objs->move(ts_io_filter_zom);
			this->write_objs->move(ts_io_filter_zom);
			this->interval_objs->move(ts_io_filter_zom2);

			idppptr->ptr = thNULL;
			this->interval_objs->check(idppptr,ts_io_get_interval_minimal);
			have_interval = idppptr->ptr.is_notNull();

			if ( stdFrameWork::trace_all )
				dump(stdFrameWork::trace_all);

			if ( have_interval )
				realMaxfd = this->read_objs->count + this->write_objs->count + announceMaxfd + 1;
			else	realMaxfd = this->read_objs->count + this->write_objs->count;
			if ( realMaxfd == announceMaxfd && !have_interval ) {
				app->frameWorkEvent(0);
				continue;
			}
			/* nothing left to wait on -> return */
			if ( this->read_objs->count == 0 && this->write_objs->count == 0 &&
					!have_interval && refio == 0 )
				break;

			if ( have_interval ) {
				INTEGER64 sub = idppptr->ptr->data - stdInterval::now();
				timeout_ms = sub <= 0 ? 0 : (DWORD)(sub / 1000);	/* us -> ms */
			} else	timeout_ms = INFINITE;
			wait_flag = 0;
		}
		backup = thNULL;

		/* --- wait on the completion port --- */
		BOOL okc = GetQueuedCompletionStatus((HANDLE)iocp,&bytes,&key,&ov,timeout_ms);

		{
		sThreadMutexHandle __hdr(mu);
			wait_flag = 1;
			rret = 1;
			result = thNEW( stdQueue<fwIOdata>,());

			/* Dispatch whenever a packet was dequeued for a non-wake key.  A
			 * *failed* overlapped op (e.g. broken pipe == EOF) makes GQCS return
			 * okc==FALSE but with a valid key+ov, so we must NOT gate on okc —
			 * the object's read()/write() calls GetOverlappedResult and sees the
			 * EOF/error.  Only (!okc && ov==NULL) is a real timeout.  A keyed
			 * PostQueuedCompletionStatus (console readiness) has okc==TRUE,
			 * ov==NULL, key==fdid and also dispatches here. */
			if ( key != WAKE_KEY && (okc || ov != NULL) ) {
				/* IOCP key == the IO object's id (registered with io->read/write
				 * + associated its HANDLE with).  Move the matching read & write
				 * registrations to the ready set — both, because one key covers
				 * both directions; the caller whose overlapped is not yet done
				 * simply re-issues and re-yields.  Matches POSIX by-fd. */
				INTEGER64 kd = (INTEGER64)key;
			sPtr<stdQueue<fwIOdata> > q;
				q = thNEW(stdQueue<fwIOdata>,());
				this->read_objs->check([kd,&result,&q](sPtr<fwIOdata> dd){
					if ( dd->data == kd )	result->ins(MAX_INTEGER64,dd);
					else			q->ins(MAX_INTEGER64,dd);
					return 0; });
				this->read_objs = q;
				q = thNEW(stdQueue<fwIOdata>,());
				this->write_objs->check([kd,&result,&q](sPtr<fwIOdata> dd){
					if ( dd->data == kd )	result->ins(MAX_INTEGER64,dd);
					else			q->ins(MAX_INTEGER64,dd);
					return 0; });
				this->write_objs = q;
			}
			/* wake (key==WAKE_KEY) and timeout (!okc && ov==NULL) both just fall
			 * through to interval processing + re-loop. */

			/* --- expired intervals --- */
			{
			INTEGER64 nn = stdInterval::now();
				interval_objs->check([&nn,&result](sPtr<fwIOdata> dd){
					if ( dd->data > nn ) return 1;
					result->ins(MAX_INTEGER64,dd);
					return 0; });
				for ( ; interval_objs->head.is_notNull() &&
					interval_objs->head->data->data <= nn ;
					interval_objs->del() );
			}
		}

		/* --- dispatch events (same as POSIX) --- */
		for ( ; ; ) {
		sPtr<stdEvent>  ev;
			ok = result->del();
			if ( ok == thNULL )
				break;
			if ( stdFrameWork::trace_all && (stdFrameWork::trace_bit & FWTR_ACTIVE) ) {
				::printf("fwIO :: %s :: type=%i %p:%s:%s data=%lli\n\t",
					stdFrameWork::trace_all, ok->type,
					ok->obj.__get(), ok->obj->getClass(),
					ok->obj->getStateName(), ok->data);
				ok->obj->printParent();
			}
			ev = thNEW( stdEvent,(ok->type,ok->obj,ok->seq,ok->data));
			ok->obj->eventHandler(ev);
		}
	}
	idppptr = thNULL;
	return rret;
}
