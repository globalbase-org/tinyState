

#ifndef ___sThreadMutexHandleRelease_cpp_H___
#define ___sThreadMutexHandleRelease_cpp_H___

#include	"ts2/c++/sThreadMutex.h"


/**
 * @brief `sThreadMutex` の RAII 一時 unlock ハンドル。スコープを抜けると再 lock する。/ RAII temporary-unlock handle; re-locks when scope exits.
 * @details
 * コンストラクタで `unlock()` し、デストラクタで `lock()` する。
 * TS_THREAD 内で app-mutex を一時的に解放するために使う。コピーは禁止。
 * / Calls `unlock()` in constructor, `lock()` in destructor.
 * Used to temporarily release the app-mutex inside `TS_THREAD`.
 */
class sThreadMutexHandleRelease : public sObject {
public:
	sThreadMutexHandleRelease(sThreadMutex & _mtx) 
		:
		mtx(_mtx)
	{
		if ( mtx.unlock() < 0 )
			sObject::panic("UNLOCK mutex handleRelease error");
	}
	sThreadMutexHandleRelease(const sThreadMutexHandleRelease & hdr) 
		:
		mtx(hdr.mtx)
	{
		sObject::panic("mutex handleRelease's copy is not permitted");
	}
	~sThreadMutexHandleRelease() {
		mtx.lock();
	}
	void operator =(sThreadMutex & _mtx) {
		sObject::panic("mutex handleRelease's copy is not permitted");
	}
	void operator =(const sThreadMutexHandleRelease & hdr) {
		sObject::panic("mutex handleRelease's copy is not permitted");
	}
protected:
	sThreadMutex & 		mtx;
};

#endif
