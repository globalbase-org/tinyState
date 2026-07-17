/*
 * hwStress — teardown-race stress (concurrent-teardown family).
 *
 * Spawns N ts2System children that stay alive a few seconds, lets their
 * child-exit waits + IOCP/devnull descriptors arm, then mass-destroy()s ALL of
 * them at once and immediately FINs the app.  This forces the concurrent-teardown pattern:
 * many worker-thread ts2System/ts2IOdescriptor FIN chains (+ MinGW on_exit_cb
 * OS-threadpool callbacks) running concurrently with the main app/fwIO teardown.
 * Sequential single-child systest never hit this; this
 * does.  Run it many times and count non-zero exits / SEGV.
 *
 * Env: STRESS_N (children, default 64), STRESS_MS (delay before mass-kill, 400).
 */
#define STRESS_MAX	256		/* must precede the generated header (member array) */
/* child that stays alive ~5s (killed early by the mass-destroy). */
#ifdef _WIN32
#define STRESS_CHILD	"#cmd /c ping -n 6 127.0.0.1"
#else
#define STRESS_CHILD	"#sleep 5"
#endif
#include	"_ts2/c++/hwStress_.h"
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
CLASS_TINYSTATE(hw/c++/hwStress,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2System.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdInterval.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwStress_(sPtr<tinyState> parent);
protected:
	TS_DEFARGS
	sPtr<tinyState> kids[STRESS_MAX]; int n; int retp; int delayMs; char cmd[128];
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE
#endif
hwStress_::hwStress_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

TS_STATE(INI_START) {
	n = ::getenv("STRESS_N") ? atoi(::getenv("STRESS_N")) : 64;
	if ( n < 1 ) n = 1;
	if ( n > STRESS_MAX ) n = STRESS_MAX;
	delayMs = ::getenv("STRESS_MS") ? atoi(::getenv("STRESS_MS")) : 400;
	return rDO|ACT_SPAWN;
}
TS_STATE(ACT_SPAWN) {
	strcpy(cmd,STRESS_CHILD);
	for ( int i = 0 ; i < n ; i++ )
		kids[i] = thNEW(ts2System,(ifThis,&retp,cmd,
				(sPtr<ts2IO>*)0,(sPtr<ts2IO>*)0,(sPtr<ts2IO>*)0));
	::printf("[stress] spawned %d children (%s)\n",n,cmd);
	stdInterval::wait(ifThis,(INTEGER64)delayMs*1000,TSE_TIMER);
	return rDO|ACT_KILL;
}
TS_STATE(ACT_KILL) {
	if ( ev->type != TSE_TIMER ) return 0;
	::printf("[stress] mass destroy %d children + app FIN (concurrent teardown)\n",n);
	for ( int i = 0 ; i < n ; i++ )
		if ( kids[i].is_notNull() ) { kids[i]->destroy(); kids[i] = thNULL; }
	return rDO|FIN_START;
}
TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
