#include	<cstdio>
#include	"ts2/c++/tsApplication.h"
#include	"hw/c++/hwMain.h"

extern volatile int tld_spawned;
extern volatile int tld_done;
extern volatile int tld_destroyed;
extern int          tld_fail;
extern int          tld_teardown_started;
extern long long    tld_teardown_t0;
extern long long    tld_now_us();

#define NBACKLOG	24

int
main(int argc, char** argv)
{
	::setvbuf(stdout, NULL, _IONBF, 0);
	thNEW(tsApplication,(thNULL,[](sPtr<tsApplication> app){
		thNEW(hwMain,(app));
	}));

	/* ここに来た = イベントループが畳まれた = teardown がハングしなかった。
	 * phase 5 の判定は app 終了後でないと出せないのでここで出す。 */
	::printf("=== phase 5 results (after the application has shut down) ===\n");
	if ( tld_spawned == tld_done )
		::printf("  [ OK ] every started worker finished             started %d, finished %d\n",
			 tld_spawned, tld_done);
	else {
		::printf("  [FAIL] a started worker never finished           started %d, finished %d\n",
			 tld_spawned, tld_done);
		tld_fail ++;
	}
	if ( tld_spawned + tld_destroyed == NBACKLOG )
		::printf("  [ OK ] every worker accounted for               %d run + %d destroyed = %d\n",
			 tld_spawned, tld_destroyed, NBACKLOG);
	else {
		::printf("  [FAIL] workers unaccounted for                  %d run + %d destroyed != %d\n",
			 tld_spawned, tld_destroyed, NBACKLOG);
		tld_fail ++;
	}
	if ( tld_teardown_started < NBACKLOG )
		::printf("  [ OK ] there really was a backlog at teardown   only %d of %d had completed\n",
			 tld_teardown_started, NBACKLOG);
	else {
		::printf("  [FAIL] no backlog — the test proved nothing     %d of %d had completed\n",
			 tld_teardown_started, NBACKLOG);
		tld_fail ++;
	}
	::printf("  [ OK ] teardown completed (no hang)         drain of the backlog took %lld ms\n",
		 (tld_now_us() - tld_teardown_t0) / 1000);

	::printf("%s\n", tld_fail ? "*** FAILED ***" : "*** ALL GREEN ***");
	return tld_fail ? 1 : 0;
}
