#include	<cstdio>
#include	<cstdlib>
#include	<cstring>
#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/stdString.h"
#include	"hw/c++/hwSrvTest.h"

/* argv[1] = "tcp" | "unix"; argv[2] = port (tcp) or socket path (unix) */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int mode = (argc > 1 && strcmp(argv[1],"unix") == 0) ? 1 : 0;
	int port = 47000;
	const char * path = "srvtest.sock";
	if ( argc > 2 ) {
		if ( mode == 0 ) port = atoi(argv[2]);
		else             path = argv[2];
	}
	thNEW(tsApplication,(thNULL,[mode,port,path](sPtr<tsApplication> app){
		thNEW(hwSrvTest,(app,mode,port,thNEW(stdString,(path))));
	}));
}
