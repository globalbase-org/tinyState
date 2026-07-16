

#ifndef ___sThreadMutexRecursive_cpp_H___
#define ___sThreadMutexRecursive_cpp_H___

#include	"ts2/c++/sThreadMutex.h"

/**
 * @brief 再帰的取得可能な `sThreadMutex`。同一スレッドからの多重 lock を許可する。/ Recursive variant of `sThreadMutex` allowing re-entrant locking by the same thread.
 * @details
 * tinyState の app-mutex (`tsApplication::mtx`) として使われる。
 * 同一スレッドが既にロックを持っている場合は `count` をインクリメントするだけでブロックしない。
 * / Used as the tinyState app-mutex (`tsApplication::mtx`).
 * If the same thread already holds the lock, increments `count` instead of blocking.
 */
class sThreadMutexRecursive : public sThreadMutex {
public:
	sThreadMutexRecursive();
	~sThreadMutexRecursive();
	virtual int lock();
	virtual int unlock();
	virtual int trylock();
	virtual int is_locked();
	virtual int counter();
protected:
	virtual int waitWrap(std::function<int()> func);
	int		count;
	pthread_t	id;
};

#endif
