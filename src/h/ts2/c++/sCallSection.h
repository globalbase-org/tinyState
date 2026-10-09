


#ifndef __sCallSection_cpp_H___
#define __sCallSection_cpp_H___

#include	"ts2/c++/sThreadKey.h"
#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/tinyState.h"

/** @brief `eventHandler()` の入れ子を許す既定の上限 (段)。/ Default cap on eventHandler() nesting. */
#define TS_EVH_DEPTH_MAX	1500
/** @brief 1 段あたりに見込むスタック消費 (バイト)。実測 656B (Linux x86-64・ほぼ空の
 *  状態関数) の約 6 倍で、状態関数が自分のローカルを持つぶんの余裕。
 *  / Stack budgeted per nesting level; ~6x the measured 656B of an almost-empty state function. */
#define TS_EVH_FRAME_BUDGET	4096

/**
 * @brief sCallSection 内部で使う tinyState スタックのノード (スタックに積む値型)。/ Stack node used internally by sCallSection (value type placed on the C++ stack).
 */
class sCallSectionNode : public sObject {
public:
	sCallSectionNode(sPtr<tinyState> ts) {
		this->ts = ts;
		next = 0;
	}
	sCallSectionNode * 		next;
	sPtr<tinyState>			ts;
};

/**
 * @brief 現在実行中の tinyState を示すスレッドローカルスタック。/ Thread-local stack tracking the currently executing tinyState.
 * @details
 * `ts2IO::read()` / `write()` などが sException を投げる際に、どの tinyState が
 * 呼び出し元かを知るために使う。`sCallSection::key->caller()` で取得できる。
 * `sCALL_SECTION(code)` マクロで push/pop を安全に管理する。
 * / Used by `ts2IO::read()`/`write()` (etc.) when throwing sException, so the yield target
 * tinyState can be retrieved via `sCallSection::key->caller()`.
 * Use the `sCALL_SECTION(code)` macro to safely manage push/pop.
 */
class sCallSection : public sObject {
public:
	sCallSection();
	~sCallSection();
	void push(sCallSectionNode * n);
	sPtr<tinyState> pop(sCallSectionNode * n);
	sPtr<tinyState> caller();

	/**
	 * @brief この スレッドで `eventHandler()` の入れ子を何段まで許すかを返す。
	 *        / How deep eventHandler() may nest on *this* thread.
	 * @details
	 * `min( TS_EVH_DEPTH_MAX, このスレッドの実スタック長 / TS_EVH_FRAME_BUDGET )`。
	 * スレッドごとに初回だけ OS に聞いて覚える (`ts2_thread_stack_size()`)。
	 * スタックの小さい機では**勝手に下がるだけ**で、沈黙死にはならない。
	 * 聞けなかった (0 が返った) ときは `TS_EVH_DEPTH_MAX` に落とす。<br>
	 * ★ アプリが `tsApplication::evh_depth` を明示していればそちらが勝つ。
	 * ここが見るのは「聞いて決める」既定値だけ。
	 */
	int depthLimit();

	/** @brief depthLimit() の高速路。覚えてあれば読むだけ (ディスパッチ毎に呼ばれる)。
	 *  / Fast path for depthLimit(): a load once this thread has asked the OS. */
	int depthLimitFast() {
		return depth_limit ? depth_limit : depthLimit();
	}

	/**
	 * @brief 現在の `eventHandler()` 入れ子段数。`sCallSectionDepth` が増減する。
	 *        / Current eventHandler() nesting depth; maintained by sCallSectionDepth.
	 * @details
	 * ★ **`push()` / `pop()` に連動させてはいけない。** `pop()` は `invoke_state()`
	 * の **前**にあるので、listener 連鎖では C スタックが伸び続けるのに段数が戻り、
	 * カウンタが浅いまま張り付く。増減は関数の入口と**本当の出口**で行う。
	 */
	int				depth;


	static sThreadKey<sCallSection> key;
protected:
	sCallSectionNode *		list;
	/** @brief depthLimit() の覚え書き。0 = まだ OS に聞いていない。/ Memoised depthLimit(); 0 = not asked yet. */
	int				depth_limit;
};

/**
 * @brief `eventHandler()` の入れ子段数を入口で +1 し、**本当の出口**で -1 する RAII ガード。
 *        / RAII guard: +1 on entry to eventHandler(), -1 at its real exit.
 * @details
 * 早期 return (C_ZOM / `state_lock` 再入 / gc へ逃がした場合) でも正しく戻る。
 * スレッドローカルの参照は構築時の 1 回だけで、`section()` で使い回せるので
 * `push()` / `pop()` のために引き直す必要はない。
 *
 * ★ 静的に置いてはならない (デストラクタを持つため)。スタック上の自動変数専用。
 */
class sCallSectionDepth {
public:
	sCallSectionDepth() {
		cs = sCallSection::key.operator -> ();
		cs->depth ++;
	}
	~sCallSectionDepth() {
		cs->depth --;
	}
	/** @brief 自分の段数 (1 = 最外周)。/ This call's depth; 1 is the outermost. */
	int depth() const {
		return cs->depth;
	}
	/** @brief この スレッドの既定の上限。/ This thread's default cap. */
	int limit() const {
		return cs->depthLimitFast();
	}
	/** @brief 構築時に引いた sCallSection。TLS を引き直さないために使う。/ The sCallSection looked up once at construction. */
	sCallSection * section() const {
		return cs;
	}
private:
	sCallSection *		cs;
};



/** @brief 現在実行中 tinyState (`ifThis`) を sCallSection スタックに積んで `code` を実行する。
 *  sException がスローされても安全に pop される。
 *  / Push `ifThis` onto the sCallSection stack, execute `code`, then pop (exception-safe).
 */
#define sCALL_SECTION(__x)	\
	{						\
		sCallSectionNode n(ifThis);		\
		sCallSection::key->push(&n)		\
		try {					\
			for ( ; ; ) {			\
				__x			\
				break;			\
			}				\
		} catch ( sException& ex ) {		\
			sCallSection::key->pop(&n);	\
			throw ex;			\
		}					\
		sCallSection::key->pop(&n);		\
	}

#endif
