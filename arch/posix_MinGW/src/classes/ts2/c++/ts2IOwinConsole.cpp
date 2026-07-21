/*
 * ts2IOwinConsole — MinGW console-handle ts2IO.  Windows-port design memo §5/§9.
 *
 * A Windows console handle is NOT an overlapped file handle (can't be put on an
 * IOCP), but its INPUT handle IS a waitable object (signaled when input is
 * available) — a readiness model, like a process HANDLE.  So this bridges
 * readiness to fwIO's IOCP the same way ts2System bridges child-exit:
 *
 *   read():  RegisterWaitForSingleObject(hConsole, WT_EXECUTEONLYONCE) ; when the
 *            console signals input, the threadpool callback sets input_ready and
 *            wakeup()s us; the pump (pump_io, run from the base ACT_START) wakes the
 *            parked reader, which re-runs read() and does a (now-ready) ReadFile.
 *            The one-shot wait is unregistered on resume (it already fired, so a
 *            plain non-blocking UnregisterWait is safe — no in-flight-callback
 *            deadlock, unlike ts2System's teardown).
 *   write(): synchronous WriteFile (console writes never meaningfully block).
 *
 * Bridges the readiness callback to the state machine with the v2 idiom (COOKBOOK
 * §6.7): callback = set-flag + wakeup(); pump = wake the parked reader; reactor
 * keep-alive = one whole-lifetime io->addRefio(ifThis) taken when the wait is armed and
 * dropped in FIN (the base's op-tied refio doesn't apply — a readiness wait has no
 * per-op threadpool completion to balance against).  It no longer routes through
 * fwIO's completion port (io->read/is_read + fdid), so this could drop the
 * base fdid member.  Overrides read/write/pump_io; does NOT associate with any IOCP
 * (a console handle can't) nor CloseHandle the process-owned std handle.
 *
 * ⚠️ UNVERIFIED AT RUNTIME: console input needs an interactive TTY, which the
 * headless test setup cannot drive (piping stdin yields a pipe handle = the base
 * ts2IOdescriptor path, not this class), so the automated suite never exercises
 * this file.  Compile + suite-non-regression only — the read path needs a manual
 * interactive check.  KNOWN caveat: a console INPUT handle in default *cooked*
 * (line) mode makes ReadFile block until Enter even once signalled; raw input needs
 * SetConsoleMode (a separate layer).
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out, so
 * <winsock2.h> (pulled by std2/includes.h) stays the sole winsock. Windows-port design memo */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	<errno.h>
#include	"ts2/c++/stdEvent.h"
#include	"_ts2/c++/ts2IOwinConsole_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOwinConsole,ts2/c++/ts2IOdescriptor)

#if 0

TS_BEGIN_IMPLEMENT

class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2IOwinConsole_(
		sPtr<tinyState> parent,
		HANDLE _fd);
	void inherit(
		sPtr<tinyState> parent,
		HANDLE _fd);

	virtual int read(void * buf,int length);
	virtual int write(void * buf,int length);
protected:
	static VOID CALLBACK	console_cb(PVOID ctx,BOOLEAN timedOut);
	virtual int		pump_io();	/* wake parked reader + reactor keep-alive */
	HANDLE			waitHandle;	/* armed one-shot readiness wait (NULL = none) */
	volatile int		input_ready;	/* set by console_cb (pool thread) on input */
	sPtr<tinyState>		reader;		/* parked reader awaiting input */
};

TS_END_IMPLEMENT

#endif


ts2IOwinConsole_::ts2IOwinConsole_(
		sPtr<tinyState> _parent,HANDLE _fd)
        : ts2IOdescriptor_(_parent,_fd)
{
	waitHandle = NULL;
	input_ready = 0;
}

void
ts2IOwinConsole_::inherit(
	sPtr<tinyState> _parent,HANDLE _fd)
{
	this->TS_BASECLASS::inherit(_parent,_fd);
}


/* threadpool readiness callback (pool thread): console signalled input.  Minimal
   per COOKBOOK §6.7 — set the flag, wake the state machine; no app-mtx, no logic. */
VOID CALLBACK
ts2IOwinConsole_::console_cb(PVOID ctx,BOOLEAN /*timedOut*/)
{
ts2IOwinConsole_ * self = (ts2IOwinConsole_*)ctx;
	self->input_ready = 1;
	self->wakeup();
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/

int
ts2IOwinConsole_::read(void * buf,int length)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	/* resume: the one-shot readiness wait fired -> input is available */
	if ( input_ready ) {
		input_ready = 0;
		if ( waitHandle ) {
			UnregisterWait(waitHandle);	/* already fired: non-blocking is safe */
			waitHandle = NULL;
		}
		wakeup();				/* let the pump drop the keep-alive if now idle */
	DWORD got = 0;
		if ( ReadFile(fd,buf,(DWORD)length,&got,NULL) )
			return (int)got;		/* 0 == EOF */
		err = (int)GetLastError();
		state = TS2IO_ERROR;
		return -1;
	}
	/* arm a one-shot readiness wait on the console input handle (once) */
	if ( ! waitHandle &&
			! RegisterWaitForSingleObject(&waitHandle,fd,console_cb,this,
				INFINITE,WT_EXECUTEONLYONCE) ) {
		err = (int)GetLastError();
		state = TS2IO_ERROR;
		return -1;
	}
	/* whole-lifetime reactor keep-alive: a console readiness wait registers nothing
	   with fwIO, and (unlike the base's op-tied refio) there is no per-op completion
	   to balance against — so hold ONE refio from the first arm until FIN drops it. */
	if ( io.is_notNull() && ! io_ref ) { io->addRefio(ifThis); io_ref = 1; }
	reader = sCallSection::key->caller();		/* park; pump wakes it when input lands */
	wakeup();
	throw sException([this](sPtr<tinyState> caller){
			return input_ready || C_TEST(tinyState_::state(),C_ZOM|C_FIN); });
}

/* pump (run from the base ACT_START on every wakeup): wake the parked reader once
   input is ready.  Console never drives the base stream ring, so the base
   ts2IOdescriptor_::pump_io() is intentionally not chained; the reactor keep-alive
   is the whole-lifetime io_ref armed in read(), not tied to this pump. */
int
ts2IOwinConsole_::pump_io()
{
	if ( reader.is_notNull() && input_ready ) {
	sPtr<tinyState> c = reader; reader = thNULL;
		c->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	return 0;
}

int
ts2IOwinConsole_::write(void * buf,int length)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	/* console output does not meaningfully block: synchronous write. */
	{
	DWORD got = 0;
		if ( WriteFile(fd,buf,(DWORD)length,&got,NULL) )
			return (int)got;
		err = (int)GetLastError();
		state = TS2IO_ERROR;
		return -1;
	}
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	return rDO|INI_ts2IOwinConsole_START;
}
TS_STATE(INI_ts2IOwinConsole_START)
{
	io = io.d_cast(application->fw());
	/* console handle: no IOCP association, no O_NONBLOCK.  Skip the descriptor
	   INI (which would associate with the port) and go straight to ts2IO. */
	state = TS2IO_ACTIVE;
	return rDO|INI_ts2IO_START;
}

TS_STATE(FIN_START)
{
	return rDO|FIN_ts2IOwinConsole_START;
}
TS_THREAD(FIN_ts2IOwinConsole_START)
{
	state = TS2IO_CLOSE;
	if ( waitHandle ) {
		UnregisterWaitEx(waitHandle,INVALID_HANDLE_VALUE);	/* drain the cb */
		waitHandle = NULL;
	}
	if ( io.is_notNull() ) {
		if ( io_ref ) { io->delRefio(ifThis); io_ref = 0; }	/* drop reactor keep-alive */
		io->detach(ifThis);
	}
	/* do NOT CloseHandle: GetStdHandle returns a process-owned std handle.
	   Bypass the descriptor FIN (which closes fd); go straight to ts2IO. */
	return rDO|FIN_ts2IOwinConsole_WAKE;
}

/* wake a leftover parked reader (mtx held) so it re-runs read() and gets EBADF. */
TS_STATE(FIN_ts2IOwinConsole_WAKE)
{
	if ( reader.is_notNull() ) {
	sPtr<tinyState> c = reader; reader = thNULL;
		c->eventHandler(thNEW(stdEvent,(TSE_RETURN, ifThis, (INTEGER64)0)));
	}
	return rDO|FIN_ts2IO_START;
}
