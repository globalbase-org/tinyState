#include	"_ts2/c++/hwMain_.h"
#include	"ts2/c++/tsThread.h"
#include	<unistd.h>
#include	<sys/time.h>

CLASS_TINYSTATE(hw/c++/hwMain,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT

#include <cstdio>
#include "ts2/c++/ts2Parallel.h"
#include "ts2/c++/sTimer.h"

class TS_THISCLASS : public TS_BASECLASS {
public:
	hwMain_(sPtr<tinyState> parent);
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
TS_END_INTERFACE
#endif

hwMain_::hwMain_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
}

#define NWORKERS	12
#define NBACKLOG	24		/* phase 5: 上限 2 に対してわざと積む数 */
#define SETTLE_US	(300*1000)	/* 縮小後、余剰 worker が job を終えて抜けるまでの猶予 */

/* main() が app 終了後に判定を出すので static にしない */
volatile int tld_spawned   = 0;
volatile int tld_cur       = 0;
volatile int tld_max       = 0;
volatile int tld_done      = 0;
volatile int tld_destroyed = 0;	/* body が is_destroyed() を見て抜けた数 */
int          tld_fail      = 0;
int          tld_teardown_started = 0;	/* phase 5 で teardown に入った時点の完走数 */
long long    tld_teardown_t0 = 0;	/* teardown 開始時刻 (us)。drain にかかった時間を測る */

long long
tld_now_us()
{
struct timeval tv;
	::gettimeofday(&tv,NULL);
	return (long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void
ok(const char * what, int cond, const char * detail)
{
	if ( cond ) {
		::printf("  [ OK ] %-44s %s\n", what, detail);
		return;
	}
	::printf("  [FAIL] %-44s %s\n", what, detail);
	tld_fail ++;
}

static void
eq(const char * what, int got, int want)
{
char buf[64];
	::snprintf(buf,sizeof(buf),"= %d (expected %d)",got,want);
	ok(what, got == want, buf);
}

static void
reset()
{
	tld_spawned = 0;
	tld_cur = 0;
	tld_max = 0;
	tld_done = 0;
	tld_destroyed = 0;
}

static void
enter()
{
int cur = __sync_add_and_fetch(&tld_cur,1);
	for ( ; ; ) {
	int m = tld_max;
		if ( cur <= m )
			break;
		if ( __sync_bool_compare_and_swap(&tld_max,m,cur) )
			break;
	}
}

/* 各 worker は「掴んだまま 120ms ブロックする」ので、同時実行数がそのまま
 * 使用中 worker スレッド数になる。chain 方式 (自分の次を spawn していく)。 */
static int
worker_body(sPtr<ts2Parallel> me, sPtr<stdEvent> ev)
{
int idx;
	if ( me->is_destroyed() ) {
		__sync_fetch_and_add(&tld_destroyed,1);
		return 1;
	}
	idx = __sync_fetch_and_add(&tld_spawned,1);
	if ( idx + 1 < NWORKERS )
		me->spawn();
	enter();
	::usleep(120*1000);
	__sync_fetch_and_sub(&tld_cur,1);
	__sync_fetch_and_add(&tld_done,1);
	return 1;
}

/* phase 5 用。chain させず、ACT_P5 から独立した root を NBACKLOG 個まとめて立てる。
 * chain 方式だと上限そのものが spawn の速度を律速してしまい、ready に積み上がらない。 */
static int
backlog_body(sPtr<ts2Parallel> me, sPtr<stdEvent> ev)
{
	if ( me->is_destroyed() ) {
		__sync_fetch_and_add(&tld_destroyed,1);
		return 1;
	}
	__sync_fetch_and_add(&tld_spawned,1);
	enter();
	::usleep(120*1000);
	__sync_fetch_and_sub(&tld_cur,1);
	__sync_fetch_and_add(&tld_done,1);
	return 1;
}

TS_STATE(INI_START)
{
	return rDO | ACT_START;
}

/* ---- phase 0: API 単体 ---- */
TS_STATE(ACT_START)
{
TS_PRIVATE(sTimer timer;)
sPtr<tsThread> thr = application->getThread();

	::printf("=== phase 0: limitThreadsNumber() API ===\n");
	eq("default limit is MAX_INTEGER", thr->limitThreadsNumber(), MAX_INTEGER);
	thr->limitThreadsNumber(0);
	eq("limitThreadsNumber(0) clamps to 2",  thr->limitThreadsNumber(), 2);
	thr->limitThreadsNumber(-5);
	eq("limitThreadsNumber(-5) clamps to 2", thr->limitThreadsNumber(), 2);
	thr->limitThreadsNumber(7);
	eq("limitThreadsNumber(7) round-trips",  thr->limitThreadsNumber(), 7);
	thr->limitThreadsNumber(MAX_INTEGER);
	eq("restored to unbounded",              thr->limitThreadsNumber(), MAX_INTEGER);
	return rDO | ACT_P1;
}

/* ---- phase 1: 既定 (無制限) の対照実験 ---- */
TS_STATE(ACT_P1)
{
	::printf("=== phase 1: default (unbounded) — control ===\n");
	reset();
	thNEW(ts2Parallel,(ifThis, 1, worker_body));
	return ACT_P1_WAIT;
}
TS_STATE(ACT_P1_WAIT)
{
char buf[64];
	R_TEST
	eq("all workers completed", tld_done, NWORKERS);
	::snprintf(buf,sizeof(buf),"max concurrent = %d (must exceed 3)",tld_max);
	ok("pool grew past the idle floor", tld_max > 3, buf);
	return rDO | ACT_P2;
}

/* ---- phase 2: limit=3 ---- */
TS_STATE(ACT_P2)
{
	::printf("=== phase 2: limitThreadsNumber(3) ===\n");
	application->getThread()->limitThreadsNumber(3);
	timer.start(ifThis,SETTLE_US);		/* 余剰 worker の退出待ち */
	return ACT_P2_SETTLE;
}
TS_STATE(ACT_P2_SETTLE)
{
	if ( ! timer.is_expire(ifThis) )
		return 0;
	reset();
	thNEW(ts2Parallel,(ifThis, 1, worker_body));
	return ACT_P2_WAIT;
}
TS_STATE(ACT_P2_WAIT)
{
char buf[64];
	R_TEST
	eq("all workers completed", tld_done, NWORKERS);
	::snprintf(buf,sizeof(buf),"max concurrent = %d (limit 3)",tld_max);
	ok("concurrency capped at 3", tld_max <= 3, buf);
	return rDO | ACT_P3;
}

/* ---- phase 3: 実行中の上限「拡大」 ---- */
TS_STATE(ACT_P3)
{
	::printf("=== phase 3: raise the limit to 6 at runtime ===\n");
	application->getThread()->limitThreadsNumber(6);
	reset();
	thNEW(ts2Parallel,(ifThis, 1, worker_body));
	return ACT_P3_WAIT;
}
TS_STATE(ACT_P3_WAIT)
{
char buf[64];
	R_TEST
	eq("all workers completed", tld_done, NWORKERS);
	::snprintf(buf,sizeof(buf),"max concurrent = %d (limit 6)",tld_max);
	ok("concurrency capped at 6", tld_max <= 6, buf);
	::snprintf(buf,sizeof(buf),"max concurrent = %d (must exceed 3)",tld_max);
	ok("growth resumed past the old cap", tld_max > 3, buf);
	return rDO | ACT_P4;
}

/* ---- phase 4: 実行中の上限「縮小」 ---- */
TS_STATE(ACT_P4)
{
	::printf("=== phase 4: shrink the limit to 2 at runtime ===\n");
	application->getThread()->limitThreadsNumber(2);
	timer.start(ifThis,SETTLE_US);
	return ACT_P4_SETTLE;
}
TS_STATE(ACT_P4_SETTLE)
{
	if ( ! timer.is_expire(ifThis) )
		return 0;
	reset();
	thNEW(ts2Parallel,(ifThis, 1, worker_body));
	return ACT_P4_WAIT;
}
TS_STATE(ACT_P4_WAIT)
{
char buf[64];
	R_TEST
	eq("all workers completed", tld_done, NWORKERS);
	::snprintf(buf,sizeof(buf),"max concurrent = %d (limit 2)",tld_max);
	ok("concurrency capped at 2", tld_max <= 2, buf);
	return rDO | ACT_P5;
}

/* ---- phase 5: 上限を絞ったまま ready に積んで、待たずに teardown へ ----
 *
 * phase 1-4 はどれも「全 worker が完走してから」終了に入るので、ready に滞留が
 * ある状態で tsThread の FIN_* に入る経路を通っていなかった。上限を絞るほど滞留は
 * 起きやすくなるので、そこを踏ませる。
 *
 * tsThread の teardown は FIN_STABLE_WAIT (ready/run が空になるまで 1ms ポーリング)
 * → FIN_DRAINED (runThreads(0)) → FIN_WAIT (currentRunThreads==0 待ち) の順。
 * 上限クランプは num > _limitThreadsNumber のときだけ効くので runThreads(0) は
 * 素通りするはずだが、FIN_STABLE_WAIT の drain が上限で直列化する点は実測でないと
 * 分からない (遅延で済むのか、抜けられなくなるのか)。
 */
TS_STATE(ACT_P5)
{
int i;
	::printf("=== phase 5: teardown with a backlog under a tight cap ===\n");
	application->getThread()->limitThreadsNumber(2);
	reset();
	/* chain させず一斉に立てる。chain だと上限が spawn 速度を律速して積み上がらない */
	for ( i = 0 ; i < NBACKLOG ; i ++ )
		thNEW(ts2Parallel,(ifThis, 1, backlog_body));
	timer.start(ifThis,150*1000);		/* 2 本走らせて残りを ready に溜める */
	return ACT_P5_TEARDOWN;
}
TS_STATE(ACT_P5_TEARDOWN)
{
	if ( ! timer.is_expire(ifThis) )
		return 0;
	tld_teardown_started = tld_done;
	tld_teardown_t0 = tld_now_us();
	::printf("  launched %d, completed %d, running %d — the rest is queued in ready\n",
		 NBACKLOG, tld_done, tld_cur);
	::printf("  entering teardown WITHOUT waiting for them\n");
	return rDO | FIN_START;			/* ★ 待たずに落ちる */
}

TS_STATE(FIN_START)
{
	/* teardown の最中に setter を叩く経路も通しておく。同じ値の再適用なので drain の
	 * 幅は変えない (= phase 5 は上限 2 のままの drain を測る)。
	 *
	 * 【実測メモ】ここで上限を「上げる」と、tsThread 自身がまだ FIN_* に入っていなければ
	 * ACT_TINYSTATE_START の自動増加が生きているので pool は実際に伸び、drain が速くなる
	 * (2 のままなら約 1.3s、8 に上げると約 0.5s)。壊れはしないが、上限 2 の drain を
	 * 測るという phase 5 の趣旨は消えるので、ここでは上げない。
	 * なお tsThread が FIN_DRAINED に達した後 (targetRunThreads==0) に上げても、setter は
	 * _runThreads_nolock(targetRunThreads) = _runThreads_nolock(0) を再適用するだけなので
	 * worker は復活しない。この時刻にはアプリ側から合わせられないためコード読みでの確認。 */
	application->getThread()->limitThreadsNumber(2);
	return rDO | FIN_TINYSTATE_START;
}
