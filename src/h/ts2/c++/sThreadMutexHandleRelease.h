

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
		/* ★★ ここが「一時解放が失敗しても黙って通る」の入口だった。
		 * pthread_mutex_unlock の失敗は *正の errno* (EPERM 等) なので
		 * `< 0` の panic は **機械語でも符号ビットだけを見ていて絶対に
		 * 発火しない** (arm64 で tbnz w0,#0x1f を確認)。 */
		if ( mtx.unlock() != 0 )
			sObject::panic("UNLOCK mutex handleRelease error (not the owner?)");
	}
	sThreadMutexHandleRelease(const sThreadMutexHandleRelease & hdr) 
		:
		mtx(hdr.mtx)
	{
		sObject::panic("mutex handleRelease's copy is not permitted");
	}
	~sThreadMutexHandleRelease() {
		/* ★ 取り直せなかったら「持っているつもり」で先へ進む。 */
		if ( mtx.lock() != 0 )
			sObject::panic("LOCK mutex handleRelease error");
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
