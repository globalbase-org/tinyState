

#include	"_ts2/c++/ts2Parallel_.h"

CLASS_TINYSTATE(ts2/c++/ts2Parallel,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief Fork/join parallel execution framework. / 並行処理フレームワーク。
 * @details
 * Pass a ts2ParallelFn (= std::function<int(sPtr<ts2Parallel>,sPtr<stdEvent>)>)
 * as the third constructor argument to define the worker body.
 * The first argument `me` is the worker's own interface pointer, which can be used
 * to spawn additional sibling workers.
 *
 * @code{.cpp}
 * TS_STATE(ACT_START)
 * {
 * TS_PRIVATE(int counter;)
 *     counter = 0;
 *     thNEW(ts2Parallel,(ifThis, 0,
 *         [this](sPtr<ts2Parallel> me, sPtr<stdEvent> ev) -> int {
 *             if (me->is_destroyed()) return 1;   // cancelled: release resources & exit
 *             ::printf("count = %d\n", counter++);
 *             if (counter < 10)
 *                 me->spawn();   // spawn next sibling
 *             return 1;
 *         }));
 *     return ACT_WAIT;
 * }
 * TS_STATE(ACT_WAIT)
 * {
 *     R_TEST   // wait for TSE_RETURN (all siblings done)
 * }
 * @endcode
 *
 * body return values:<br>
 * return 0  : suspend, wait for next event<br>
 *             (mutable lambda captures are preserved across sException restarts)<br>
 * return !0 : done
 *
 * <b>The body MUST handle cancellation</b>:<br>
 * On cancel() — or any destroy() of this worker — the body is re-invoked once
 * with `me->is_destroyed()` true (the framework does NOT skip the body, because
 * cleanup of application-owned resources is body-specific). The body must, at its
 * top, detect this, release whatever it owns, and <b>return !0</b> to finalize:<br>
 *     if (me->is_destroyed()) { ...cleanup...; return 1; }<br>
 * Failing to do so is a bug: a body that keeps working may spawn() orphan siblings
 * that escape the cancel, and a body that returns 0 while destroyed suspends
 * forever (destroy() delivers only a single wake-up) and never terminates.
 *
 * Spawning siblings:<br>
 * me->spawn()             : spawn sibling, inherit type and body from root<br>
 * me->spawn(type)         : spawn sibling, override type<br>
 *
 * Cancellation:<br>
 * me->cancel()            : destroy all siblings and root; send TSE_RETURN to caller<br>
 *                           safe to call from any worker body; call only once per group<br>
 *                           always return immediately after cancel() in the body<br>
 *
 * type == 0: run in TS_STATE (sException-aware)<br>
 * type == 1: run in TS_THREAD (blocking allowed)
 *
 * When all siblings complete (or cancel() is called), TSE_RETURN is sent to the caller.
 *
 * ---
 *
 * 第3引数に ts2ParallelFn (= std::function<int(sPtr<ts2Parallel>,sPtr<stdEvent>)>)
 * を渡してワーカーの body を定義する。第1引数 `me` はワーカー自身の interface pointer で、
 * 兄弟ワーカーの生成に使う。
 *
 * @code{.cpp}
 * TS_STATE(ACT_START)
 * {
 * TS_PRIVATE(int counter;)
 *     counter = 0;
 *     thNEW(ts2Parallel,(ifThis, 0,
 *         [this](sPtr<ts2Parallel> me, sPtr<stdEvent> ev) -> int {
 *             if (me->is_destroyed()) return 1;   // cancel 時: リソース回収して抜ける
 *             ::printf("count = %d\n", counter++);
 *             if (counter < 10)
 *                 me->spawn();   // 次の兄弟を生成
 *             return 1;
 *         }));
 *     return ACT_WAIT;
 * }
 * TS_STATE(ACT_WAIT)
 * {
 *     R_TEST   // TSE_RETURN (全兄弟完了) まで待つ
 * }
 * @endcode
 *
 * body の戻り値:<br>
 * return 0  : 中断・イベント待ち<br>
 *             (sException 後の再起動でも mutable キャプチャは保持)<br>
 * return !0 : 完了
 *
 * <b>body は cancel を必ず処理すること</b>:<br>
 * cancel() (または自分への destroy()) が起きると、body は `me->is_destroyed()` が真の
 * 状態で 1 回だけ再呼び出しされる (後始末はアプリ依存なのでフレームワークは body を
 * スキップしない)。body は冒頭でこれを検出し、自分が抱えるリソースを解放して
 * <b>非 0 を返して</b>終了すること:<br>
 *     if (me->is_destroyed()) { ...後始末...; return 1; }<br>
 * これを怠るとバグになる: 処理を続ける body は cancel から逃げる孤児 spawn を生み、
 * destroyed 中に 0 を返す body は永久に suspend し (destroy() の起こしは 1 回だけ)
 * 終了しない。
 *
 * 兄弟プロセスの生成:<br>
 * me->spawn()             : 兄弟を生成 (type・body を root から継承)<br>
 * me->spawn(type)         : type を指定して兄弟を生成<br>
 *
 * キャンセル:<br>
 * me->cancel()            : 全兄弟と root を destroy し、呼び出し元へ TSE_RETURN を返す<br>
 *                           どのワーカーの body からでも呼べる。cancel() の直後は必ず return すること<br>
 *                           グループあたり 1 回のみ有効 (_cancelling フラグで二重実行を防ぐ)<br>
 *
 * type == 0: TS_STATE で実行 (sException 対応)<br>
 * type == 1: TS_THREAD で実行 (blocking 可)
 *
 * 全兄弟が完了する (または cancel() が呼ばれる) と呼び出し元へ TSE_RETURN が返る。
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2Parallel_(sPtr<tinyState> parent, int type = -1, ts2ParallelFn fn = {});
	sPtr<ts2Parallel> _next;
	sPtr<ts2Parallel> _root;
	ts2ParallelFn org;
	int org_type;
	/** @brief Spawn a new sibling worker. / 兄弟ワーカーを生成する。
	 * @details
	 * Equivalent to `thNEW(ts2Parallel,(ifThis, type))`.
	 * The new worker inherits `org` from the root, so its body always
	 * starts from the same clean initial state regardless of spawn order.
	 * Always return immediately after spawn() if done with this worker.
	 *
	 * @param type 0 = TS_STATE, 1 = TS_THREAD, -1 = inherit from root.
	 *
	 * ---
	 *
	 * `thNEW(ts2Parallel,(ifThis, type))` と等価。
	 * 新しいワーカーは root から `org` を継承するため、spawn 順によらず
	 * body は常に同じ初期状態から始まる。
	 * spawn() 後、このワーカーで作業を終えるならすぐに return すること。
	 *
	 * @param type 0 = TS_STATE, 1 = TS_THREAD, -1 = root から継承。
	 */
	void spawn(int type = -1);
	/** @brief Destroy all sibling workers and root; send TSE_RETURN to caller.
	 *         / 全兄弟と root を destroy し呼び出し元へ TSE_RETURN を送る。
	 * @details
	 * Safe to call from any worker body; delegates to root if called on a sibling.
	 * Idempotent — `_cancelling` flag prevents double execution.
	 * Always return immediately after cancel().
	 *
	 * ---
	 *
	 * どのワーカーの body からでも呼べる (兄弟から呼ぶと root に委譲)。
	 * 冪等 — `_cancelling` フラグで二重実行を防ぐ。
	 * cancel() の直後は必ず return すること。
	 */
	void cancel();
protected:
	ts2ParallelFn _fn;
	unsigned _cancelling:1;
	int body(sPtr<stdEvent> ev);
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
#include	<functional>
class ts2Parallel;
using ts2ParallelFn = std::function<int(sPtr<ts2Parallel>,sPtr<stdEvent>)>;
class tinyState;
TS_END_INTERFACE

#endif


ts2Parallel_::ts2Parallel_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
	_cancelling = 0;
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/


int
ts2Parallel_::body(sPtr<stdEvent> ev)
{
	if ( _fn ) {
		sPtr<ts2Parallel> me;
		me = ifThis;
		return _fn(me, ev);
	}
	return 1;
}

void
ts2Parallel_::spawn(int type)
{
	thNEW(ts2Parallel,(ifThis, type));
}

void
ts2Parallel_::cancel()
{
	if ( C_TEST(_root->state(), C_ZOM) ) return;
	if ( _root != ifThis ) {
		_root->cancel();
		return;
	}
	// root: destroy all siblings then self
	if ( _cancelling ) return;
	_cancelling = 1;
	sPtr<ts2Parallel> p = _next;
	while ( p != thNULL ) {
		sPtr<ts2Parallel> nxt = p->_next;
		p->destroy();
		p = nxt;
	}
	destroy();
}


/*******************************************
	STATE MACHINE
********************************************/


TS_STATE(INI_START)
{
sPtr<ts2Parallel> pr;
	pr = sPtr<ts2Parallel>::d_cast(parent);
	if ( pr == thNULL || C_TEST(pr->state(), C_ZOM) ) {
		_root = ifThis;
		_next = thNULL;
		org = fn;
		org_type = type;
	}
	else {
		_root = pr->_root;
		_next = _root->_next;
		_root->_next = ifThis;
	}
	_fn = _root->org;
	if ( type < 0 )
	  	type = _root->org_type;
	if ( type == 0 )
		return rDO|ACT_START;
	return rDO|ACT_START_THR;
}
TS_STATE(ACT_START)
{
	if ( body(ev) == 0 )
		return 0;
	return rDO|FIN_START;
}
TS_THREAD(ACT_START_THR)
{
	if ( body(ev) == 0 )
		return 0;
	return rDO|FIN_START;
}
TS_STATE(FIN_START)
{
	if ( _root == ifThis ) {
		if ( _next != thNULL )
			return 0;
		parent->eventHandler(
			thNEW(stdEvent,(TSE_RETURN,ifThis,(INTEGER64)0)));
	}
	else {
	sPtr<ts2Parallel>* np;
		for ( np = &_root->_next ; *np != ifThis ; np = &(*np)->_next );
		*np = _next;
		_next = thNULL;
		if ( _root->_next == thNULL )
			_root->wakeup();
	}
	return rDO|FIN_TINYSTATE_START;
}
