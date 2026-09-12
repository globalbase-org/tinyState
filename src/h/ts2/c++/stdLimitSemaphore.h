

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
	 * @brief Order the wait queue by priority() instead of arrival — 待ちキューを到着順ではなく priority() 順にする
	 * @details
	 * Off by default: waiters are served strictly in arrival order.
	 * Set it to 1 and get() files each waiter under tinyState::priority() instead.
	 *
	 * <b>A smaller value is served earlier</b> — the queue is sorted ascending and
	 * dequeued from the head.  The default tinyState::priority() is
	 * TS_DEFAULT_PRIORITY (10000), so override it to return less than that to jump
	 * ahead, more to fall behind.
	 *
	 * Waiters of equal priority keep arrival order, so turning this on without
	 * overriding priority() anywhere behaves exactly as it does off.
	 *
	 * 既定は 0 = 到着順 (先着順)。1 にすると get() が tinyState::priority() を
	 * キーにして待ちキューへ入れる。
	 *
	 * <b>値が小さいほど先に入場する</b> — キューは昇順に並べて先頭から取り出すため。
	 * tinyState::priority() の既定は TS_DEFAULT_PRIORITY (10000) なので、追い越したい
	 * なら 10000 より小さい値を、後回しでよいなら大きい値を返すように override する。
	 *
	 * 同じ優先度の待ち手どうしは到着順を保つので、priority() をどこも override せずに
	 * このフラグだけ立てても挙動は既定と同じになる。
	 */
	unsigned	enablePriority:1;
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
	/**
	 * @brief Set how equal-priority waiters are ordered — 同じ優先度の待ち手の並べ方を設定する
	 * @param[in] v 1 = arrival order (default) / 0 = reverse arrival order
	 *              1 = 到着順 (既定) / 0 = 到着順の逆
	 * @details Only observable while #enablePriority is set: with it clear, get()
	 * appends to the tail and this flag is not consulted.
	 *
	 * <b>The default is 1</b>, which is what makes the #enablePriority contract hold —
	 * turning that flag on without overriding tinyState::priority() anywhere leaves
	 * behaviour unchanged, because every waiter ties at TS_DEFAULT_PRIORITY and ties
	 * keep arrival order.  Set this to 0 and those same ties invert, so a stream of
	 * equal-priority waiters serves the newest first and the earliest arrival starves.
	 * Only choose 0 when the ties are meaningful to you and you want the newest first.
	 *
	 * #enablePriority が立っているときだけ効く。倒れているときの get() は末尾追加なので
	 * このフラグを見ない。
	 *
	 * <b>既定は 1</b>。#enablePriority の契約 —「priority() をどこも override せずに
	 * フラグだけ立てても挙動は既定と同じ」— はこれが支えている。全員が
	 * TS_DEFAULT_PRIORITY で同点になり、同点が到着順を保つからである。0 にすると
	 * その同点が反転するので、同じ優先度の待ち手が並び続ける状況では新しいものから
	 * 順に入場し、<b>最初に来た待ち手が飢える</b>。同点に意味があって新しい順に
	 * 入れたいときだけ 0 を選ぶこと。
	 */
	void insNeq(int v);
	/**
	 * @brief Get how equal-priority waiters are ordered — 同じ優先度の待ち手の並べ方を取得する
	 * @return 1 = arrival order / 0 = reverse arrival order / 到着順 = 1・逆順 = 0
	 */
	int insNeq();
private:
	/** @brief Upper limit — 上限値
	 */
	int	v_limit;
	/** @brief Wait queue — 待ちキュー
	 */
	sPtr<stdQueue<tinyState> > 	wait;
};


#endif

