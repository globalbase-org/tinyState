#include	"_ts2/c++/hwMain_.h"

CLASS_TINYSTATE(hw/c++/hwMain,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include <cstdio>
#include "hw/c++/hwWorker.h"
#include "ts2/c++/stdLimitSemaphore.h"

/** @brief Top-level coordinator for the semaphore-demo.
 *
 * Creates a stdLimitSemaphore(limit) and spawns _n_workers hwWorker instances
 * that all compete to hold the semaphore simultaneously.
 * Waits until every worker has finished, then exits.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwMain_(sPtr<tinyState> parent, int _n_workers, int _limit, int _work_ms);
private:
	sPtr<stdLimitSemaphore>	sem;
	int			alive;
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
	::printf("semaphore-demo: %d workers, limit=%d, work=%dms each\n",
		_n_workers, _limit, _work_ms);
	sem   = thNEW(stdLimitSemaphore, (_limit));
	alive = _n_workers;
	for ( int i = 0; i < _n_workers; i++ ) {
		sPtr<hwWorker> w = thNEW(hwWorker, (ifThis, sem, i, _work_ms));
		w->listen(ifThis, TSE_DESTROY);
	}
	return ACT_WAIT;
}

/// Count TSE_DESTROY events from workers; proceed to FIN when the last one finishes.
TS_STATE(ACT_WAIT)
{
	if ( ev->type != TSE_DESTROY )
		return 0;
	alive--;
	::printf("[main] a worker finished (%d remaining)\n", alive);
	if ( alive > 0 )
		return 0;
	return rDO | FIN_START;
}

TS_STATE(FIN_START)
{
	::printf("all workers done\n");
	return rDO | FIN_TINYSTATE_START;
}
