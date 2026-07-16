/*
 * tsSignalCore — MinGW implementation.  Windows-port design memo §5.
 *
 * Windows has no POSIX signals.  The only thing tinyState needs here is
 * "catch Ctrl+C (and window close) and drive a graceful shutdown", so this is a
 * thin SetConsoleCtrlHandler shim mapped onto the SAME delivery machinery the
 * POSIX version uses (a front_list of tsSignal listeners woken via fwIO):
 *
 *   - INI registers a global console-control handler once and puts this object
 *     on the static signal_list (keyed by sig, e.g. SIGINT).
 *   - the console handler runs on a Windows OS thread (NOT a POSIX async-signal
 *     handler, so it may do real work); it maps the control event to a signal
 *     number, finds the matching tsSignalCore and wakes fwIO by posting a
 *     completion to its IOCP port keyed by this object's id.  That replaces the
 *     POSIX self-pipe write.
 *   - ACT_START registers io->read(ifThis, key); the posted completion resumes
 *     ACT_START_RET, which calls send_signal() on every front_list listener —
 *     identical to POSIX from here on.
 *
 * SIGPIPE is intentionally NOT handled: on Windows it does not exist, and its
 * POSIX roles (THR_KILL interrupt + broken-pipe suppression) both vanish (Windows-port design memo
 * §4/5).  FIRST CUT (2026-07-12): compiles; run verification pending on the box.
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out, so
 * <winsock2.h> (pulled by std2/includes.h) stays the sole winsock. Windows-port design memo */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	<signal.h>
#include	<errno.h>
#include	"_ts2/c++/tsSignalCore_.h"
#include	"ts2/c++/ts2IOdescriptor.h"	/* ts2io_alloc_key() — shared IOCP key space */

CLASS_TINYSTATE(tsSignalCore,tinyState)

#if 0
TS_BEGIN_IMPLEMENT

class TS_THISCLASS : public TS_BASECLASS {
public:
	tsSignalCore_(
		sPtr<tinyState>  parent,
		int sig);
	void inherit(
		sPtr<tinyState>  parent,
		int sig);

	static tsSignalCore * 	search_signal(int sig);
	void			ins_front(sPtr<tsSignal>  obj);
	void			del_front(sPtr<tsSignal>  obj);

	tsSignalCore * 		next;
	sPtr<tsSignalCore> 	_next;
	int			sig;
	int			sig_count;
	/** @brief IOCP registration key (unique per object); the console handler
	 *  posts a completion with this key to wake fwIO. */
	int			key;
	/** @brief fwIO IOCP port HANDLE (void*), cached so the static console
	 *  handler can post without touching the (private) io sPtr. */
	void *			port;
private:
	sPtr<fwIO> 			io;
	sPtr<stdQueue<tsSignal> > 	front_list;

	static BOOL WINAPI	console_handler(DWORD ctrlType);
	static tsSignalCore * 	signal_list;
	static sPtr<tsSignalCore> _signal_list;
	void			ins_signal();
	void			del_signal();
};
TS_END_IMPLEMENT
#endif


/*******************************************
	PUBLIC FUNCTIONS
********************************************/

tsSignalCore_::tsSignalCore_(
	sPtr<tinyState>  parent,
	int sig)
	:
	tinyState_(parent->application)
{
	this->sig = sig;
}

void
tsSignalCore_::inherit(
	sPtr<tinyState>  parent,
	int sig)
{
	this->TS_BASECLASS::inherit(parent);
}

static volatile LONG	g_console_handler_installed = 0;

tsSignalCore *
tsSignalCore_::signal_list;
sPtr<tsSignalCore>
tsSignalCore_::_signal_list;

tsSignalCore *
tsSignalCore_::search_signal(int sig)
{
tsSignalCore *  ret;
	for ( ret = tsSignalCore_::signal_list ; ret ; ret = ret->next )
		if ( ret->sig == sig )
			return ret;
	return 0;
}

void
tsSignalCore_::ins_signal()
{
	this->next = tsSignalCore_::signal_list;
	tsSignalCore_::signal_list = ifThis.__get();
	this->_next = tsSignalCore_::_signal_list;
	tsSignalCore_::_signal_list = ifThis;
}

void
tsSignalCore_::del_signal()
{
	/* unlink from the raw traversal list (search_signal walks this). */
tsSignalCore ** ip;
	for ( ip = &tsSignalCore_::signal_list ;
			(*ip) && *ip != ifThis.__get() ;
			ip = &(*ip)->next );
	if ( *ip == 0 )
		return;
	*ip = this->next;
	this->next = 0;

	/* ALSO unlink from the owning sPtr list (_signal_list / _next).  Without
	   this we stay referenced by the static _signal_list and are only released
	   at static-destructor time — where relref() locks a per-id sThreadMutex
	   whose vtable may already be torn down, so the virtual lock() call jumps
	   through a NULL vtable and crashes (real Windows + wine).  Dropping the
	   sPtr ref here lets our parent (application) free us at normal teardown,
	   while the refcount-lock statics are still alive.  Windows-port design memo */
sPtr<tsSignalCore> * sp;
	for ( sp = &tsSignalCore_::_signal_list ;
			sp->is_notNull() && (*sp).__get() != ifThis.__get() ;
			sp = &(*sp)->_next );
	if ( sp->is_notNull() ) {
	sPtr<tsSignalCore> nxt = (*sp)->_next;
		*sp = nxt;		/* splice out; releases the list's ref to us */
	}
	this->_next = thNULL;
}

/* Windows console control handler.  Runs on an OS thread; maps the control
   event to a signal number and wakes the matching tsSignalCore via its fwIO
   IOCP port.  Returns TRUE if handled (suppresses default termination). */
BOOL WINAPI
tsSignalCore_::console_handler(DWORD ctrlType)
{
int sig;
	switch ( ctrlType ) {
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
		sig = SIGINT;
		break;
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		sig = SIGTERM;
		break;
	default:
		return FALSE;
	}
	{
	tsSignalCore * c = search_signal(sig);
		if ( c && c->port ) {
			c->sig_count ++;
			PostQueuedCompletionStatus((HANDLE)c->port, 0, (ULONG_PTR)c->key, NULL);
			return TRUE;
		}
	}
	return FALSE;
}

void
tsSignalCore_::ins_front(sPtr<tsSignal>  obj)
{
	this->front_list->ins(0,obj);
	if ( this->front_list->count == 1 )
		wakeup();
}

void
tsSignalCore_::del_front(sPtr<tsSignal>  obj)
{
	this->front_list->del(obj,0);
	if ( this->front_list->count == 0 )
		this->wakeup();
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	this->io = sPtr<fwIO>::d_cast(this->application->fw());
	this->front_list = thNEW( stdQueue<tsSignal>,());
	/* draw the IOCP completion key from the SAME allocator as ts2IO descriptors
	   / sockets / ts2System.  A separate counter here collided (both start at 1)
	   so a socket completion could match this tsSignalCore's io->read key and
	   corrupt the reactor -> teardown hang when a tsSignal coexists with socket
	   I/O.  Windows-port design memo */
	this->key = ts2io_alloc_key();
	this->port = this->io->port();

	this->ins_signal();

	/* register the console handler once for the whole process */
	if ( InterlockedCompareExchange(&g_console_handler_installed,1,0) == 0 )
		SetConsoleCtrlHandler(console_handler, TRUE);

	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
	this->io->read(ifThis,this->key);
	return ACT_START_RET;
}
TS_STATE(ACT_START_RET)
{
sPtr<stdQueue<tsSignal> >  qs;
sPtr<stdQueueElement<tsSignal> > el;
	if ( this->front_list->count == 0 )
		return rDO|FIN_START;
	if ( ev->type != TSE_RETURN )
		return 0;
	if ( this->sig_count > 0 )
		this->sig_count --;
	qs = thNEW( stdQueue<tsSignal>,(this->front_list));
	for ( el = qs->head ; el.is_notNull() ; el = el->next )
		el->data->send_signal();
	return rDO|ACT_FINISH;
}
TS_STATE(ACT_FINISH)
{
	if ( this->front_list->count == 0 )
		return rDO|FIN_START;
	return rDO|ACT_START;
}

TS_STATE(FIN_START)
{
	io->detach(ifThis);
	this->del_signal();
	/* leave the global console handler installed (harmless; search_signal
	   returns 0 once this object is off signal_list). */
	return rDO|FIN_SLEEP;
}
TS_STATE(FIN_SLEEP)
{
	if ( front_list->count == 0 )
		return 0;
	sig_count = 0;
	this->ins_signal();
	return rDO|ACT_START;
}
