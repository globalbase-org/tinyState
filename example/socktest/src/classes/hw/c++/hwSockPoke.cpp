/*
 * hwSockPoke — timer-driven helper for the socktest reactor/teardown checks.
 *
 * action 0 (connect): tick a few times on a stdInterval timer (printing each
 *   tick) THEN open a ts2IOsockTCPconnect to 127.0.0.1:port.  Run while the main
 *   test is parked in accept(): the ticks appearing BEFORE the accept completes
 *   prove the reactor kept running (i.e. AcceptEx yielded, it did not block).
 *   A blocking accept would freeze the reactor and this timer would never fire
 *   -> the whole app would hang.
 * action 1 (destroy): after one tick, destroy the target (the server) while its
 *   accept is pending -> exercises the CancelIoEx teardown path.
 */
#include	"_ts2/c++/hwSockPoke_.h"

CLASS_TINYSTATE(hw/c++/hwSockPoke,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockTCPconnect.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdInterval.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwSockPoke_(sPtr<tinyState> parent, int action, int port, sPtr<tinyState> target);
protected:
	TS_DEFARGS
	sPtr<ts2IO>		conn;
	struct sockaddr_in	resolve;
	int			ticks_left;
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
TS_END_INTERFACE
#endif

hwSockPoke_::hwSockPoke_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

TS_STATE(INI_START)
{
	ticks_left = 3;
	stdInterval::wait(ifThis,100*1000,TSE_TIMER);	/* 100ms */
	return ACT_TICK;
}
TS_STATE(ACT_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	if ( action == 1 ) {			/* teardown: destroy the target */
		if ( target != thNULL )
			target->destroy();
		return rDO|FIN_START;
	}
	/* action 0: connect (after a few visible ticks) */
	::printf("[poke] tick (reactor alive while accept pending)\n");
	ticks_left --;
	if ( ticks_left > 0 ) {
		stdInterval::wait(ifThis,100*1000,TSE_TIMER);
		return ACT_TICK;
	}
	conn = thNEW(ts2IOsockTCPconnect,(ifThis,&resolve,"127.0.0.1",port));
	stdInterval::wait(ifThis,400*1000,TSE_TIMER);	/* hold conn for accept+exchange */
	return ACT_DONE;
}
TS_STATE(ACT_DONE)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|FIN_START;
}
TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
