#include	"ts2/c++/tsApplication.h"
#include	"ss/c++/ssApplication.h"

int
main(int argc, char** argv)
{
	thNEW(tsApplication,(thNULL,[argc,argv](sPtr<tsApplication> app){
		thNEW(ssApplication,(app, argc, argv));
	}));
}
