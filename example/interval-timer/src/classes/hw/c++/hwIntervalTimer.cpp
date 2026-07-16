#include	"_ts2/c++/hwIntervalTimer_.h"

CLASS_TINYSTATE(hw/c++/hwIntervalTimer,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include	"ts2/c++/tsSignal.h"
#include	"ts2/c++/sTimer.h"
#include	"ts2/c++/stdString.h"

/** @brief Periodic timer that prints a message at a fixed interval.
 *
 * Handles SIGINT gracefully: finishes the current tick, then shuts down.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief Construct and start the timer.
	 *  @param parent    Parent state machine (owns this object).
	 *  @param _interval Interval between ticks in milliseconds.
	 *  @param _str      Message string printed on each tick.
	 */
	hwIntervalTimer_(
		sPtr<tinyState> parent,
		int _interval,
		const char * _str);
	IMP_PRIVATE virtual sPtr<stdEvent> filter(sPtr<stdEvent> ev);
private:
	unsigned	sigint_flag:1;
protected:
	TS_DEFARGS
	int			count;
	sPtr<stdString>		label;
	sPtr<tsSignal>		sig_int;
	sTimer			timer;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
class tinyState;
TS_END_INTERFACE

#endif


hwIntervalTimer_::hwIntervalTimer_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
}


sPtr<stdEvent>
hwIntervalTimer_::filter(sPtr<stdEvent> ev)
{
	if ( ev == thNULL )
		return ev;
	if ( ev->type == TSE_SIGNAL && ev->msg_int == SIGINT )
		sigint_flag = 1;
	return TS_BASECLASS::filter(ev);
}


/*******************************************
	STATE MACHINE
********************************************/


/** @brief Initialize: reset counter, register SIGINT handler, start loop. */
TS_STATE(INI_START)
{
	sigint_flag = 0;
	count = 0;
	label = thNEW(stdString,(_str));
	sig_int = thNEW(tsSignal,(ifThis,SIGINT));
	return rDO|ACT_LOOP;
}

/// Print the message on each tick. Transitions to FIN_START if SIGINT received.
TS_STATE(ACT_LOOP)
{
	count++;
	::printf("%3d %s\n",count,label->get_str());
	if ( sigint_flag )
		return rDO|FIN_START;
	timer.start(ifThis,(INTEGER64)_interval);
	return ACT_WAIT;
}

/// Wait for timer expiry, then return to ACT_LOOP.
TS_STATE(ACT_WAIT)
{
	if ( !timer.is_expire(ifThis) )
		return 0;
	return rDO|ACT_LOOP;
}

/** @brief Graceful shutdown: print quit message, stop timer, destroy signal handler. */
TS_STATE(FIN_START)
{
	::printf("quit %s\n",label->get_str());
	timer.stop(ifThis);
	sig_int->destroy();
	return rDO|FIN_TINYSTATE_START;
}
