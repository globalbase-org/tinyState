

#ifndef __sThreadKey_cpp_H___
#define __sThreadKey_cpp_H___


#include	"ts2/c++/sObject.h"
#include	<pthread.h>

/**
 * @brief pthread スレッドローカルストレージのラッパー。/ pthread thread-local storage wrapper.
 * @details
 * `operator->()` でスレッドごとに独立した `__TYPE` インスタンスへアクセスする。
 * スレッドが終了すると自動的に `delete` される。
 * `sCallSection::key` (現在実行中 tinyState の追跡) 等の実装に使われる。
 * / Access per-thread `__TYPE` instances via `operator->()`. Automatically deleted on thread exit.
 * Used to implement `sCallSection::key` (tracking the current tinyState).
 * @tparam __TYPE スレッドローカルに保持する型。/ Per-thread type.
 */
template<class __TYPE>
class sThreadKey : public sObject {
public:
	sThreadKey() {}
	~sThreadKey() {}

	/* Per-thread instance via C++11 thread_local (native TLS) instead of
	   pthread_key_create/getspecific/setspecific.  On Windows, winpthreads'
	   pthread_key emulation returned a *different* (fresh, empty) value for the
	   SAME thread mid-execution under heavy worker-pool churn
	   (pthread_getspecific -> 0), which corrupted sCallSection's yield/resume
	   stack -> "ENTER_CALL is required" panic + hangs on real hardware (never on
	   Linux/glibc).  Native thread_local storage is stable for the thread's
	   lifetime; the holder's destructor frees the object at thread exit.  Works
	   identically on POSIX and MinGW.  Windows-port design memo */
	__TYPE * operator -> () const {
		struct holder {
			__TYPE * p;
			holder() : p(0) {}
			~holder() { if ( p ) delete p; }
		};
		thread_local holder h;
		if ( !h.p )
			h.p = new(__FILE__,__LINE__) __TYPE();
		return h.p;
	}
};

#endif
