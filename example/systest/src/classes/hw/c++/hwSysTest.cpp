#include	"_ts2/c++/hwSysTest_.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
CLASS_TINYSTATE(hw/c++/hwSysTest,ts2/c++/tinyState)

/* Windows uses `cmd /c` (PATH-resolve echo/sort); POSIX direct-execvp's them.
   Keeping the test runnable on both means POSIX (where parent->child works)
   PASSES phase B — validating the harness so a Windows FAIL is a real bug. */
#ifdef _WIN32
#define SYSTEST_ECHO_CMD	"#cmd /c echo hello_from_child"
#define SYSTEST_SORT_CMD	"#cmd /c sort"
#else
#define SYSTEST_ECHO_CMD	"#echo hello_from_child"
#define SYSTEST_SORT_CMD	"#sort"
#endif

/* Phase F table — the exit status TSE_RETURN carries.  It is the POSIX wait
   status shape on every platform (Windows normalises its raw exit code into it),
   so the expectations below are written once and read the same way everywhere.
   Windows gets the extra rows because only there can the raw code be wider than
   a POSIX status. */
struct systest_st { const char * cmd; INTEGER64 expect; const char * what; };
static const struct systest_st systest_status_tab[] = {
#ifdef _WIN32
  { "#cmd /c exit 3",		0x300,		"exit 3 -> (3 << 8)" },
  /* 259 is a perfectly ordinary Windows exit code that does not fit a POSIX
     status; truncated to 8 bits exactly as POSIX truncates exit(1000).  Left
     raw it would be 0x103, which reads back as "killed by signal 3". */
  { "#cmd /c exit 259",		0x300,		"exit 259 -> truncated to (3 << 8)" },
  /* 0xC0000005 (access violation) — wider than any POSIX status, so it is
     passed through raw and stays positive in the INTEGER64 payload. */
  /* Above the window a POSIX status can occupy, so it is handed over raw. */
  { "#cmd /c exit 66051",	0x10203LL,	"exit 0x10203 -> raw, not POSIX-shaped" },
  /* 0xC0000005 is the access-violation code a crashing child exits with.  cmd's
     `exit` takes the value signed, which is how the raw 32-bit code is set here
     without having to crash a child for real (`exit 3221225477` saturates at
     0x7FFFFFFF).  The property under test is that it survives as a positive
     0xC0000005 -- folded into an int it would be -1073741819, which a caller
     reads as "has not exited yet". */
  { "#cmd /c exit -1073741819",	0xC0000005LL,	"0xC0000005 -> raw, stays positive" },
#else
  /* The `#` form splits on spaces and cannot carry a quoted script, so this one
     row goes through `sh -c` on purpose. */
  { "exit 3",			0x300,		"exit 3 -> (3 << 8)" },
#endif
};
#define SYSTEST_STATUS_N ((int)(sizeof(systest_status_tab)/sizeof(systest_status_tab[0])))
#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2System.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdInterval.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwSysTest_(sPtr<tinyState> parent);
protected:
	TS_DEFARGS
	int retPid; sPtr<ts2IO> rfd; sPtr<ts2IO> wfd; sPtr<ts2IO> efd; sPtr<tinyState> sys;
	char rbuf[256]; char wbuf[64]; char acc[512]; int acclen;
	char cmd[512]; char * bigbuf; int biglen;
	int fidx; int ffail; INTEGER64 fdeadline; INTEGER64 gdeadline;
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE
#endif
hwSysTest_::hwSysTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }
TS_STATE(INI_START) { acclen = 0; bigbuf = 0; fidx = 0; ffail = 0; return rDO|ACT_A_START; }

/* Each phase's verdict is decided at rfd EOF — the moment the child's output is
   fully drained — NOT at child-exit.  child-exit is edge-racy for a fast child
   (the reactor can empty before the exit event lands); tying the verdict or the
   phase transition to it would drop results / skip later phases.  So we judge on
   the data we read, then move on. */

/* ------------------------------------------------------------------ *
 * Phase A : child -> parent (child stdout -> parent read).           *
 * The original systest.  Proves the INBOUND (parent-read) path.      *
 * ------------------------------------------------------------------ */
TS_STATE(ACT_A_START) {
	sys = thNEW(ts2System,(ifThis,&retPid,SYSTEST_ECHO_CMD,
			&rfd,(sPtr<ts2IO>*)0,(sPtr<ts2IO>*)0));
	if ( retPid < 0 ) { ::printf("[systest] A: spawn failed\n"); return rDO|FIN_START; }
	return rDO|ACT_A_READ;
}
TS_STATE(ACT_A_READ) {
	int n = rfd->read(rbuf,sizeof(rbuf)-1);
	if ( n > 0 ) { rbuf[n]=0; ::printf("[systest] A read: %s",rbuf); return rDO|ACT_A_READ; }
	::printf("[systest] A: EOF — child->parent OK\n");
	rfd = thNULL;
	return rDO|ACT_B_START;
}

/* ------------------------------------------------------------------ *
 * Phase B : parent -> child -> parent (bidirectional, shell child).   *
 * Child `sort` copies stdin -> stdout once stdin hits EOF.  We write  *
 * a marker to the child's stdin (wfd), close the write end to signal  *
 * EOF, then read it back on stdout (rfd).  Guards the OUTBOUND        *
 * (parent-write) path that systest historically never exercised.      *
 * ------------------------------------------------------------------ */
TS_STATE(ACT_B_START) {
	sys = thNEW(ts2System,(ifThis,&retPid,SYSTEST_SORT_CMD,
			&rfd,(sPtr<ts2IO>*)0,&wfd));		/* rfd + wfd (efd unused) */
	if ( retPid < 0 ) { ::printf("[systest] B: spawn failed\n"); return rDO|FIN_START; }
	strcpy(wbuf,"PING_bidir_ok\n");
	return rDO|ACT_B_WRITE;
}
TS_STATE(ACT_B_WRITE) {			/* ev-independent, single write_c on member buf (鉄則5) */
	wfd->write_c(wbuf,(int)strlen(wbuf));
	return rDO|ACT_B_WEND;
}
TS_STATE(ACT_B_WEND) {			/* close child stdin -> EOF so sort emits */
	wfd->destroy(); wfd = thNULL;
	return rDO|ACT_B_READ;
}
TS_STATE(ACT_B_READ) {
	int n = rfd->read(rbuf,sizeof(rbuf)-1);
	if ( n > 0 ) {
		if ( acclen + n < (int)sizeof(acc) ) { memcpy(acc+acclen,rbuf,n); acclen += n; }
		rbuf[n]=0; ::printf("[systest] B read: %s",rbuf);
		return rDO|ACT_B_READ;
	}
	acc[acclen] = 0;
	if ( strstr(acc,"PING_bidir_ok") )
		::printf("[systest] BIDIRECTIONAL OK — parent->child->parent delivered\n");
	else
		::printf("[systest] BIDIRECTIONAL FAIL — parent->child NOT delivered\n");
	rfd = thNULL;
	return rDO|ACT_E_START;
}

/* ------------------------------------------------------------------ *
 * Phase E : parent -> *tinyState* child (overlapped-read guard).       *
 * Unlike a shell child (plain synchronous _read), a tinyState child    *
 * reads its stdin through s2IOstd -> ts2IOdescriptor (overlapped       *
 * ReadFile + IOCP).  On MinGW that only works if ts2System created the *
 * child pipe end OVERLAPPED — the overlapped-child-pipe fix.  Child =  *
 * the `tschild` companion (reads stdin to EOF, prints COUNT=<n>).  A   *
 * consumer sends via set_divisible(), so mirror that here.             *
 * Opt-in: SYSTEST_BIG_EXE=<tschild path>; SYSTEST_BIG_BYTES (def 131072).*
 * ------------------------------------------------------------------ */
TS_STATE(ACT_E_START) {
	const char * exe = ::getenv("SYSTEST_BIG_EXE");
	if ( !exe ) return rDO|ACT_F_START;			/* portable default: skip */
	const char * bs = ::getenv("SYSTEST_BIG_BYTES");
	biglen = bs ? atoi(bs) : 131072;			/* > 64KB pipe buffer */
	bigbuf = (char*)malloc(biglen);
	for ( int i = 0 ; i < biglen ; i++ ) bigbuf[i] = (char)('A' + (i % 26));
	acclen = 0;
	snprintf(cmd,sizeof(cmd),"#%s",exe);
	sys = thNEW(ts2System,(ifThis,&retPid,cmd,&rfd,(sPtr<ts2IO>*)0,&wfd));
	if ( retPid < 0 ) { ::printf("[systest] E: spawn failed\n"); free(bigbuf); bigbuf=0; return rDO|ACT_F_START; }
	wfd->set_divisible();					/* the consumer's agent path */
	::printf("[systest] E: sending %d bytes to a tinyState child ...\n",biglen);
	return rDO|ACT_E_WRITE;
}
TS_STATE(ACT_E_WRITE) {			/* ev-independent, single write_c on member buf (鉄則5) */
	wfd->write_c(bigbuf,biglen);
	return rDO|ACT_E_WEND;
}
TS_STATE(ACT_E_WEND) {
	wfd->destroy(); wfd = thNULL;
	return rDO|ACT_E_READ;
}
TS_STATE(ACT_E_READ) {
	int n = rfd->read(rbuf,sizeof(rbuf)-1);
	if ( n > 0 ) {
		if ( acclen + n < (int)sizeof(acc) ) { memcpy(acc+acclen,rbuf,n); acclen += n; }
		rbuf[n]=0; ::printf("[systest] E read: %s",rbuf);
		return rDO|ACT_E_READ;
	}
	acc[acclen] = 0;
	{
	const char * p = strstr(acc,"COUNT=");
	long got = p ? atol(p+6) : -1;
		if ( got == (long)biglen )
			::printf("[systest] TINYSTATE-CHILD OK — overlapped-read child received all %d bytes\n",biglen);
		else
			::printf("[systest] TINYSTATE-CHILD FAIL — child got %ld / %d bytes (overlapped child-pipe bug)\n",got,biglen);
	}
	free(bigbuf); bigbuf = 0;
	/* release IO/child NOW so their IOCP-descriptor teardown runs while fwIO is
	   still alive, then linger briefly so it does NOT coincide with the reactor's
	   IOCP port-close (a known teardown race; see ts2System.cpp header). */
	rfd = thNULL; sys = thNULL;
	stdInterval::wait(ifThis,300*1000,TSE_TIMER);
	return rDO|ACT_E_LINGER;
}
TS_STATE(ACT_E_LINGER) {
	if ( ev->type != TSE_TIMER ) return 0;
	return rDO|ACT_F_START;
}

/* ------------------------------------------------------------------ *
 * Phase F : the exit status handed to the parent.                     *
 * Unlike A/B/E this one IS judged on child-exit — the status is the    *
 * thing under test, and it only exists in TSE_RETURN.  No pipes are    *
 * opened, so there is no output to drain and nothing to race with.     *
 * Each row spawns, waits for its own TSE_RETURN, compares, and steps   *
 * to the next row.                                                     *
 * ------------------------------------------------------------------ */
TS_STATE(ACT_F_START) {
	if ( fidx >= SYSTEST_STATUS_N ) {
		if ( ffail )	::printf("[systest] EXIT-STATUS FAIL — %d of %d rows wrong\n",
					ffail,SYSTEST_STATUS_N);
		else		::printf("[systest] EXIT-STATUS OK — all %d rows as expected\n",
					SYSTEST_STATUS_N);
		return rDO|ACT_G_START;
	}
	sys = thNEW(ts2System,(ifThis,&retPid,systest_status_tab[fidx].cmd,
			(sPtr<ts2IO>*)0,(sPtr<ts2IO>*)0,(sPtr<ts2IO>*)0));
	if ( retPid < 0 ) {
		::printf("[systest] F[%d]: spawn failed (%s)\n",fidx,systest_status_tab[fidx].cmd);
		ffail ++; fidx ++;
		return rDO|ACT_F_START;
	}
	/* deadline: a wait with no deadline shows up as a hang, and a missing
	   TSE_RETURN is exactly one of the things this phase is here to catch.
	   TIMER2 (not TIMER) so a timer armed by an earlier phase cannot fire it. */
	fdeadline = stdInterval::now() + 10*1000*1000;
	stdInterval::wait(ifThis,10*1000*1000,TSE_TIMER2);
	return rDO|ACT_F_WAIT;
}
TS_STATE(ACT_F_WAIT) {
	if ( ev->type == TSE_TIMER2 && stdInterval::now() >= fdeadline ) {
		::printf("[systest] F[%d] FAIL no TSE_RETURN within 10s  %s\n",
			fidx,systest_status_tab[fidx].what);
		ffail ++; fidx ++;
		if ( sys.is_notNull() ) sys->destroy();
		sys = thNULL;
		return rDO|ACT_F_START;
	}
	if ( ev->type != TSE_RETURN || ev->source != sys ) return 0;
	{
	INTEGER64 st = ev->msg_int;
	const struct systest_st * row = &systest_status_tab[fidx];
		if ( st == row->expect )
			::printf("[systest] F[%d] OK   status=0x%llX  %s\n",
				fidx,(unsigned long long)st,row->what);
		else {
			::printf("[systest] F[%d] FAIL status=0x%llX expected=0x%llX  %s\n",
				fidx,(unsigned long long)st,
				(unsigned long long)row->expect,row->what);
			/* the two shapes this guards against, spelled out */
			if ( st < 0 )
				::printf("[systest]        (negative — a raw exception code folded into an int)\n");
			else if ( st < 0x10000 && (st & 0x7f) )
				::printf("[systest]        (reads as \"signal %d\" — Windows has no signals)\n",
					(int)(st & 0x7f));
#ifndef _WIN32
			else if ( st == 0 && row->expect != 0 )
				::printf("[systest]        (status 0 on POSIX: the child was auto-reaped before\n"
					"[systest]         ts2System's waitpid could see it — tsSignalCore's\n"
					"[systest]         teardown arms SA_NOCLDWAIT on SIGCHLD.  A separate\n"
					"[systest]         POSIX defect, not the normalisation this phase guards.)\n");
#endif
			ffail ++;
		}
	}
	sys = thNULL;
	fidx ++;
	return rDO|ACT_F_START;
}
/* ------------------------------------------------------------------ *
 * Phase G : exit delivered with nothing of ours registered in fwIO.    *
 * Phases A/B/F all leave rfd or efd with ts2System, and draining that  *
 * to EOF is itself gated on the child exiting -- so the state machine  *
 * always had a second way to notice, and the child-exit wait was never *
 * load-bearing.  Take rfd AND efd and it is: INI_START goes straight   *
 * to ACT_FINISH with the child still running, and on Windows the       *
 * RegisterWaitForSingleObject armed there is the ONLY thing that can   *
 * deliver TSE_RETURN.  If that registration is ever lost the app sits  *
 * in its reactor forever, so this is the shape worth guarding.  Same   *
 * `sort` child as phase B: it lives until we close its stdin.          *
 * ------------------------------------------------------------------ */
TS_STATE(ACT_G_START) {
	sys = thNEW(ts2System,(ifThis,&retPid,SYSTEST_SORT_CMD,&rfd,&efd,&wfd));
	if ( retPid < 0 ) { ::printf("[systest] G: spawn failed\n"); return rDO|FIN_START; }
	strcpy(wbuf,"G_exit_notify\n");
	return rDO|ACT_G_WRITE;
}
TS_STATE(ACT_G_WRITE) {			/* ev-independent, single write_c on member buf (鉄則5) */
	wfd->write_c(wbuf,(int)strlen(wbuf));
	return rDO|ACT_G_WEND;
}
TS_STATE(ACT_G_WEND) {			/* close child stdin -> the child exits */
	wfd->destroy(); wfd = thNULL;
	gdeadline = stdInterval::now() + 10*1000*1000;
	stdInterval::wait(ifThis,10*1000*1000,TSE_TIMER2);
	return rDO|ACT_G_WAIT;
}
TS_STATE(ACT_G_WAIT) {
	if ( ev->type == TSE_TIMER2 && stdInterval::now() >= gdeadline ) {
		::printf("[systest] EXIT-NOTIFY FAIL — no TSE_RETURN within 10s with"
			" nothing registered in fwIO\n");
		::printf("[systest]        (the child-exit wait is the only notifier in"
			" this shape — it was not armed, or it was lost)\n");
		if ( sys.is_notNull() ) sys->destroy();
		sys = thNULL; rfd = thNULL; efd = thNULL;
		return rDO|FIN_START;
	}
	if ( ev->type != TSE_RETURN || ev->source != sys ) return 0;
	if ( ev->msg_int == 0 )
		::printf("[systest] EXIT-NOTIFY OK — TSE_RETURN delivered (status=0x%llX)\n",
			(unsigned long long)ev->msg_int);
	else
		::printf("[systest] EXIT-NOTIFY FAIL — delivered but status=0x%llX, expected 0\n",
			(unsigned long long)ev->msg_int);
	sys = thNULL; rfd = thNULL; efd = thNULL;
	return rDO|FIN_START;
}
TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
