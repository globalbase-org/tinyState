

#ifndef ___stdMutex_cpp_H___
#define ___stdMutex_cpp_H___

#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/tinyState.h"

/**
 * @brief Reader-writer lock for use within tinyState state machines. / tinyState 向けの読み書きロック (state-machine aware mutex)。
 * @details
 * Unlike `sThreadMutex`, never blocks a thread. When the lock is unavailable the<br>
 * caller (`sCallSection::key->caller()`) is queued and an `sException` is thrown to<br>
 * yield; `release()` wakes the waiters so they retry.<br>
 * <br>
 * - `read_lock()`: shared read — multiple readers (`count > 0`)<br>
 * - `write_lock()`: exclusive write (`count == -1`)<br>
 * - `release()`: release either lock; wakes waiters once the lock becomes free<br>
 * <br>
 * Call only from within a tinyState execution (the caller must be on the<br>
 * sCallSection stack, as it is during TS_STATE / TS_THREAD). For raw thread<br>
 * synchronisation use `sThreadMutex` instead.<br>
 *
 * ---
 *
 * `sThreadMutex` と違いスレッドをブロックしない。ロックが取れない場合、呼び出し元<br>
 * (`sCallSection::key->caller()`) を待ちキューに積んで `sException` を throw し yield<br>
 * する。`release()` が待ち手を `wakeup()` して再試行させる。<br>
 * <br>
 * - `read_lock()`: 共有読み取り — 複数リーダ可 (`count > 0`)<br>
 * - `write_lock()`: 排他書き込み (`count == -1`)<br>
 * - `release()`: いずれのロックも解放。空きになったら待ち手を起こす<br>
 * <br>
 * 呼び出しは tinyState 実行中 (caller が sCallSection に積まれている TS_STATE /<br>
 * TS_THREAD の文脈) からのみ。生スレッド同期には `sThreadMutex` を使うこと。<br>
 */
class stdMutex : public stdObject {
public:
  	stdMutex();
	~stdMutex();
	/** @brief Acquire read lock; queue caller and throw sException (yield) if a writer holds it. / 読み取りロックを取得。writer 保持中なら caller を待ちキューに積み sException を投げ yield。 */
	void read_lock();
	/** @brief Acquire exclusive write lock; queue caller and throw sException (yield) if held. / 排他書き込みロックを取得。保持中なら caller を待ちキューに積み sException を投げ yield。 */
	void write_lock();
	/** @brief Release the lock; wakeup waiting tinyStates once it becomes free. / ロックを解放し、空きになったら待ち tinyState を wakeup する。 */
	void release();
	int wait_count() {
		if ( wait == thNULL )
			return 0;
		return wait->count;
	}
	int flag_count() {
		return count;
	}

private:
	int			count;
	sPtr<stdQueue<tinyState> > 	wait;
};


#endif

