#include	"_ts2/c++/hwMain_.h"

CLASS_TINYSTATE(hw/c++/hwMain,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include "hw/c++/hwIntervalTimer.h"
#include "hw/c++/hwPiCalc.h"
#include "ts2/c++/tsSignal.h"

/** @brief Top-level coordinator that owns hwIntervalTimer and hwPiCalc.
 *
 * Starts both children, waits for hwIntervalTimer to die on SIGINT,
 * then destroys hwPiCalc and waits for it to finish before exiting.
 *
 * hwPiCalc's TS_THREAD worker holds refio=1 for its entire lifetime
 * (from INI_START through all rounds to ZOM), so fwIO stays alive
 * without any extra keepalive timer.
 *
 * sig_pipe absorbs SIGPIPE that tinyState's destroy() sends via THR_KILL
 * to interrupt TS_THREAD workers.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief Construct the coordinator.
	 *  @param parent    Parent state machine (tsApplication).
	 *  @param _interval hwIntervalTimer tick interval in microseconds.
	 *  @param _str      Label printed by hwIntervalTimer each tick.
	 *  @param _trials   Monte Carlo trials per round for hwPiCalc.
	 */
	hwMain_(sPtr<tinyState> parent,
		int _interval, const char * _str, long long _trials);
private:
	sPtr<hwIntervalTimer>	timer;
	sPtr<hwPiCalc>		picalc;
	sPtr<tsSignal>		sig_pipe;
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
	trace("hwMain");
	sig_pipe = thNEW(tsSignal,(ifThis, SIGPIPE));
	timer    = thNEW(hwIntervalTimer,(ifThis, _interval, _str));
	picalc   = thNEW(hwPiCalc,       (ifThis, _trials));
	timer->listen(ifThis, TSE_DESTROY);
	return ACT_WAIT;
}

/// Wait for hwIntervalTimer to die (SIGINT), then trigger hwPiCalc shutdown.
TS_STATE(ACT_WAIT)
{
	if ( ev->type != TSE_DESTROY || ev->source != timer )
		return 0;
	picalc->destroy();
	picalc->listen(ifThis, TSE_DESTROY);
	return ACT_WAIT_PICALC;
}

/// Wait for hwPiCalc's final TS_THREAD round to finish and its FIN chain to complete.
TS_STATE(ACT_WAIT_PICALC)
{
	if ( ev->type != TSE_DESTROY || ev->source != picalc )
		return 0;
	return rDO | FIN_START;
}

/** @brief Both children are gone — print goodbye and shut down. */
TS_STATE(FIN_START)
{
	::printf("goodbye\n");
	sig_pipe->destroy();
	return rDO | FIN_TINYSTATE_START;
}
