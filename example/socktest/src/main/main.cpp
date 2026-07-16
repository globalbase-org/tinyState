#include	<cstdio>
#include	<cstdlib>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwSockTest.h"

/* argv[1] = port (default 48210), argv[2] = mode (0=pingpong, 1=reactor, 2=teardown) */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int port = (argc > 1) ? atoi(argv[1]) : 48210;
	int mode = (argc > 2) ? atoi(argv[2]) : 0;
	thNEW(tsApplication,(thNULL,[port,mode](sPtr<tsApplication> app){
		thNEW(hwSockTest,(app,port,mode));
	}));
}
