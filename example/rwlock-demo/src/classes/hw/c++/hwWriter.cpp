#include	"_ts2/c++/hwWriter_.h"

CLASS_TINYSTATE(hw/c++/hwWriter,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include <cstdio>
#include <unistd.h>
#include "ts2/c++/stdMutex.h"

/** @brief Worker that takes an EXCLUSIVE write lock on a stdMutex. / stdMutex の排他 WRITE ロックを取るワーカ。
 * @details
 * Acquires the write lock (yielding via sException while readers or another writer<br>
 * hold it), holds it exclusively while "writing" in a TS_THREAD, then releases.<br>
 *
 * ---
 *
 * write ロックを取得 (reader や別 writer 保持中は sException で yield)、TS_THREAD で<br>
 * 「書き込み」している間 排他保持し、解放する。<br>
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwWriter_(sPtr<tinyState> parent,
		sPtr<stdMutex> _rw, int _id, int _work_ms);
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
TS_END_INTERFACE

#endif


hwWriter_::hwWriter_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
}


TS_STATE(INI_START)
{
	::printf("[writer %d] start — want WRITE lock\n", _id);
	return rDO | ACT_ACQUIRE;
}

/// write_lock() throws sException(EX_STAY) if held; retried on wakeup.
TS_STATE(ACT_ACQUIRE)
{
	_rw->write_lock();
	::printf("[writer %d] WRITING   (EXCLUSIVE, count = %d)\n",
		_id, _rw->flag_count());
	return rDO | ACT_WORK;
}

/// Hold the write lock exclusively while "writing".
TS_THREAD(ACT_WORK)
{
	::usleep(_work_ms * 1000);
	return rDO | ACT_RELEASE;
}

TS_STATE(ACT_RELEASE)
{
	::printf("[writer %d] done writing — releasing\n", _id);
	_rw->release();
	return rDO | FIN_START;
}

TS_STATE(FIN_START)
{
	return rDO | FIN_TINYSTATE_START;
}
