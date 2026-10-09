

#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadStack.h"	/* 深さの既定値を OS に聞く */

#include	<cstdio>


sThreadKey<sCallSection>
sCallSection::key;

/* thread_local の実体はこの翻訳単位だけに置く (宣言は sThreadKey.h)。
 * ヘッダに inline のままだと exe と DLL がそれぞれ自分のスロットを持ち、
 * PE には ELF の STB_GNU_UNIQUE に相当する統一機構が無いので複数実体になる。
 * docs/GOTCHAS.md §13。 */
template<> sCallSection *
sThreadKey<sCallSection>::operator -> () const
{
	struct holder {
		sCallSection * p;
		holder() : p(0) {}
		~holder() { if ( p ) delete p; }
	};
	thread_local holder h;
	if ( !h.p )
		h.p = new(__FILE__,__LINE__) sCallSection();
	return h.p;
}

sCallSection::sCallSection()
{
	list = 0;
	depth = 0;
	depth_limit = 0;
}

sCallSection::~sCallSection()
{
	list = 0;
}

/* min( TS_EVH_DEPTH_MAX, このスレッドの実スタック / TS_EVH_FRAME_BUDGET )。
 *
 * スレッドごとに初回だけ OS に聞いて覚える。sCallSection 自身がスレッドごとの
 * オブジェクトなので、覚える場所はここでよく、同期は要らない。
 *
 * 規則は 4 機で同じ (場合分けした規則は作らない)。違うのは「聞き方」だけで、
 * それは arch overlay の層が持つ → ts2_thread_stack_size()。
 * スタックが足りない機では閾値が勝手に下がるだけで、沈黙死にはならない:
 *
 *   8MB  (companion 適用後の 4 機)  -> 1500   (TS_EVH_DEPTH_MAX で止まる)
 *   519KB (mac の既定 worker)       ->  129
 *   2031KB (Windows の main)        ->  507
 *   1024KB (ulimit -s 1024)         ->  256
 */
int
sCallSection::depthLimit()
{
INTEGER64	sz,  n;
	if ( depth_limit )
		return depth_limit;

	sz = ts2_thread_stack_size();
	if ( sz <= 0 )
		n = TS_EVH_DEPTH_MAX;		/* 聞けなかった = 既定へ落とす */
	else	n = sz / TS_EVH_FRAME_BUDGET;
	if ( n > TS_EVH_DEPTH_MAX )
		n = TS_EVH_DEPTH_MAX;
	if ( n < 1 )
		n = 1;				/* 1 でも前進する: gc 経由は深さ 1 から始まる */
	depth_limit = (int)n;

	/* 既定より浅くなったことは一度だけ言う。意図した挙動だが、利用側が
	 * 「なぜこの機だけ gc 経由になるのか」を調べられるように素性を残す。
	 *
	 * static は POD の int で、定数初期化なのでガード変数もデストラクタも持たない
	 * (tinyState は破棄される static を 1 つも作らない — sImmortal.h / ctest の
	 * tinyState_no_static_dtor)。複数スレッドが同時に通れば 2 回出得るが、
	 * 出力が 1 行増えるだけなので同期は入れない。
	 *
	 * stdout ではなく stderr へ出す (ctest の PASS/FAIL_REGULAR_EXPRESSION に
	 * 当たらないように — tsProbe.h と同じ理由)。 */
	if ( depth_limit < TS_EVH_DEPTH_MAX ) {
	static int	reported = 0;
		if ( reported == 0 ) {
			reported = 1;
			::fprintf(stderr,
				"tinyState: eventHandler nesting capped at %d levels"
				" (this thread's stack is %lld KB; deeper chains are"
				" handed to the GC)\n",
				depth_limit,(long long)(sz / 1024));
		}
	}
	return depth_limit;
}

void
sCallSection::push(sCallSectionNode * n)
{
	n->next = list;
	list = n;
}

sPtr<tinyState>
sCallSection::pop(sCallSectionNode * n)
{
sPtr<tinyState> ret;
	if ( list != n )
		sObject::panic("ENTER_CALL is required");
	ret = list->ts;
	list = list->next;
	return ret;
}


sPtr<tinyState>
sCallSection::caller()
{
	if ( list )
		return list->ts;
	return thNULL;
}
