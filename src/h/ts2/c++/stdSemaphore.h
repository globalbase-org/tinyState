

#ifndef ___stdSemaphore_cpp_H___
#define ___stdSemaphore_cpp_H___

#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/tinyState.h"

/**
 * @brief Counting semaphore — カウンティングセマフォ
 * @details Manages a count of available resources.
 * get() decrements count; if count is already 0, the caller is queued and suspended until release() is called.
 * Unlike stdLimitSemaphore, there is no guard against excess release() calls.
 *
 * 利用可能リソース数を管理する。
 * count が 0 になると get() で呼び出し元を待ちキューに入れ、release() が呼ばれるまで停止する。
 * stdLimitSemaphore と異なり、release() の過剰呼び出しに対するガードはない。
 */
class stdSemaphore : public stdObject {
public:
	/** @brief Current semaphore count — 現在のカウント値
	 */
	int	count;
	/**
	 * @brief Constructor — コンストラクタ
	 * @param[in] _count Initial count value / 初期カウント値
	 */
  	stdSemaphore(int _count);
	~stdSemaphore();
	/**
	 * @brief Acquire semaphore — セマフォ取得
	 * @details If count > 0, decrements count and returns.
	 * If count == 0, throws sException(EX_STAY) and queues the caller.
	 * In TS_STATE: yields and resumes when release() wakes the caller.
	 * In TS_THREAD + THR_CATCH: blocks the worker thread until release().
	 *
	 * count > 0 のとき count を減じてそのまま返る。
	 * count == 0 のとき sException(EX_STAY) を投げ、呼び出し元を待ちキューに入れる。
	 * TS_STATE では yield し、release() で再起動される。
	 * TS_THREAD + THR_CATCH ではスレッドをブロックし、release() まで待つ。
	 */
	void get();
	/**
	 * @brief Release semaphore — セマフォ解放
	 * @details Increments count and wakes the first queued caller, if any.
	 *
	 * count を増やし、待ちキューの先頭を起こす。
	 */
	void release();
	/**
	 * @brief Number of waiters — 待ち中の tinyState 数
	 * @return Number of tinyState instances currently waiting / 現在待ち中の数
	 */
	int waitCount();
	/** @brief Enable priority-ordered wait queue — 優先度順待ちキューを有効にする
	 */
	unsigned	enablePriority:1;
private:
	sPtr<stdQueue<tinyState> > 	wait;
};


#endif

