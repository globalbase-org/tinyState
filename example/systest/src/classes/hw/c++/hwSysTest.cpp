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
	int retPid; sPtr<ts2IO> rfd; sPtr<ts2IO> wfd; sPtr<tinyState> sys;
	char rbuf[256]; char wbuf[64]; char acc[512]; int acclen;
	char cmd[512]; char * bigbuf; int biglen;
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE
#endif
hwSysTest_::hwSysTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }
TS_STATE(INI_START) { acclen = 0; bigbuf = 0; return rDO|ACT_A_START; }

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
 * Phase E : parent -> *tinyState* child (overlapped-read regression guard).  *
 * Unlike a shell child (plain synchronous _read), a tinyState child    *
 * reads its stdin through s2IOstd -> ts2IOdescriptor (overlapped       *
 * ReadFile + IOCP).  On MinGW that only works if ts2System created the *
 * child pipe end OVERLAPPED — the overlapped-child-pipe fix.  Child = the `tschild`*
 * companion (reads stdin to EOF, prints COUNT=<n>).  cgalp sends via   *
 * set_divisible(), so mirror that here.                                *
 * Opt-in: SYSTEST_BIG_EXE=<tschild path>; SYSTEST_BIG_BYTES (def 131072).*
 * ------------------------------------------------------------------ */
TS_STATE(ACT_E_START) {
	const char * exe = ::getenv("SYSTEST_BIG_EXE");
	if ( !exe ) return rDO|FIN_START;			/* portable default: skip */
	const char * bs = ::getenv("SYSTEST_BIG_BYTES");
	biglen = bs ? atoi(bs) : 131072;			/* > 64KB pipe buffer */
	bigbuf = (char*)malloc(biglen);
	for ( int i = 0 ; i < biglen ; i++ ) bigbuf[i] = (char)('A' + (i % 26));
	acclen = 0;
	snprintf(cmd,sizeof(cmd),"#%s",exe);
	sys = thNEW(ts2System,(ifThis,&retPid,cmd,&rfd,(sPtr<ts2IO>*)0,&wfd));
	if ( retPid < 0 ) { ::printf("[systest] E: spawn failed\n"); free(bigbuf); bigbuf=0; return rDO|FIN_START; }
	wfd->set_divisible();					/* the cgalp path (pigfAgent.cpp) */
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
	return rDO|FIN_START;
}
TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
