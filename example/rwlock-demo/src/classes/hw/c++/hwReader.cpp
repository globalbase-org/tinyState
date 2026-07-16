#include	"_ts2/c++/hwReader_.h"

CLASS_TINYSTATE(hw/c++/hwReader,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include <cstdio>
#include <unistd.h>
#include "ts2/c++/stdMutex.h"

/** @brief Worker that takes a shared READ lock on a stdMutex. / stdMutex の共有 READ ロックを取るワーカ。
 * @details
 * Acquires the read lock (yielding via sException if a writer holds it), holds it<br>
 * while "reading" in a TS_THREAD, then releases. Multiple readers hold at once.<br>
 *
 * ---
 *
 * read ロックを取得 (writer 保持中は sException で yield)、TS_THREAD で「読み取り」<br>
 * している間ロックを保持し、解放する。複数 reader が同時に保持できる。<br>
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwReader_(sPtr<tinyState> parent,
		sPtr<stdMutex> _rw, int _id, int _work_ms);
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
TS_END_INTERFACE

#endif


hwReader_::hwReader_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
}


TS_STATE(INI_START)
{
	::printf("[reader %d] start — want READ lock\n", _id);
	return rDO | ACT_ACQUIRE;
}

/// read_lock() throws sException(EX_STAY) if a writer holds it; retried on wakeup.
TS_STATE(ACT_ACQUIRE)
{
	_rw->read_lock();
	::printf("[reader %d] READING   (concurrent readers = %d)\n",
		_id, _rw->flag_count());
	return rDO | ACT_WORK;
}

/// Hold the read lock while "reading" — TS_THREAD releases mtx so readers overlap.
TS_THREAD(ACT_WORK)
{
	::usleep(_work_ms * 1000);
	return rDO | ACT_RELEASE;
}

TS_STATE(ACT_RELEASE)
{
	::printf("[reader %d] done reading — releasing\n", _id);
	_rw->release();
	return rDO | FIN_START;
}

TS_STATE(FIN_START)
{
	return rDO | FIN_TINYSTATE_START;
}
