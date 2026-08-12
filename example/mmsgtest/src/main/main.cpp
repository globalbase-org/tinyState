#include	<cstdio>
#include	<cstdlib>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwMmsgTest.h"

/* set by hwMmsgTest: 0 = every batch verified, non-zero = failure/timeout */
extern int mmsgtest_failed;

/* argv[1] = base port (default 47600); binds base and base+1 */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int base = (argc > 1) ? atoi(argv[1]) : 47600;
	thNEW(tsApplication,(thNULL,[base](sPtr<tsApplication> app){
		thNEW(hwMmsgTest,(app,base,base+1));
	}));
	return mmsgtest_failed;
}
