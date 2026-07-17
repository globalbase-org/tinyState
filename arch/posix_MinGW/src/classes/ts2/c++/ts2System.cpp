/*
 * ts2System — MinGW (CreateProcess) implementation.  Windows-port design memo §E.
 *
 * POSIX spawns via fork()+exec() with self-pipes and reaps via waitpid()/SIGCHLD.
 * Windows has none of that, so:
 *   - stdio is 3 OVERLAPPED *named* pipes (anonymous pipes can't do overlapped);
 *     the parent (overlapped) ends are wrapped in ts2IOdescriptor and driven
 *     through fwIO's IOCP; the child (inheritable) ends go into STARTUPINFO.
 *   - the process is spawned with CreateProcess, only the `#`-direct-exec form
 *     is supported (shell mode returns an error), and it is put in a Job Object
 *     so destroy() can kill the whole tree via TerminateJobObject.
 *   - exit is detected with RegisterWaitForSingleObject(hProcess): the OS
 *     threadpool (bounded, shared — it does NOT cost one thread per child, so
 *     this scales to many concurrent children, unlike a per-child blocking
 *     TS_THREAD wait) fires on_exit_cb.  on_exit_cb does the MINIMAL thread-safe
 *     thing — ONLY PostQueuedCompletionStatus(exit_key) to fwIO — and NEVER
 *     touches the state machine.  fwIO's main loop then re-runs ACT_FINISH_RET
 *     on the main thread (via the io->read(exit_key) registration), so
 *     eventHandler always runs on the loop thread.  (Driving eventHandler
 *     directly from the callback thread — an earlier variant — corrupted the
 *     state machine: 91% wrong output on real Windows.  Windows-port design memo.)  The wait is
 *     torn down with a blocking UnregisterWaitEx in a TS_THREAD FIN state so no
 *     callback can fire after teardown.  destroy() = TerminateJobObject in
 *     filter().  detach() = don't wait.  Windows-port design memo §E.
 *   - NOTE (wine only): closing a process that used IOCP makes wine crash ~0.5-
 *     2.5% at process teardown in its own ntdll (a wine-internal I/O thread
 *     dereferences a NULL TEB); real Windows is 0/1000.  Not our bug.  Windows-port design memo.
 *
 * FIRST CUT (2026-07-12): compiles; run verification pending on the box.
 * DM_TTY (pty) is not supported (ConPTY is a later refinement, Windows-port design memo §8).
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out, so
 * <winsock2.h> (pulled by std2/includes.h) stays the sole winsock. Windows-port design memo */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	"_ts2/c++/ts2System_.h"

CLASS_TINYSTATE(ts2/c++/ts2System,tinyState)

static volatile LONG	g_ts2sys_uid = 0;

/* child-exit detection mode (Windows-port design memo; selectable via CMake
 * -DTS2SYS_CHILDWAIT_MODE=N, default 1):
 *   1 = (default, primary) on_exit_cb drives eventHandler(TSE_INVOKE) DIRECTLY
 *       on the OS-threadpool (foreign, non-pthread_create) thread.  The reactor
 *       is kept alive across the child-wait window by fwIO::addRefio(); FIN_START
 *       calls delRefio() whose wake() lets the loop re-check its exit condition.
 *   2 = on_exit_cb only PostQueuedCompletionStatus(exit_key); fwIO's main loop
 *       re-runs ACT_FINISH_RET on the main thread.  A dummy io->read(exit_key)
 *       keeps the reactor alive + delivers the wake.  eventHandler never runs on
 *       the foreign thread.  Kept for observation; delete once 1 is settled.
 *
 * Both are correct on REAL Windows 11: 1000/1000, bad=0, crash=0 (2026-07-12).
 * The proven fact behind mode 1: the OLD callback-driven variant's 91% output
 * corruption came from the reactor tearing down mid-wait (nothing registered ->
 * loop exits), NOT from foreign-thread eventHandler — sCallSection (pthread TLS
 * via winpthreads) IS functional on the OS-threadpool thread.  addRefio fixes it.
 * NOTE: wine HIDES mode-1 corruption (wine bad=0 even for the broken old (1));
 * the child-read/badout path must be judged on real hardware, never wine.
 */
#ifndef TS2SYS_CHILDWAIT_MODE
#define TS2SYS_CHILDWAIT_MODE 1
#endif

#if 0

TS_BEGIN_IMPLEMENT

#include	"ts2/c++/stdString.h"
#include	"ts2/c++/stdArray.h"
#include	"ts2/c++/ts2IOdescriptor.h"
#include	"ts2/c++/ts2IOdevNull.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/fwIO.h"

class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2System_(
		sPtr<tinyState> parent,
		int *		retp,
		const char *	commandLine,
		sPtr<ts2IO> * rfd=0,
		sPtr<ts2IO> * efd=0,
		sPtr<ts2IO> * wfd=0,
		int	dmode=0);
	ts2System_(
		sPtr<tinyState> parent,
		int *		retp,
		sPtr<stdString> commandLine2,
		sPtr<ts2IO> * rfd=0,
		sPtr<ts2IO> * efd=0,
		sPtr<ts2IO> * wfd=0,
		int	dmode=0);

	virtual void destroy();
	virtual void destroy(int mode);
	void detach();
protected:
	void newProcess(sPtr<tinyState> p);
	virtual sPtr<stdEvent>  filter(sPtr<stdEvent>  ev);

	TS_DEFARGS

	int do_exec(
		const char *command,
		HANDLE *fd_r,
		HANDLE *fd_w,
		HANDLE *fd_err,
		int dmode);

	/* child-exit bridge: OS threadpool waits on hProcess; on_exit_cb only posts
	   PostQueuedCompletionStatus(exit_key) to fwIO so the exit is processed on
	   the fwIO main-loop thread (never eventHandler from the callback thread). */
	static VOID CALLBACK on_exit_cb(PVOID ctx, BOOLEAN timedOut);

	unsigned 	detach_flag:1;
	unsigned	kill_flag:1;
	int status;

	INTEGER64	kill_timer;
	int		destroy_mode;
	int		ret;

	HANDLE		hProcess;
	HANDLE		hJob;
	HANDLE		waitHandle;
	sPtr<fwIO>	io;
	int		exit_key;

	sPtr<ts2IO>	d_rfd;
	sPtr<ts2IO>	d_efd;
	sPtr<tinyState>	wait_pin;	/* self-ref held while the RegisterWait is armed,
					   released only after UnregisterWaitEx — so the
					   object cannot be freed while on_exit_cb may still
					   fire with a raw `this`. */
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
class tsSignal;
TS_END_INTERFACE

#endif


ts2System_::ts2System_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
	hProcess = hJob = waitHandle = NULL;
	newProcess(parent);
}

ts2System_::ts2System_(TS_ARGS1)
        : tinyState_(parent)
{
    TS_CPARGS1
	commandLine = commandLine2->get_str();
	hProcess = hJob = waitHandle = NULL;
	newProcess(parent);
}

void
ts2System_::newProcess(sPtr<tinyState> p)
{
HANDLE _fd[3];
sPtr<ts2IO> iofd[3];
	_fd[0] = _fd[1] = _fd[2] = INVALID_HANDLE_VALUE;
	this->ret = this->do_exec(commandLine,&_fd[0],&_fd[1],&_fd[2],dmode);
	*retp = ret;
	if ( ret < 0 )
		return;
int i,j;
	for ( i = 0 ; i < 3 ; i ++ ) {
		for ( j = 0 ; j < i ; j ++ )
			if ( _fd[i] == _fd[j] )
				break;
		if ( j == i )	iofd[i] = thNEW(ts2IOdescriptor,(p,_fd[i]));
		else		iofd[i] = iofd[j];
	}
	if ( rfd ) {
		*rfd = iofd[0];
		for ( j = 0 ; j < 3 ; j ++ )
			if ( iofd[0] == iofd[j] ) iofd[j] = thNULL;
	}
	if ( wfd ) {
		*wfd = iofd[1];
		for ( j = 0 ; j < 3 ; j ++ )
			if ( iofd[1] == iofd[j] ) iofd[j] = thNULL;
	}
	if ( efd ) {
		*efd = iofd[2];
		for ( j = 0 ; j < 3 ; j ++ )
			if ( iofd[2] == iofd[j] ) iofd[j] = thNULL;
	}
	if ( iofd[0] != thNULL )	d_rfd = iofd[0];
	if ( iofd[2] != thNULL )	d_efd = iofd[2];
	if ( iofd[1] != thNULL )	iofd[1]->destroy();
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/

/* create one stdio pipe: parent end overlapped (server), child end inheritable.
   dir 0 = child reads (stdin): parent writes.  dir 1 = child writes (stdout/err). */
static int
ts2sys_make_pipe(HANDLE *parentEnd,HANDLE *childEnd,int dir)
{
char name[128];
LONG uid = InterlockedIncrement(&g_ts2sys_uid);
	wsprintfA(name,"\\\\.\\pipe\\ts2sys_%lu_%ld",(unsigned long)GetCurrentProcessId(),(long)uid);
DWORD openMode = FILE_FLAG_OVERLAPPED | (dir == 0 ? PIPE_ACCESS_OUTBOUND : PIPE_ACCESS_INBOUND);
HANDLE server = CreateNamedPipeA(name,openMode,PIPE_TYPE_BYTE|PIPE_WAIT,1,65536,65536,0,NULL);
	if ( server == INVALID_HANDLE_VALUE )
		return -1;
SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa); sa.lpSecurityDescriptor = NULL; sa.bInheritHandle = TRUE;
DWORD access = (dir == 0 ? GENERIC_READ : GENERIC_WRITE);
	/* Child end is OVERLAPPED too: a tinyState child reads/writes its inherited
	   std handles through s2IOstd -> ts2IOdescriptor (overlapped ReadFile+IOCP);
	   a *synchronous* child end silently drops parent->child delivery.
	   Overlapped handles still serve plain synchronous children (cmd/sort): a
	   ReadFile/WriteFile with a NULL OVERLAPPED on a byte-mode pipe blocks and
	   completes normally.  Windows-port design memo §E / s2IOstd follow-up. */
HANDLE client = CreateFileA(name,access,0,&sa,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,NULL);
	if ( client == INVALID_HANDLE_VALUE ) {
		CloseHandle(server);
		return -1;
	}
	*parentEnd = server;
	*childEnd  = client;
	return 0;
}

int
ts2System_::do_exec(
	const char *command,
	HANDLE *fd_r,
	HANDLE *fd_w,
	HANDLE *fd_err,
	int dmode)
{
HANDLE p_in=INVALID_HANDLE_VALUE,  c_in=INVALID_HANDLE_VALUE;
HANDLE p_out=INVALID_HANDLE_VALUE, c_out=INVALID_HANDLE_VALUE;
HANDLE p_err=INVALID_HANDLE_VALUE, c_err=INVALID_HANDLE_VALUE;
	if ( command[0] != '#' )
		return -6;	/* shell mode unsupported on MinGW (Windows-port design memo §E) */

	if ( dmode & DM_APPLY ) {
		c_in = *fd_r; c_out = *fd_w; c_err = *fd_err;
	}
	else {
		if ( ts2sys_make_pipe(&p_in,&c_in,0) < 0 )		return -1;
		if ( ts2sys_make_pipe(&p_out,&c_out,1) < 0 )	{ CloseHandle(p_in); CloseHandle(c_in); return -2; }
		if ( ts2sys_make_pipe(&p_err,&c_err,1) < 0 )	{ CloseHandle(p_in); CloseHandle(c_in);
								  CloseHandle(p_out); CloseHandle(c_out); return -3; }
	}

STARTUPINFOA si;
PROCESS_INFORMATION pi;
	ZeroMemory(&si,sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESTDHANDLES;
	si.hStdInput  = c_in;
	si.hStdOutput = c_out;
	si.hStdError  = c_err;

char *cmdline = _strdup(&command[1]);	/* CreateProcess wants a mutable buffer */
	BOOL ok = CreateProcessA(NULL, cmdline, NULL, NULL, TRUE,
			CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED, NULL, NULL, &si, &pi);
	free(cmdline);
	if ( !ok ) {
		if ( !(dmode & DM_APPLY) ) {
			CloseHandle(p_in); CloseHandle(c_in);
			CloseHandle(p_out); CloseHandle(c_out);
			CloseHandle(p_err); CloseHandle(c_err);
		}
		return -4;
	}

	/* Job Object: capture the child (and its descendants) for group kill. */
	hJob = CreateJobObjectA(NULL,NULL);
	if ( hJob )
		AssignProcessToJobObject(hJob,pi.hProcess);
	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);
	hProcess = pi.hProcess;

	if ( !(dmode & DM_APPLY) ) {
		CloseHandle(c_in); CloseHandle(c_out); CloseHandle(c_err);
		*fd_w   = p_in;
		*fd_r   = p_out;
		*fd_err = p_err;
	}
	return (int)pi.dwProcessId;
}


sPtr<stdEvent>
ts2System_::filter(sPtr<stdEvent>  ev)
{
	if ( ev == thNULL ) return ev;
	if ( is_destroyed() )
		if ( kill_flag == 0 ) {
			kill_flag = 1;
			if ( hJob )
				TerminateJobObject(hJob,1);
			else if ( hProcess )
				TerminateProcess(hProcess,1);
		}
	return ev;
}


void
ts2System_::destroy()
{
	destroy_mode = DM_NORMAL;
	tinyState_::destroy();
}
void
ts2System_::destroy(int mode)
{
	destroy_mode = mode;
	tinyState_::destroy();
}
void
ts2System_::detach()
{
	detach_flag = 1;
}

VOID CALLBACK
ts2System_::on_exit_cb(PVOID ctx,BOOLEAN /*timedOut*/)
{
ts2System_ * self = (ts2System_*)ctx;
#if TS2SYS_CHILDWAIT_MODE == 2
	/* threadpool thread: ONLY post to fwIO (thread-safe).  fwIO's main loop
	   dequeues it keyed by exit_key and re-runs ACT_FINISH_RET on the main
	   thread — the state machine is never driven from this foreign thread. */
	if ( self->io.is_notNull() )
		PostQueuedCompletionStatus((HANDLE)self->io->port(),0,(ULONG_PTR)self->exit_key,NULL);
#else	/* mode 1: drive the state machine DIRECTLY from this foreign thread.
	   eventHandler() takes application->mtx internally and this thread holds
	   no lock, so 鉄則3 (no other->eventHandler while holding app-mtx) is not
	   violated.  TSE_INVOKE just re-runs the current state (ACT_FINISH_RET),
	   which sees the child exited and flows to FIN_START/FIN_UNREGISTER.
	   FIN_UNREGISTER is a TS_THREAD, so UnregisterWaitEx runs on an app worker
	   thread, not here — no "unregister-from-within-callback" self-deadlock.
	   This exercises sCallSection (pthread TLS) on a non-pthread thread. */
	self->ifp->eventHandler(thNEW(stdEvent,(TSE_INVOKE,self->ifp,(INTEGER64)0)));
#endif
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	if ( d_rfd == thNULL && d_efd == thNULL )
		return rDO|ACT_FINISH;
	if ( d_rfd != thNULL )
		thNEW(ts2IOdevNull,(ifThis,d_rfd));
	if ( d_efd != thNULL )
		thNEW(ts2IOdevNull,(ifThis,d_efd));
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
	if ( is_destroyed() )
		return rDO|ACT_FINISH;
	if ( d_rfd != thNULL && !C_TEST(d_rfd->tinyState::state(),C_ZOM) )
		return ACT_START;
	if ( d_efd != thNULL && !C_TEST(d_efd->tinyState::state(),C_ZOM) )
		return ACT_START;
	return rDO|ACT_FINISH;
}

TS_STATE(ACT_FINISH)
{
	if ( this->ret < 0 )
		return rDO|FIN_START;
	/* Arm the OS threadpool wait on the child (unless detaching).  on_exit_cb
	   only PQCS's exit_key; fwIO's main loop re-runs ACT_FINISH_RET on the main
	   thread via the io->read(exit_key) registration. */
	if ( !detach_flag && hProcess && waitHandle == NULL ) {
		io = io.d_cast(application->fw());
#if TS2SYS_CHILDWAIT_MODE == 2
		exit_key = ts2io_alloc_key();
		if ( io.is_notNull() )
			io->read(ifThis,exit_key);
#else	/* mode 1: keep the reactor alive with a refcount, not a dummy read.
	   Without this the loop would see nothing registered and break out
	   (== the old (1)'s "reactor tore down before the child-exit callback
	   fired" == output destruction). */
		if ( io.is_notNull() )
			io->addRefio();
#endif
		RegisterWaitForSingleObject(&waitHandle,hProcess,on_exit_cb,
			this,INFINITE,WT_EXECUTEONLYONCE);
		wait_pin = ifThis;	/* pin the object alive until FIN_UNREGISTER */
	}
	return rDO|ACT_FINISH_RET;
}
TS_STATE(ACT_FINISH_RET)
{
	if ( detach_flag )
		return rDO|FIN_START;
	if ( hProcess && WaitForSingleObject(hProcess,0) == WAIT_OBJECT_0 ) {
	DWORD code = 0;
		GetExitCodeProcess(hProcess,&code);
		status = (int)code;
		return rDO|FIN_START;
	}
	/* not exited yet: wait for on_exit_cb → fwIO to re-run this state (main thread). */
	return 0;
}

TS_STATE(FIN_START)
{
	this->parent->eventHandler(
		thNEW( stdEvent,(TSE_RETURN,ifThis,(INTEGER64)status)));
	if ( io.is_notNull() )
#if TS2SYS_CHILDWAIT_MODE == 2
		io->detach(ifThis);
#else	/* mode 1: drop the keep-alive refcount; delRefio()'s wake() nudges the
	   main loop to re-check its exit condition (now refio==0, nothing
	   registered → the reactor returns cleanly). */
		io->delRefio();
#endif
	if ( d_rfd != thNULL )
		d_rfd->destroy();
	if ( d_efd != thNULL )
		d_efd->destroy();
	d_rfd = thNULL;
	d_efd = thNULL;
	return rDO|FIN_UNREGISTER;
}
TS_THREAD(FIN_UNREGISTER)
{
	/* mtx released: block-unregister so no exit callback fires after teardown.
	   INVALID_HANDLE_VALUE => wait for any in-flight callback to finish. */
	if ( waitHandle ) {
		UnregisterWaitEx(waitHandle,INVALID_HANDLE_VALUE);
		waitHandle = NULL;
	}
	if ( hProcess ) { CloseHandle(hProcess); hProcess = NULL; }
	if ( hJob )     { CloseHandle(hJob);     hJob = NULL; }
	wait_pin = thNULL;	/* wait is unregistered; safe to drop the self-ref */
	return rDO|FIN_TINYSTATE_START;
}
