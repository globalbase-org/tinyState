#include	<cstdio>
#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/stdString.h"
#include	"hw/c++/hwUnixTest.h"

/* argv[1] = socket path (default "unixtest.sock", relative to cwd) */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	const char * p = (argc > 1) ? argv[1] : "unixtest.sock";
	thNEW(tsApplication,(thNULL,[p](sPtr<tsApplication> app){
		thNEW(hwUnixTest,(app,thNEW(stdString,(p))));
	}));
}
