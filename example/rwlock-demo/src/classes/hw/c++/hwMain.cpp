#include	"_ts2/c++/hwMain_.h"

CLASS_TINYSTATE(hw/c++/hwMain,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include <cstdio>
#include "hw/c++/hwReader.h"
#include "hw/c++/hwWriter.h"
#include "ts2/c++/stdMutex.h"

/** @brief Top-level coordinator for rwlock-demo. / rwlock-demo のトップレベル調整役。
 * @details
 * Creates one shared stdMutex and spawns _n_readers hwReader + _n_writers hwWriter,<br>
 * all competing on it. Readers share; writers are exclusive. Waits for all to finish.<br>
 *
 * ---
 *
 * 共有 stdMutex を 1 つ作り、_n_readers 個の hwReader と _n_writers 個の hwWriter を<br>
 * 生成して競わせる。reader は共有、writer は排他。全員終了まで待つ。<br>
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwMain_(sPtr<tinyState> parent, int _n_readers, int _n_writers, int _work_ms);
private:
	sPtr<stdMutex>	rw;
	int		alive;
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
	::printf("rwlock-demo: %d readers + %d writers on one stdMutex, work=%dms each\n",
		_n_readers, _n_writers, _work_ms);
	rw    = thNEW(stdMutex, ());
	alive = _n_readers + _n_writers;
	for ( int i = 0; i < _n_readers; i++ ) {
		sPtr<hwReader> r = thNEW(hwReader, (ifThis, rw, i, _work_ms));
		r->listen(ifThis, TSE_DESTROY);
	}
	for ( int i = 0; i < _n_writers; i++ ) {
		sPtr<hwWriter> w = thNEW(hwWriter, (ifThis, rw, i, _work_ms));
		w->listen(ifThis, TSE_DESTROY);
	}
	return ACT_WAIT;
}

/// Count TSE_DESTROY from readers/writers; FIN when the last finishes.
TS_STATE(ACT_WAIT)
{
	if ( ev->type != TSE_DESTROY )
		return 0;
	alive--;
	if ( alive > 0 )
		return 0;
	return rDO | FIN_START;
}

TS_STATE(FIN_START)
{
	::printf("all readers/writers done\n");
	rw = thNULL;
	return rDO | FIN_TINYSTATE_START;
}
