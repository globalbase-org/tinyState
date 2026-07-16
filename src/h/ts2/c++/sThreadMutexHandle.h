

#ifndef ___sThreadMutexHandle_cpp_H___
#define ___sThreadMutexHandle_cpp_H___

#include	"ts2/c++/sThreadMutex.h"


/**
 * @brief `sThreadMutex` の RAII ロックハンドル。スコープを抜けると自動 unlock する。/ RAII lock handle for `sThreadMutex`; auto-unlocks on scope exit.
 * @details
 * コンストラクタで `lock()` し、デストラクタで `unlock()` する。
 * コピーは禁止。`{ sThreadMutexHandle __hdr(mtx); ... }` のように使う。
 * / Calls `lock()` in constructor, `unlock()` in destructor. Copy is prohibited.
 */
class sThreadMutexHandle : public sObject {
public:
	sThreadMutexHandle(sThreadMutex & _mtx) 
		:
		mtx(_mtx)
	{
		if ( mtx.lock() < 0 )
			sObject::panic("LOCK mutex handleRelease error");
	}
	sThreadMutexHandle(const sThreadMutexHandle & hdr) 
		:
		mtx(hdr.mtx)
	{
		sObject::panic("mutex handle's copy is not permitted");
	}
	~sThreadMutexHandle() {
		mtx.unlock();
	}
	void operator =(sThreadMutex & _mtx) {
		mtx.unlock();
		mtx = _mtx;
		mtx.lock();
	}
	void operator =(const sThreadMutexHandle & hdr) {
		sObject::panic("mutex handle's copy is not permitted");
	}
protected:
	sThreadMutex & 		mtx;
};

#endif
