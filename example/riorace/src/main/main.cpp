#include	<cstdio>
#include	<cstdlib>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwRioRace.h"

/* riorace [base_port] [rounds]
 *   base_port (default 47600): victim binds base, flooder binds base+1
 *   rounds    (default 50)   : create/receive/destroy churn iterations
 * Exercises rio_teardown() against in-flight RIO completions.  Pair it with the
 * instrumented library (TS2_RIO_RACE_SLEEP_MS) to widen the window. */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int base   = (argc > 1) ? atoi(argv[1]) : 47600;
	int rounds = (argc > 2) ? atoi(argv[2]) : 50;
	thNEW(tsApplication,(thNULL,[base,rounds](sPtr<tsApplication> app){
		thNEW(hwRioRace,(app,base,base+1,rounds,0));
	}));
}
