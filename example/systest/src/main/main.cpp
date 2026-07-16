#include	<cstdio>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwSysTest.h"

int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	thNEW(tsApplication,(thNULL,[](sPtr<tsApplication> app){
		thNEW(hwSysTest,(app));
	}));
}
