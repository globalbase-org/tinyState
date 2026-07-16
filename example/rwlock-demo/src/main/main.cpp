#include	<cstdio>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwMain.h"

int
main(int argc, char** argv)
{
	::setvbuf(stdout, NULL, _IONBF, 0);
	thNEW(tsApplication,(thNULL,[](sPtr<tsApplication> app){
		// 4 readers + 2 writers share one stdMutex, each holds it ~250ms.
		thNEW(hwMain,(app, 4, 2, 250));
	}));
}
