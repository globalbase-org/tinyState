#include	"_ts2/c++/hwWorker_.h"

CLASS_TINYSTATE(hw/c++/hwWorker,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include <cstdio>
#include <unistd.h>
#include "ts2/c++/stdLimitSemaphore.h"

/** @brief Worker that competes for stdLimitSemaphore.
 *
 * Tries to acquire the semaphore (yielding if the limit is reached),
 * holds it for _work_ms milliseconds in a TS_THREAD, then releases it.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwWorker_(sPtr<tinyState> parent,
		sPtr<stdLimitSemaphore> _sem, int _id, int _work_ms);
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
TS_END_INTERFACE

#endif


hwWorker_::hwWorker_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
}


TS_STATE(INI_START)
{
	::printf("[worker %d] start — waiting for semaphore\n", _id);
	return rDO | ACT_ACQUIRE;
}

/// Acquire semaphore — throws sException(EX_STAY) if limit reached; retried on wakeup.
TS_STATE(ACT_ACQUIRE)
{
	_sem->get();
	::printf("[worker %d] acquired  (holders now: %d / limit: %d)\n",
		_id, _sem->count, _sem->count);
	return rDO | ACT_WORK;
}

/// Simulate work while holding the semaphore — runs in TS_THREAD so mtx is released.
TS_THREAD(ACT_WORK)
{
	::usleep(_work_ms * 1000);
	return rDO | ACT_RELEASE;
}

TS_STATE(ACT_RELEASE)
{
	::printf("[worker %d] releasing (holders before: %d)\n", _id, _sem->count);
	_sem->release();
	return rDO | FIN_START;
}

TS_STATE(FIN_START)
{
	return rDO | FIN_TINYSTATE_START;
}
