#include	<cstdio>
#include	<cstdlib>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwUdpTest.h"

/* argv[1] = base port (default 47500); binds base and base+1 */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int base = (argc > 1) ? atoi(argv[1]) : 47500;
	thNEW(tsApplication,(thNULL,[base](sPtr<tsApplication> app){
		thNEW(hwUdpTest,(app,base,base+1));
	}));
}
