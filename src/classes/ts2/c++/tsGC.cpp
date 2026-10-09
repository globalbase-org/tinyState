

#include	"_ts2/c++/tsGC_.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/sCallSection.h"	/* 配送できる深さかの検め */
#include	<cstdio>
#include	"ts2/c++/tsProbe.h"	/* 配送したことの記録 */

CLASS_TINYSTATE(ts2/c++/tsGC,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


#include	"ts2/c++/stdHalfOrderQueueTS.h"

class TS_THISCLASS : public TS_BASECLASS {
public:
	tsGC_(
		sPtr<tinyState>  parent);
	void inherit(
		sPtr<tinyState>  parent);

	void exe(sPtr<tinyState>  caller);

	/** @brief 指定イベントを**そのまま**添えて後で配送する。深さ超過で逃がしてきた
	 *  `eventHandler()` の入場を引き受けるための口。/ Queue an object together with
	 *  the event it was entered with; used when eventHandler() nesting ran too deep.
	 *  @details
	 *  従来の `exe(obj)` は `TSE_RETURN` で呼び直すので、**元のイベントが要る**入場は
	 *  引き受けられない。こちらは `ev` を保持し、順番が来たら `obj->eventHandler(ev)`
	 *  を呼ぶ。`ev` が thNULL なら `exe(obj)` と同じ扱い。<br>
	 *  ★ 順序は保証されない (tsApplication::evh_depth 参照)。 */
	void exe(sPtr<tinyState>  caller,sPtr<stdEvent>  ev,INTEGER64 probe_seq=0);

	/** @brief ACT_EXE が 1 回の入場で使っていい時間 (マイクロ秒) を返す。
	 *  / Returns the time ACT_EXE may spend in one dispatch, in microseconds. */
	INTEGER64 interval();
	/** @brief その時間を設定する。既定は GC_BUDGET_US (100ms)。
	 *         0 以下を渡すと「1 件配送するたびに譲る」になる。
	 *  / Sets it; the default is GC_BUDGET_US (100ms).  A value of 0 or less
	 *    means "yield after every single delivery". */
	void interval(INTEGER64 intvl);

private:
protected:
	sPtr<stdHalfOrderQueueTS> 	que;
	INTEGER64			budget_us;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
class tinyState;
TS_END_INTERFACE

#endif


/* ACT_EXE が 1 回の入場で使っていい時間の既定値。interval() で変えられる。
 * 100ms は tinyState_::enter_time のドキュメントと COOKBOOK §6.1 が例に使って
 * いる値に合わせてある。 */
static const INTEGER64	GC_BUDGET_US = 100*1000;

tsGC_::tsGC_(
		sPtr<tinyState>  _parent)
        : tinyState_(_parent)
{
}

void
tsGC_::inherit(
	sPtr<tinyState>  _parent)
{
	this->TS_BASECLASS::inherit(_parent);
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/

/* exe(obj,ev) で積む 1 件。
 *
 * stdHalfOrderQueue は sPtr<stdObject> を持てるので、従来の exe(obj) が積む
 * tinyState と同じキューに同居できる (キューを 2 本に分けると予算と順番を
 * 2 重に面倒見ることになる)。取り出し側は d_cast で見分ける。
 *
 * ★ そのため ACT_EXE は stdHalfOrderQueueTS::del() を**使えない**。あちらは
 * tinyState へ d_cast して失敗すれば thNULL を返すので、この要素を引くと
 * 「キューが空」に見えてしまう — しかも要素は既に取り除かれているので、
 * 黙って捨てることになる。ACT_EXE は基底の del() を呼ぶ。 */
class tsGCJob : public stdObject {
public:
	tsGCJob(sPtr<tinyState>  ts,sPtr<stdEvent>  ev,INTEGER64 seq) {
		this->ts = ts;
		this->ev = ev;
		this->seq = seq;
	}
	~tsGCJob() {
		this->ts = thNULL;
		this->ev = thNULL;
	}
	sPtr<tinyState> 	ts;
	sPtr<stdEvent> 		ev;
	/** 逃がし側のプローブが振った通し番号。配送側がこれを出すので、
	 *  「逃がした ev が全部届いたか」を外から突き合わせられる。 */
	INTEGER64		seq;
};

void
tsGC_::exe(sPtr<tinyState>  caller)
{
	que->ins(caller->priority(ifThis),caller);
	wakeup();
}

void
tsGC_::exe(sPtr<tinyState>  caller,sPtr<stdEvent>  ev,INTEGER64 probe_seq)
{
	if ( ev == thNULL ) {
		this->exe(caller);
		return;
	}
	que->ins(caller->priority(ifThis),thNEW( tsGCJob,(caller,ev,probe_seq)));
	wakeup();
}

INTEGER64
tsGC_::interval()
{
	return budget_us;
}

/* 値はそのまま持つ。丸めない。
 *   0 以下 … 1 件配送するたびに譲る (いちばん公平・いちばん往復が多い)
 *   大きい … 譲らないに近づく。2026-10-02 以前の挙動が欲しければそれ。 */
void
tsGC_::interval(INTEGER64 intvl)
{
	budget_us = intvl;
}

/* filter() の override は撤去した (2026-10-02)。
 *
 * やっていたのは TSE_PRIORITY で priority_flag を立てることだけで、その
 * priority_flag 自体を下の理由で捨てたため、中身が無くなった。
 *
 * ついでに効くこと: 基底 tinyState_::filter(thNULL) は **例外を投げる**ので、
 * inherit() の試行で filter_lock が 0 になる。override が在る間は thNULL を
 * そのまま返していたので filter_lock = 1 で、eventHandler は **イベントごとに**
 * lm を解放して application->mtx を取り直していた (filter 呼び出しのため)。
 * override を消すとその往復がまるごと無くなる。gc は撤去中にいちばん回る
 * 状態機械なので、app-mutex のトラフィックとしてここは小さくない。
 */


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	/* 初期化は INI_START で行う (COOKBOOK §10)。
	 * ★ inherit() ではできない。生成コンストラクタは parent 1 引数のクラスでは
	 * `this->tinyState::inherit(parent)` と書き、その先も
	 * `impl->tinyState_::inherit(parent)` と修飾されていて tinyState_::inherit は
	 * virtual でもないので、**tsGC_::inherit は構築時に一度も呼ばれない**。
	 * ここに置く前は budget_us が operator new の memset のまま 0 で、既定が
	 * 「1 件ごとに譲る」になっていた。 */
	budget_us = GC_BUDGET_US;
	que = (thNEW( stdHalfOrderQueueTS,()));
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
	if ( que->count() == 0 )
		return rDO|ACT_TINYSTATE_CHECK1;
	stdInterval::wait(ifThis,0,TSE_RETURN);
	return ACT_RET;
}
TS_STATE(ACT_RET)
{
	R_TEST
	return rDO|ACT_EXE;
}

/* キューから 1 つ取り出して配送する。予算を使い切るまで rDO で回り続ける。
 *
 * 2026-10-02: 優先度とタイマを撤去し、譲る判断を enter_time に一本化した。
 * 撤去したものと、その理由:
 *
 *   priority_flag / stdHalfOrderQueueTS の作り直し
 *       TSE_PRIORITY を送る側がツリーに **1 つも無い**ので、flag は決して
 *       立たなかった。
 *
 *   last_pri != pri での並べ直し
 *       tinyState_::priority() は定数 TS_DEFAULT_PRIORITY を返すだけで、
 *       override もツリーに無い。つまり pri は常に同じで、この分岐も決して
 *       成立しなかった。★ところがこれが **既定構成での唯一の yield 点**
 *       だったので、結果として gc はキューを一息に流し切っていた。撤去中に
 *       座り込みが gc->exe へ移って見えたのはこれが理由。
 *
 *   sTimer / tsGC::timer_mode
 *       timer_mode は誰も設定せず 0 のまま = is_expire は決して見られない。
 *       それでも ACT_RET が毎ラウンド timer.start(ifThis,0) を呼ぶため、
 *       **0 遅延の TSE_TIMER を毎周 fwIO に登録**していた。誰も使わない
 *       登録と、それが呼ぶ余分な gc ディスパッチが増えるだけだった。
 *       同じ目的 (占有時間で切り上げる) は enter_time なら登録ゼロでできる。
 *
 * 予算は interval() で変えられる (既定 GC_BUDGET_US = 100ms)。
 * 予算を超えたら ACT_START へ行く。あちらは stdInterval::wait(ifThis,0,
 * TSE_RETURN) でスレッドを返すので、他の状態機械に順番が回り、TSE_RETURN で
 * 入場し直したときには enter_time も打ち直されている。
 *
 * 注意: 配送先 1 つが予算より長く掛かる場合、ここでは切れない。その手当ては
 * 配送先が自分の enter_time で行う (COOKBOOK §6.1)。ここで見ているのは
 * 「gc が何個も続けて配送してスレッドを抱え込む」側だけ。
 */
TS_STATE(ACT_EXE)
{
sPtr<stdObject>  it;
sPtr<tinyState>  ok;
	/* ★★ 配送できない設定なら、黙って空転せずに **突き返す**。
	 *
	 * ここは逃がされた仕事を返す場所で、返した相手は **自分より 1 段深い**所で走る。
	 * 上限がその深さに届いていなければ、返した途端にまた逃がされる —
	 * キューを往復するだけで 1 段も進まない (実測: 15 秒で ACT_EXE 350 万回・
	 * 出力ゼロ・落ちもしない)。従来の `exe(obj)` の譲りの作法 (COOKBOOK §6.1) も
	 * 同じ経路なので、**gc 経由の譲りが全部効かなくなる**。
	 *
	 * ★ `evh_depth` は「このスレッドのスタックに何段積めるか」から来た値なので、
	 * 足りないときに**こちらで下駄を履かせてはいけない** (履かせた値はもう
	 * スタックの実力を表さない)。足りないのはアプリの指定か、その機のスタックの
	 * 方である。⇒ 設定を再考させる。
	 *
	 * ★ 判定を ACT_START ではなく **ここ** に置く理由: ACT_START は深い所からの
	 * `wakeup()` でも走り得るので (そのときは timer を張り直すだけで配送はしない)、
	 * あちらの深さで判定すると**誤って panic する**。配送が起きるのはこの状態だけで、
	 * ここの深さが配送先の深さを決める。
	 *
	 * 毎回の配送で 1 回 int を比べるだけなので、実質「定期的に検める」になっている。
	 *
	 * ★★ **この検めは「設定の検査」であると同時に、tinyState 自身の構造変化の
	 * 検出器でもある。** ここで消費されている段数は今 **3** だが
	 * (tsApplication の eventHandler → fwIO::loop → gc の eventHandler)、
	 * 将来この経路に 1 段入れば 4 になる。そのとき、それまで通っていた設定値が
	 * 黙って不可能になる — その齟齬を見つけるのがこの panic である。
	 * だからメッセージは閾値を書かず、**実測した深さを名指しする**。
	 *
	 * ★ 実際に当たるのは利用側の負荷試験 (ctest) だろう。利用者が panic するほど
	 * 小さい値を設定することはまず無い。⇒ **検出器として置いてある**と読むのが正しい。
	 *
	 * ★★ 相対深さ (`depth - base` で比べる形) は **採らない**。アプリから見た
	 * 目盛りは素直になるが、**同じ `evh_depth` が経路ごとに違う絶対深さを許す**
	 * ことになり (fwIO 由来なら base=1 ・ gc 由来なら base=3 …)、スタックの余裕が
	 * 経路ごとに揺れる。再現しない余裕は追えない。加えて、この値の由来は
	 * 「スタックの底から何段入るか」という**絶対量**なので、相対で比べると
	 * 由来と食い違う。⇒ 値は加工せず絶対で比べ、齟齬は panic で突き返す。 */
	{
	sCallSection *	csec = sCallSection::key.operator -> ();
	int		mydepth = csec->depth;
	int		limit = 0;
		if ( application.is_notNull() )
			limit = application->evh_depth;		/* 明示指定が勝つ */
		if ( limit <= 0 )
			limit = csec->depthLimitFast();		/* 無指定 = 機ごとに自動 */
		if ( limit <= mydepth ) {
		char msg[256];
			::snprintf(msg,sizeof(msg),
				"tsGC: evh_depth %d cannot deliver from depth %d"
				" (the delivery would be deferred again, forever)."
				" Raise application->evh_depth above %d, or leave it 0"
				" for the per-thread default.",
				limit,mydepth,mydepth);
			stdObject::panic(msg);
		}
	}
	/* ★ 基底の del() を呼ぶ。stdHalfOrderQueueTS::del(thNULL) は中身が
	 * d_cast(stdHalfOrderQueue::del(&key)) で、caller が thNULL なら
	 * それをそのまま返すだけなので、tinyState 要素については挙動が同じ。
	 * 違いは tsGCJob を引いたときに thNULL へ化けないこと。 */
	it = que->stdHalfOrderQueue::del();
	if ( it == thNULL )
		return rDO|ACT_START;
	{
		/* ★ 免除 (sCallSectionExempt) は撤去した。配送先が上限に弾かれない
		 * ことは、上の panic が **設定の側で** 保証している。免除で辻褄を合わせると
		 * 「上限を超えて 1 段だけ通る」例外が常に残り、上限の意味が曖昧になる。 */
		ok = sPtr<tinyState>::d_cast(it);
		if ( ok.is_notNull() )
			ok->eventHandler(
				thNEW( stdEvent,(TSE_RETURN,ifThis,(INTEGER64)0)));
		else {
		sPtr<tsGCJob>  job = sPtr<tsGCJob>::d_cast(it);
			/* 深さ超過で逃がされてきた入場。元のイベントで呼び直す。 */
			if ( job.is_notNull() ) {
				/* ★ 配送したことを記録する。逃がし側の seq と 1 対 1 に
				 * 対応するので、利用側は「順序が変わって自分のコードが
				 * 壊れた」のか「ev が届いていない」のかを**見分けられる**。
				 * 記録は eventHandler を**呼ぶ前**に出す — 呼んだ先で落ちたり
				 * 座り込んだりしたときに「配送まで到達した」ことが残るように。 */
				tsProbeEvhDeliver(job->ts,
					job->ev.is_notNull() ? job->ev->type : 0,job->seq);
				job->ts->eventHandler(job->ev);
			}
		}
	}
	/* 差で比べる。利用側のコードは enter_time + 予算 < now() と足し算で書いて
	 * よいが (COOKBOOK §6.1)、ここの予算は interval() で外から入る値なので、
	 * 極端に大きい値を渡されたときに enter_time + budget_us が桁あふれして
	 * 符号が反転し「毎回譲る」に化けるのを避ける。 */
	if ( stdInterval::now() - enter_time > budget_us )
		return rDO|ACT_START;
	return rDO;
}
TS_STATE(FIN_START)
{
	que = thNULL;
	return rDO|FIN_TINYSTATE_START;
}

