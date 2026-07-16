


#ifndef ___sThreadCond_cpp_H___
#define ___sThreadCond_cpp_H___


#include	<pthread.h>
#include	"ts2/c++/sThreadMutex.h"

/**
 * @brief pthread_cond_t のラッパー。`sThreadMutex` と組み合わせて使う条件変数。/ pthread_cond_t wrapper; used with `sThreadMutex` for condition signaling.
 * @details
 * `signal()` / `broadcast()` で待ちスレッドを起こす。`wait()` / `timedwait()` でスレッドを待機させる。
 * TS_THREAD 内でスレッド間通知に使う。TS_STATE 内では使わないこと。
 * / Use `signal()`/`broadcast()` to wake waiting threads; `wait()`/`timedwait()` to sleep.
 * For use inside `TS_THREAD` only — not `TS_STATE`.
 */
class sThreadCond : public sObject {
public:
	sThreadCond();
	~sThreadCond();

	int signal();
	int broadcast();
	int wait(sThreadMutex & m);
	int timedwait(sThreadMutex & m,INTEGER64 tim);
protected:
	pthread_cond_t		cond;
};

#endif
