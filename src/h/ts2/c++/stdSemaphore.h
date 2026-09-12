

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
	 * overriding priority() anywhere behaves exactly as it does off.  See #insNeq
	 * to change how ties are ordered.
	 *
	 * 既定は 0 = 到着順 (先着順)。1 にすると get() が tinyState::priority() を
	 * キーにして待ちキューへ入れる。
	 *
	 * <b>値が小さいほど先に入場する</b> — キューは昇順に並べて先頭から取り出すため。
	 * tinyState::priority() の既定は TS_DEFAULT_PRIORITY (10000) なので、追い越したい
	 * なら 10000 より小さい値を、後回しでよいなら大きい値を返すように override する。
	 *
	 * 同じ優先度の待ち手どうしは到着順を保つので、priority() をどこも override せずに
	 * このフラグだけ立てても挙動は既定と同じになる。同点の並べ方は #insNeq で変えられる。
	 */
	unsigned	enablePriority:1;
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
	 *
	 * @note Until 2026-09-10 this class had no default and behaved as insNeq(0) —
	 * ties were served newest-first.  stdLimitSemaphore has always defaulted to 1.
	 * The two are now aligned on 1.
	 *
	 * 2026-09-10 まで、このクラスは既定を設定しておらず insNeq(0) 相当 (同点は新しい順)
	 * だった。stdLimitSemaphore は当初から 1 である。両者を 1 に揃えた。
	 */
	void insNeq(int v);
	/**
	 * @brief Get how equal-priority waiters are ordered — 同じ優先度の待ち手の並べ方を取得する
	 * @return 1 = arrival order / 0 = reverse arrival order / 到着順 = 1・逆順 = 0
	 */
	int insNeq();
private:
	sPtr<stdQueue<tinyState> > 	wait;
};


#endif

