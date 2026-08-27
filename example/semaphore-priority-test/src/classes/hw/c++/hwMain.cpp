#include	"_ts2/c++/hwMain_.h"
#include	"ts2/c++/stdLimitSemaphore.h"
#include	"hw/c++/hwWorker.h"

CLASS_TINYSTATE(hw/c++/hwMain,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT

#include <cstdio>
#include "ts2/c++/sTimer.h"

/**
 * @brief stdLimitSemaphore::enablePriority の入場順を検証する。
 *
 * 主 (main) が上限 1 のセマフォを先に占有し、その間に worker を並べる。全員が待ちキューに
 * 入ったところで release すると、あとは連鎖的に入場していく。その入場順を検証する。
 *
 * <b>判定は期待列の直書きではなく不変条件で行う。</b>
 *
 *   1. 入場順が priority について非減少であること
 *   2. 同じ priority の worker どうしは、<b>観測した到着順</b>と同じ順で入場すること
 *
 * 2 を「到着順 = 生成順」と決め打つと、テストが自分では測っていない仮定 (thNEW した順に
 * 状態機械がスケジュールされる) に乗ることになる。その仮定が崩れた日に落ちても、実装が
 * 壊れたのかテストの前提が崩れたのかを区別できない。だから worker には自分が待ちキューに
 * 並んだ順を記録させ、判定はその観測値と突き合わせる。
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwMain_(sPtr<tinyState> parent);
protected:
	TS_DEFARGS
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
class stdLimitSemaphore;
TS_END_INTERFACE
#endif

hwMain_::hwMain_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

#define NW	5

/* 優先度を意図的にばらけさせ、同順位 (10000 が 2 つ・100 が 2 つ) を含める。
 * 生成順と優先度順が一致しないので、先着順のままなら 1 が、
 * 同順位が逆転するなら 2 が落ちる。 */
static int pri[NW] = { 10000, 100, 10000, 100, 50 };

int arrival[NW]; int narrival = 0;	/* 待ちキューに並んだ順 (worker が自分で記録) */
int entry[NW];   int nentry   = 0;	/* セマフォを取れた順 */
int sempri_fail = 0;

void
record_arrival(int name)
{
	arrival[narrival++] = name;
}

void
record_entry(int name)
{
	entry[nentry++] = name;
	::printf("  entered: w%d (priority %d)\n", name, pri[name]);
}

static void
show(const char * label, int * a, int n)
{
int i;
	::printf("%s", label);
	for ( i = 0 ; i < n ; i++ )
		::printf("w%d(%d) ", a[i], pri[a[i]]);
	::printf("\n");
}

static int
pos(int * a, int n, int name)
{
int i;
	for ( i = 0 ; i < n ; i++ )
		if ( a[i] == name )
			return i;
	return -1;
}

TS_STATE(INI_START) { return rDO | ACT_START; }

TS_STATE(ACT_START)
{
TS_PRIVATE(sPtr<stdLimitSemaphore> sem;)
TS_PRIVATE(sTimer timer;)
int i;
	sem = thNEW(stdLimitSemaphore,(1));
	sem->enablePriority = 1;
	::printf("=== stdLimitSemaphore enablePriority ===\n");
	::printf("%d workers, priorities", NW);
	for ( i = 0 ; i < NW ; i++ )
		::printf(" %d", pri[i]);
	::printf(" (smaller enters first; ties keep arrival order)\n");
	sem->get();			/* main が先に占有 → 以後の worker は全員待ちに入る */
	for ( i = 0 ; i < NW ; i++ )
		thNEW(hwWorker,(ifThis,sem,i,pri[i]));
	timer.start(ifThis,200*1000);	/* 全員が待ちキューに並ぶまでの余裕 */
	return ACT_LET_GO;
}
TS_STATE(ACT_LET_GO)
{
	if ( ! timer.is_expire(ifThis) )
		return 0;
	if ( narrival != NW ) {
		::printf("*** FAILED: only %d of %d reached the gate in 200ms ***\n", narrival, NW);
		sempri_fail = 1;
		return rDO | FIN_START;
	}
	show("arrival order: ", arrival, narrival);
	::printf("releasing\n");
	sem->release();
	timer.start(ifThis,1000*1000);
	return ACT_DONE;
}
TS_STATE(ACT_DONE)
{
int i, j;
	if ( nentry < NW ) {
		if ( timer.is_expire(ifThis) ) {
			::printf("*** FAILED: only %d of %d entered ***\n", nentry, NW);
			sempri_fail = 1;
			return rDO | FIN_START;
		}
		return 0;
	}
	show("entry order:   ", entry, nentry);

	/* 不変条件 1: 入場順は priority について非減少 */
	for ( i = 1 ; i < NW ; i++ )
		if ( pri[entry[i]] < pri[entry[i-1]] ) {
			::printf("*** FAILED: w%d (%d) entered after w%d (%d) — "
				 "higher priority came later\n",
				 entry[i], pri[entry[i]], entry[i-1], pri[entry[i-1]]);
			sempri_fail = 1;
		}

	/* 不変条件 2: 同じ priority どうしは到着順を保つ */
	for ( i = 0 ; i < NW ; i++ )
		for ( j = 0 ; j < NW ; j++ ) {
			if ( i == j || pri[i] != pri[j] )
				continue;
			if ( pos(arrival,narrival,i) >= pos(arrival,narrival,j) )
				continue;
			if ( pos(entry,nentry,i) < pos(entry,nentry,j) )
				continue;
			::printf("*** FAILED: w%d and w%d are both priority %d; "
				 "w%d arrived first but w%d entered first\n",
				 i, j, pri[i], i, j);
			sempri_fail = 1;
		}

	if ( ! sempri_fail )
		::printf("*** OK — priority non-decreasing, ties in arrival order ***\n");
	return rDO | FIN_START;
}
TS_STATE(FIN_START) { sem = thNULL; return rDO | FIN_TINYSTATE_START; }
