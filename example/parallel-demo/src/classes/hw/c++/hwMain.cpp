

#include	"_ts2/c++/hwMain_.h"
#include	"ts2/c++/stdLimitSemaphore.h"
#include	"ts2/c++/sPicoState.h"

CLASS_TINYSTATE(hw/c++/hwMain,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include <cstdio>
#include "ts2/c++/ts2Parallel.h"

class stdLimitSemaphore;
/**
 * @brief ts2Parallel デモ: 10 個のワーカーを chain 生成して fork/join
 *
 * 最初のワーカーが body の中で次の兄弟を生成する chain 方式で 10 個の
 * ワーカーを起動する。全員が完了すると TSE_RETURN が届き、ACT_WAIT が返る。
 *
 * TS_PRIVATE(int counter;) により、counter は hwMain_ のメンバ変数として
 * 生成される。lambda は [this] でキャプチャするため、全ワーカーから
 * 同じ counter を参照できる。
 */
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


TS_STATE(INI_START)
{
	return rDO | ACT_START;
}

TS_STATE(ACT_START)
{
TS_PRIVATE(int counter;)
TS_PRIVATE(sPtr<stdLimitSemaphore> sem;)
	counter = 0;
	sem = thNEW(stdLimitSemaphore,(4));
	::printf("parallel-demo: launching 10 workers\n");
	struct {
		PS_PRESET
		int my_counter;
	} pico_state = {};
	thNEW(ts2Parallel,(ifThis, 0,
		[this,pico_state](sPtr<ts2Parallel> me, sPtr<stdEvent> ev) mutable -> int {
			// cancel()/destroy() 時はここで後始末をして必ず非0を返して抜ける。
			// (このデモは cancel しないが、ts2Parallel 利用の必須契約として明示)
			if ( me->is_destroyed() )
				return 1;
	 	PS_STATEMENT(pico_state,
			PS_DEF(my_counter);
		,
		PS_STATE(psINI)
			::printf("COUNTER %i\n",counter);
			my_counter = counter++;
			if ( counter < 10 )
				thNEW(ts2Parallel,(me, 0));
		PS_STATE(psDO)
			sem->get();
			::printf("  worker %d\n", my_counter);
			sem->release();
			PS_RETURN(1);
		)
		return 0;
		}));
	return ACT_WAIT;
}

TS_STATE(ACT_WAIT)
{
	R_TEST
	::printf("all %d workers done\n", counter);
	return rDO | FIN_START;
}

TS_STATE(FIN_START)
{
	sem = thNULL;
	return rDO | FIN_TINYSTATE_START;
}
