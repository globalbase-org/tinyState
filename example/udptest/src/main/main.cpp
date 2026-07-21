#include	<cstdio>
#include	<cstdlib>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwUdpTest.h"

/* argv[1] = base port (default 47500); binds base and base+1.
   argv[2]: ignored (datagram path is automatic — RIO by default, plain via env
            TS2_DISABLE_RIO; on Windows RIO, on Linux/macOS native).
   argv[3]: 0 = normal, 1 = drop-demo (responder delays its recv so the client
            sends with no receive posted — plain is kernel-buffered, RIO drops). */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int base = (argc > 1) ? atoi(argv[1]) : 47500;
	int mode = (argc > 2) ? atoi(argv[2]) : 0;	/* ignored */
	int drop = ((argc > 3) ? atoi(argv[3]) : 0) ? 1 : 0;
	thNEW(tsApplication,(thNULL,[base,mode,drop](sPtr<tsApplication> app){
		thNEW(hwUdpTest,(app,base,base+1,mode,drop));
	}));
}
