/*
 * ts2IOwinConsole — MinGW console-handle ts2IO.  Windows-port design memo §5/§9.
 *
 * A Windows console handle is NOT an overlapped file handle (can't be put on an
 * IOCP), but its INPUT handle IS a waitable object (signaled when input is
 * available) — a readiness model, like a process HANDLE.  So this bridges
 * readiness to fwIO's IOCP the same way ts2System bridges child-exit:
 *
 *   read():  RegisterWaitForSingleObject(hConsole, WT_EXECUTEONLYONCE) ; when the
 *            console signals input, the threadpool callback posts a completion to
 *            fwIO's port keyed by fdid, which resumes the reader, which then does
 *            a (now-ready) ReadFile.  The one-shot wait is unregistered on resume
 *            (it already fired, so a plain non-blocking UnregisterWait is safe —
 *            no in-flight-callback deadlock, unlike ts2System's teardown).
 *   write(): synchronous WriteFile (console writes never meaningfully block).
 *
 * Derives from ts2IOdescriptor to reuse fd/fdid/io + the io->read(caller,fdid)+
 * sException registration; overrides read/write and does NOT associate with the
 * IOCP (console can't) nor CloseHandle the process-owned std handle.
 *
 * FIRST CUT (2026-07-12): compiles.  KNOWN caveat: a console INPUT handle in
 * default *cooked* (line) mode makes ReadFile block until Enter even once
 * signaled; raw input needs SetConsoleMode (a separate layer, Windows-port design memo).  Run
 * verification pending on the box.
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out, so
 * <winsock2.h> (pulled by std2/includes.h) stays the sole winsock. Windows-port design memo */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	<errno.h>
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
	HANDLE	waitHandle;
	int	wait_pending;
};

TS_END_IMPLEMENT

#endif


ts2IOwinConsole_::ts2IOwinConsole_(
		sPtr<tinyState> _parent,HANDLE _fd)
        : ts2IOdescriptor_(_parent,_fd)
{
	waitHandle = NULL;
	wait_pending = 0;
}

void
ts2IOwinConsole_::inherit(
	sPtr<tinyState> _parent,HANDLE _fd)
{
	this->TS_BASECLASS::inherit(_parent,_fd);
}


/* threadpool callback: console signalled input -> wake fwIO by posting a
   completion keyed by fdid (resumes the read registration).  Thread-safe. */
VOID CALLBACK
ts2IOwinConsole_::console_cb(PVOID ctx,BOOLEAN /*timedOut*/)
{
ts2IOwinConsole_ * self = (ts2IOwinConsole_*)ctx;
	if ( self->io.is_notNull() )
		PostQueuedCompletionStatus((HANDLE)self->io->port(),0,(ULONG_PTR)self->fdid,NULL);
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
	if ( wait_pending ) {
		if ( waitHandle ) {
			UnregisterWait(waitHandle);	/* already fired: non-blocking is safe */
			waitHandle = NULL;
		}
		wait_pending = 0;
	DWORD got = 0;
		if ( ReadFile(fd,buf,(DWORD)length,&got,NULL) )
			return (int)got;		/* 0 == EOF */
		err = (int)GetLastError();
		state = TS2IO_ERROR;
		return -1;
	}
	/* first call: arm a one-shot readiness wait on the console input handle */
	if ( RegisterWaitForSingleObject(&waitHandle,fd,console_cb,this,
			INFINITE,WT_EXECUTEONLYONCE) ) {
		wait_pending = 1;
		io->read(sCallSection::key->caller(),fdid);
		throw sException([this](sPtr<tinyState> caller){ return !io->is_read(caller); });
	}
	err = (int)GetLastError();
	state = TS2IO_ERROR;
	return -1;
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
		UnregisterWaitEx(waitHandle,INVALID_HANDLE_VALUE);
		waitHandle = NULL;
	}
	if ( io.is_notNull() )
		io->detach(ifThis);
	/* do NOT CloseHandle: GetStdHandle returns a process-owned std handle.
	   Bypass the descriptor FIN (which closes fd); go straight to ts2IO. */
	return rDO|FIN_ts2IO_START;
}
