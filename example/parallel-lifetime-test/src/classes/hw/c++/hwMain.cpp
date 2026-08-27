#include	"_ts2/c++/hwMain_.h"
#include	<unistd.h>

CLASS_TINYSTATE(hw/c++/hwMain,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT

#include <cstdio>
#include "ts2/c++/ts2Parallel.h"

/**
 * @brief ts2Parallel の worker ラムダが「root が FIN_START で待っている間の spawn」まで
 *        生きているかを確かめる回帰テスト。
 *
 * ts2Parallel は同じラムダを 3 つ持つ (worker ごとの _fn、root の org、TS_ARGS が保存する
 * fn)。std::function はキャプチャをヒープに持つので、使い終わったら捨てたい。ただし
 * 3 つとも同じ場所で捨てられるわけではない:
 *
 *   fn   → INI_START で org にコピーした直後。以後は誰も読まない
 *   _fn  → FIN_START 冒頭。読むのは自分の body() だけで、もう呼ばれない
 *   org  → FIN_START の中でも「_next == thNULL を確認した後」。★ ここが罠
 *
 * org を読むのは body ではなく<b>新しい兄弟の INI_START</b> (_fn = _root->org)。root は
 * 自分の body が終わった時点で FIN_START に入り _next 待ちに座るが、その待ちの間も生きて
 * いる兄弟は spawn() できる。FIN_START の冒頭で org を捨てると、以後に生まれた兄弟の _fn が
 * 空になり body() が if ( _fn ) の偽側で return 1 する = <b>クラッシュもエラーも無く、その
 * ワーカーの仕事だけが静かに消える</b>。しかも group は TSE_RETURN を返して正常完了に見える。
 *
 * このテストは root が「1 つ spawn したら即終了」する形にして、確実にその窓を踏む。
 * example/parallel-demo では踏めない (root 自身がセマフォ待ちを含む body を最後まで走らせる
 * ので、root が FIN_START へ入る頃には chain の spawn が終わっている)。
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwMain_(sPtr<tinyState> parent);
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
TS_END_INTERFACE
#endif

hwMain_::hwMain_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
}

#define NWANT	5

volatile int plt_ran  = 0;	/* body が実際に呼ばれた回数 */
int          plt_want = NWANT;

static int
body(sPtr<ts2Parallel> me, sPtr<stdEvent> ev)
{
int idx;
	if ( me->is_destroyed() )
		return 1;
	idx = __sync_fetch_and_add(&plt_ran,1);
	::printf("  body invoked: idx=%d\n", idx);
	if ( idx == 0 ) {
		me->spawn();		/* root: 1 つだけ産んで即座に終わる */
		return 1;
	}
	/* root が確実に FIN_START へ入って _next 待ちに座るまで待つ。TS_THREAD (type=1)
	 * なので、ここでブロックしても状態機械は止まらない。 */
	::usleep(100*1000);
	if ( idx + 1 < NWANT )
		me->spawn();		/* ★ root が FIN_START に座っている間の spawn */
	return 1;
}

TS_STATE(INI_START)
{
	return rDO | ACT_START;
}

TS_STATE(ACT_START)
{
	::printf("=== ts2Parallel worker-lambda lifetime ===\n");
	::printf("root spawns one sibling and finishes at once; each sibling then spawns\n");
	::printf("the next while the root is parked in FIN_START.  Expect %d bodies.\n", NWANT);
	thNEW(ts2Parallel,(ifThis, 1, body));
	return ACT_WAIT;
}

TS_STATE(ACT_WAIT)
{
	R_TEST
	::printf("=== %d/%d bodies ran ===\n", plt_ran, NWANT);
	if ( plt_ran == NWANT )
		::printf("*** OK — org survived the late spawns ***\n");
	else
		::printf("*** FAILED — a late sibling got an empty _fn and did nothing.\n"
			 "    org was cleared before the root left FIN_START. ***\n");
	return rDO | FIN_START;
}

TS_STATE(FIN_START)
{
	return rDO | FIN_TINYSTATE_START;
}
