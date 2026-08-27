#include	<cstdio>
#include	<cstdlib>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwRioRace.h"

/* riorace [base_port] [rounds] [fanout]
 *   base_port (default 47600): victim binds base, flooder binds base+1
 *   rounds    (default 50)   : create/receive/destroy churn iterations
 *   fanout    (default 1)    : sockets torn down in the SAME turn.  1 keeps the original
 *                              one-at-a-time churn; above 1 the victim binds base..base+N-1
 *                              instead and destroys the whole set at once, so several FIN
 *                              TS_THREADs queue up and the pool drains a backlog while
 *                              shutting down.
 *   limit     (default 0)    : cap the worker pool with tsThread::limitThreadsNumber().
 *                              0 leaves the pool alone (default unlimited growth), which is
 *                              what this harness has always run with.
 *   raise     (default 0)    : if >0, widen the cap to this value in the same turn the
 *                              sockets are destroyed, logging which state the pool was in.
 * Exercises rio_teardown() against in-flight RIO completions.  Pair it with the
 * instrumented library (TS2_RIO_RACE_SLEEP_MS) to widen the window. */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int base   = (argc > 1) ? atoi(argv[1]) : 47600;
	int rounds = (argc > 2) ? atoi(argv[2]) : 50;
	int fanout = (argc > 3) ? atoi(argv[3]) : 1;
	int tlimit = (argc > 4) ? atoi(argv[4]) : 0;
	int raiseto= (argc > 5) ? atoi(argv[5]) : 0;
	if ( fanout < 1 ) fanout = 1;
	thNEW(tsApplication,(thNULL,[base,rounds,fanout,tlimit,raiseto](sPtr<tsApplication> app){
		/* fanout>1 は base..base+fanout-1 を victim が使うので、flooder は十分離す */
		thNEW(hwRioRace,(app,base,base + (fanout > 1 ? fanout : 1),rounds,0,fanout,
				tlimit,raiseto));
	}));
}
