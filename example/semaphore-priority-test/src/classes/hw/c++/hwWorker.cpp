#include	"_ts2/c++/hwWorker_.h"
#include	"ts2/c++/stdLimitSemaphore.h"

CLASS_TINYSTATE(hw/c++/hwWorker,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include <cstdio>
class stdLimitSemaphore;
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwWorker_(sPtr<tinyState> parent, sPtr<stdLimitSemaphore> sem, int name, int pri);
	/* ★ override は implement ブロックの public: に置くこと。protected: だと tscpp2 が
	 * interface 側 (_pb.h) に載せないので、tinyState::priority() の glue が
	 * impl->tinyState_::priority() を修飾付き (= 非仮想) で呼び、override が素通りされる。
	 * エラーは出ず、既定の TS_DEFAULT_PRIORITY (10000) が返り続けるだけになる。 */
	virtual int priority(sPtr<tinyState> caller=thNULL);
protected:
	TS_DEFARGS
	int	arrived;	/* 到着を一度だけ記録するためのフラグ */
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
class stdLimitSemaphore;
TS_END_INTERFACE
#endif

hwWorker_::hwWorker_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

int
hwWorker_::priority(sPtr<tinyState> caller)
{
	return pri;
}

extern void record_arrival(int name);
extern void record_entry(int name);

TS_STATE(INI_START)
{
	arrived = 0;
	return rDO | ACT_GET;
}

/* 1 状態 = get() 1 回。取れなければ sException で yield し、release() で先頭から再走する。
 *
 * 到着 (= 待ちキューに並んだ順) をここで自分で記録する。get() の中で ins() されるので、
 * その直前が到着時刻。再走で二重に記録しないよう arrived で一度だけにする。
 * 「生成順に並ぶはず」と決め打たず観測するのが要点 — 判定はこの観測値を使う。 */
TS_STATE(ACT_GET)
{
	if ( ! arrived ) {
		arrived = 1;
		record_arrival(name);
	}
	sem->get();
	record_entry(name);
	return rDO | ACT_REL;
}
TS_STATE(ACT_REL)
{
	sem->release();
	return rDO | FIN_START;
}
TS_STATE(FIN_START) { return rDO | FIN_TINYSTATE_START; }
