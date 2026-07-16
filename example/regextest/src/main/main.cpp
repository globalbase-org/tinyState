#include	<cstdio>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwRegexTest.h"

/*
 * regextest — stdRx regression test.  Asserts POSIX ERE leftmost-longest
 * matching through the framework regex path (system regex on Linux/macOS/
 * Cygwin, vendored TRE on MinGW).  Exits non-zero if any check fails.
 */
int
main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	thNEW(tsApplication,(thNULL,[](sPtr<tsApplication> app){
		thNEW(hwRegexTest,(app));
	}));
}
