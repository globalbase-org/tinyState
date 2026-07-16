

#ifndef ___stdLimitSemaphore_cpp_H___
#define ___stdLimitSemaphore_cpp_H___

#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/tinyState.h"

/**
 * @brief Semaphore with panic on excess release() calls — 余計な release に対してパニックを発生させるセマフォ
 * @details Create with thNEW(stdLimitSemaphore,(lim)).
 * Up to lim tinyState instances may hold the semaphore simultaneously.
 * The upper limit can be changed at runtime with limit(lim).
 * stdSemaphore had no guard against excess release() calls; this class adds that guard.
 * At the same time, the ability to change the limit mid-operation was added.
 *
 * thNEW(stdLimitSemaphore,(lim)) で生成する。
 * lim 個までの tinyState がセマフォ取得可となる。
 * 利用中も、limit(lim) で新しい上限値を設定可能。
 * stdSemaphore では、リリースしすぎた場合のガードがなかった。それをガードする仕組みを入れた。
 * 同時に、limit を途中で変更できる機能を追加した。
 */
class stdLimitSemaphore : public stdObject {
public:
	/** @brief Number of tinyState instances currently holding the semaphore — 現在取得中の tinyState の数
	 */
	int	count;
	/**
	 * @brief Constructor — コンストラクタ
	 * @param[in] lim Maximum number of concurrent holders / 取得可能最大数
	 */
  	stdLimitSemaphore(int lim=1);
	~stdLimitSemaphore();
	/**
	 * @brief Acquire semaphore — セマフォ取得
	 * @details If count < limit, increments count and returns.
	 * Otherwise throws sException(EX_STAY) and queues the caller.
	 * In TS_STATE: yields and resumes when release() wakes the caller.
	 * In TS_THREAD + THR_CATCH: blocks the worker thread until release().
	 *
	 * count < limit のとき count を増やしてそのまま返る。
	 * そうでなければ sException(EX_STAY) を投げ、呼び出し元を待ちキューに入れる。
	 * TS_STATE では yield し、release() で再起動される。
	 * TS_THREAD + THR_CATCH ではスレッドをブロックし、release() まで待つ。
	 */
	void get();
	/**
	 * @brief Release semaphore — セマフォ解放
	 * @details Decrements count. Panics if count is already 0.
	 * If a slot becomes available and a waiter exists, wakes the first queued caller.
	 *
	 * count を減じる。count が既に 0 の場合はパニック。
	 * 空きが生じ、待ちキューに tinyState がいれば先頭を起こす。
	 */
	void release();
	/**
	 * @brief Get the current upper limit — 上限値の取得
	 * @return Current upper limit / 上限値
	 */
	int limit();
	/**
	 * @brief Set a new upper limit — 上限値の設定
	 * @param[in] lim New upper limit / 新しい上限値
	 * @details If the new limit is larger, wakes queued waiters for the newly available slots.
	 *
	 * 新しい上限が大きい場合、増えた空き枠の分だけ待ちキューを起こす。
	 */
	void limit(int lim);
private:
	/** @brief Upper limit — 上限値
	 */
	int	v_limit;
	/** @brief Wait queue — 待ちキュー
	 */
	sPtr<stdQueue<tinyState> > 	wait;
};


#endif

