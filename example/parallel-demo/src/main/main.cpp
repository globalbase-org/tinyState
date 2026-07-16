#include	<cstdio>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwMain.h"

int
main(int argc, char** argv)
{
	::setvbuf(stdout, NULL, _IONBF, 0);
	thNEW(tsApplication,(thNULL,[](sPtr<tsApplication> app){
		thNEW(hwMain,(app));
	}));
}
