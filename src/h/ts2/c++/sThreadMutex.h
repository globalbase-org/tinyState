

#ifndef ___sThreadMutex_cpp_H___
#define ___sThreadMutex_cpp_H___

#include	"ts2/c++/sObject.h"
#include	<pthread.h>
#include	<functional>

class sThreadCond;
/**
 * @brief pthread_mutex_t のラッパー。TS_THREAD 内やフレームワーク内部で使うスレッドレベル mutex。/ pthread_mutex_t wrapper for use in TS_THREAD or framework internals.
 * @details
 * `stdMutex` と異なり、ロックが取れない場合にスレッドをブロックする。
 * TS_STATE 内では `stdMutex` を使うこと。TS_THREAD 内や、フレームワーク内部のスレッド同期に使う。
 * RAII で使うには `sThreadMutexHandle` / `sThreadMutexHandleRelease` を使う。
 * / Blocks the calling thread when lock is unavailable — use `stdMutex` inside `TS_STATE` instead.
 * Use `sThreadMutexHandle` for RAII locking.
 */
class sThreadMutex : public sObject {
public:
  	sThreadMutex() {
	pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutex_init(&m,&attr);
	}
	sThreadMutex(int f) {
	}

	virtual ~sThreadMutex() {
		pthread_mutex_destroy(&m);
	}
	virtual int lock() {
		return pthread_mutex_lock(&m);
	}
	virtual int unlock() {
		return pthread_mutex_unlock(&m);
	}
	virtual int trylock() {
		return pthread_mutex_trylock(&m);
	}
	virtual int is_locked() {
	int ret;
		ret = trylock();
		if ( ret < 0 ) {
			if ( errno == EBUSY )
				return 1;
			return -1;
		}
		unlock();
		return 0;
	}
	void debug(pthread_t id) {
		debugThread = id;
		debugFlag = 1;
	}
	void debug() {
		debugFlag = 0;
	}
	virtual int counter() {
		return 0;
	}
protected:
	friend sThreadCond;

	virtual int waitWrap(std::function<int()> func) {
		return func();

	}

	pthread_mutex_t m;
	volatile pthread_t	debugThread;
	volatile int		debugFlag;
};


#endif
