#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwHello.h"

int
main(int argc, char** argv)
{
	thNEW(tsApplication,(thNULL,[](sPtr<tsApplication> app){
		thNEW(hwHello,(app));
	}));
}
