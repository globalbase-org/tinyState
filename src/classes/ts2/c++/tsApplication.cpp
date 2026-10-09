
#include	"ts2/c++/sThreadMutexRecursive.h"
#include	"_ts2/c++/tsApplication_.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/tsThread.h"
#include	"ts2/c++/sThreadMutexHandleRelease.h"

#include	<cstdlib>

/* tsThread.cpp 定義: 生存 worker pthread 数。shutdown で全 worker の tail relref 完了(=0)を
 * 待ってから静的破棄へ進むために参照する(detached worker vs 静的 refMtx 破棄の競合防止)。 */
extern volatile int tsThreadLiveWorkers;

CLASS_TINYSTATE(tsApplication,tinyState);

#if 0

TS_BEGIN_IMPLEMENT

#include	<ts2/c++/stdQueue.h>

class stdFrameWork;
class tsApplicationGlobal : public stdObject {
public:
	sPtr<stdString>  		name;
	sPtr<stdObject> 		ptr;

	tsApplicationGlobal(sPtr<stdString>  name,sPtr<stdObject>  ptr)
	{
		this->name = name;
		this->ptr = ptr;
	}
	virtual ~tsApplicationGlobal() {
		this->name = thNULL;
		this->ptr = thNULL;
	}
};

/**
 * @brief tinyState アプリケーションのルートコンテキスト。/ Root context for a tinyState application.
 * @details
 * アプリケーション全体で共有される `fw()` (fwIO イベントループ)・`gc` (GC スレッド)・
 * `mtx` (アプリケーション共有 mutex) を管理する。
 * 通常は `thNEW(tsApplication, (application, lambda))` でエントリポイントとして起動する。
 * `application` メンバを通じてどの tinyState からも参照可能。
 *
 * グローバル変数は `set_global`/`get_global`/`del_global` で名前付き `stdObject` として管理できる。
 * / Manages the shared `fw()` (fwIO event loop), `gc` (GC thread), and `mtx` (application mutex)
 * accessible from every tinyState via the `application` member.
 * Global objects are stored by name via `set_global`/`get_global`/`del_global`.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief アプリケーションを初期化する。/ Initialize the application.
	 * @param parent          親 tinyState (通常 `application` 自身)。/ Parent tinyState (usually `application` itself).
	 * @param initial_lambda  起動時に最初に実行されるラムダ。/ Lambda executed first on startup.
	 */
	tsApplication_(
		sPtr<tinyState>  parent,
		TS_APPLICATION_FUNC initial_lambda);

	void inherit(sPtr<tinyState>  parent,TS_APPLICATION_FUNC initial_lambda);
	/** @brief fwIO インスタンスを返す。イベントループへの I/O 登録に使う。/ Returns the fwIO instance for registering I/O with the event loop. */
	sPtr<fwIO> fw();
	/** @brief ワーカスレッドキューを返す。/ Returns the worker thread queue. */
	sPtr<tsThread>  getThread();
	/** @brief フレームワークイベント (TSE_UPDATED) を全リスナーに送る。/ Broadcasts a framework event (TSE_UPDATED) to all listeners. */
  	void frameWorkEvent(int msg);
	/** @brief 名前付きグローバルオブジェクトを登録する。/ Register a named global object. */
	void set_global(const char * name,sPtr<stdObject> );
	/** @brief 名前付きグローバルオブジェクトを削除する。/ Remove a named global object. */
	void del_global(const char * name);
	/** @brief 名前付きグローバルオブジェクトを取得する。/ Retrieve a named global object. */
	sPtr<stdObject>  get_global(const char * name);

	/** @brief GC スレッド。長処理の yield や遅延 destroy を管理する。/ GC thread managing yielded long-ops and deferred destroys. */
  	sPtr<tsGC> 		gc;

	virtual int eventHandler(sPtr<stdEvent>  ev,sPtr<stdThreadInfo> __thrInfo=thNULL);

	/** @brief 再起動フラグ。アプリケーション全体の再起動を要求する。/ Restart flag to request a full application restart. */
	uint8_t		restart_flag;

	/**
	 * @brief `eventHandler()` の入れ子をこの段数まで許す。**0 = 機ごとに自動** (既定)。
	 *        / Cap on eventHandler() nesting; 0 (default) means "decide per machine".
	 * @details
	 * `eventHandler()` は入れ子に呼ばれる (親への `TSE_RETURN`・listener 通知など)。
	 * `state_lock` は**同一オブジェクトの再入**しか止めないので A→B→C の連鎖は
	 * 無制限に深くなれ、放っておくとスタックを食い尽くして**何のメッセージも出さずに**
	 * SIGSEGV で死ぬ (実測 1 段 656 バイト・Linux の 8MB で約 12,800 段)。<br>
	 * <br>
	 * 上限を超えた入場は、イベントごと `gc` のキューへ回される (`gc->exe(obj,ev)`)。
	 * gc 経由の再投入は深さ 1 から始まるので、長い連鎖は「上限 × gc 往復」に分割されて
	 * 前進する。<br>
	 * <br>
	 * **0 のまま (既定) なら、スレッドごとに実スタック長を OS に聞いて**
	 * `min( 1500, 実スタック / 4KB )` を使う。スタックの小さい機では勝手に下がるだけで、
	 * 沈黙死にはならない。明示した値はそちらが勝つ (全スレッド共通)。<br>
	 * <br>
	 * ★ **`INI` 段は逃がさない**: `TS_STATE(INI_START)` が `thNEW` の中で走り切ること
	 * が仕様なので (COOKBOOK §10)、構築の連鎖はこの安全網の対象外。<br>
	 * ★ **順序は保証されない**: 遅延された `ev` が gc キューに並ぶ一方、後から来た
	 * 別イベントは直接そのオブジェクトのキューに入るので、先に処理され得る。
	 * `onlyOneEvent` の併合も、逃がしが `_insEvent()` の**前**で分岐するぶん効かない
	 * (配送時に改めて併合されるので、1 回で済んだ状態関数が 2 回走り得る)。<br>
	 * ★ 自分のアプリが上限に届いているかは `TS2_EVH_PROBE` を設定すれば測れる
	 * (逃がし 1 件ごとに stderr へ 1 行) → `tsProbeEvhEscapeCount()`。<br>
	 * ★ 撤収中など `gc` が既に無いときは、逃がし先が無いので**従来どおり再帰する**
	 * (落とすことはしない)。<br>
	 * <br>
	 * 読み書きともできる。小さい値 (例 `4`) を入れるとこの機構の陽性対照になる。<br>
	 * ★ **小さすぎる値は `tsGC` が `panic` で突き返す** (「返した仕事がまた逃がされる
	 * だけで 1 段も進まない」設定を、黙って空転させずに知らせる)。閾値は定数ではなく
	 * **その場で測った gc の配送深さ**で、実測ではいま深さ 2 = `3` 以上なら通る。<br>
	 * ★ 足りないときに**繰り上げはしない** — この値はスタックの実力から来ているので、
	 * 下駄を履かせたらそれを表さなくなる。再考するのはアプリ側。<br>
	 * ★ 環境変数 **`TS2_EVH_DEPTH`** でも入れられる (アプリを 1 行も直さずに陽性対照を
	 * 取るための口。アプリが自分で代入すればそちらが勝つ)。
	 */
	int		evh_depth;

	/** @brief アプリケーション共通の再帰的 mutex。TS_STATE の app-mutex として使用される。/ Recursive mutex shared across all tinyStates in this application (the "app-mutex"). */
  	sThreadMutexRecursive mtx;
private:
	sPtr<fwIO>	fwClass;
	sPtr<tsThread>  	threadQueue;
	sPtr<stdQueue<tsApplicationGlobal> > 	global;
	TS_APPLICATION_FUNC		initial_lambda;

	/** @brief イベントハンドラが再入しているかのフラグ。内部用。/ Internal re-entry guard for eventHandler. */
	unsigned	eventHandler_flag:1;
};
TS_END_IMPLEMENT

#endif


/*******************************************
	PUBLIC FUNCTIONS
********************************************/




tsApplication_::tsApplication_ (
	sPtr<tinyState>  parent,
	TS_APPLICATION_FUNC initial_lambda)
:
	tinyState_(parent)
{
	this->initial_lambda = initial_lambda;
	/* 0 = 「スレッドごとに OS に聞いて決める」。既定値を operator new の memset に
	 * 頼らず明示する (COOKBOOK §10 と同じ理由)。この member は 0 が有効値なので
	 * 事故にはならないが、意図を読めるようにしておく。 */
	this->evh_depth = 0;
	/* env からも入れられるようにしてある。狙いは **アプリを 1 行も直さずに
	 * 陽性対照を取れること**: 極小の値を入れると逃がしが常時起きるので、
	 * 「順序が入れ替わってもこのアプリは壊れないか」を既存の試験のまま検定できる。
	 * 凍結中のツリーや他プロジェクトのアプリに当てるのはこの口しか無い。
	 * アプリが自分で代入すれば、そちらが後から上書きする (明示が最も強い)。 */
	{
	const char * v = ::getenv("TS2_EVH_DEPTH");
		if ( v && *v )
			this->evh_depth = (int)::strtol(v,0,0);
	}
}


void
tsApplication_::inherit(
		sPtr<tinyState>  parent,
		TS_APPLICATION_FUNC func)
{
	this->TS_BASECLASS::inherit(parent);
}

sPtr<fwIO>
tsApplication_::fw()
{
	return this->fwClass;
}


void
tsApplication_::frameWorkEvent(int msg)
{
	invoke_listen(thNEW( stdEvent,(TSE_UPDATED,ifThis,(INTEGER64)msg)));
}



static int cmp_app_global(sPtr<tsApplicationGlobal>  gr,sPtr<stdObject>  d2)
{
sPtr<stdString>  name = sPtr<stdString>::d_cast(d2);
	if ( gr->name->cmp(name) == 0 )
		return 1;
	return 0;
}

void
tsApplication_::set_global(const char * name,sPtr<stdObject>  data)
{
sPtr<tsApplicationGlobal>  gr;
sPtr<stdString>  nameObj = thNEW( stdString,(name));
	gr = this->global->check(
			nameObj,
			cmp_app_global);
	if ( gr.is_notNull() ) {
		gr->ptr = data;
		invoke_listen(thNEW( stdEvent,(TSE_UPDATED,ifThis,(INTEGER64)0)));
		return;
	}
	gr = thNEW( tsApplicationGlobal,(nameObj,data));
	this->global->ins(0,gr);
	invoke_listen(thNEW( stdEvent,(TSE_UPDATED,ifThis,(INTEGER64)0)));
}


sPtr<tsThread> 
tsApplication_::getThread()
{
	return threadQueue;
}


void
tsApplication_::del_global(const char * name)
{
sPtr<stdString>  nameObj = thNEW( stdString,(name));
	this->global->del(nameObj,cmp_app_global);
}

sPtr<stdObject> 
tsApplication_::get_global(const char * name)
{
sPtr<tsApplicationGlobal>  gr;
sPtr<stdString>  nameObj = thNEW( stdString,(name));
	gr = this->global->check(nameObj,cmp_app_global);
	if ( gr.is_notNull() )
		return gr->ptr;
	return thNULL;

}

int
tsApplication_::eventHandler(sPtr<stdEvent>  ev,sPtr<stdThreadInfo> __thrInfo)
{
	if ( eventHandler_flag )
		return 0;
	return TS_BASECLASS::eventHandler(ev,__thrInfo);
}


/*******************************************
	STATE MACHINE
********************************************/

#ifndef _WIN32
#include	<signal.h>
/* Process-wide no-op SIGPIPE handler, installed once at app startup via
   sigaction() and never removed.  tinyState::destroy() sends THR_KILL(SIGPIPE)
   (pthread_kill) to interrupt a blocked worker's syscall, and broken-pipe writes
   raise SIGPIPE too; without a handler the process dies.  Three requirements —
   all satisfied deterministically by sigaction(SIGPIPE, {noop, mask=empty,
   flags=0}):
     - a *no-op handler*, NOT SIG_IGN: SIG_IGN would not deliver, so THR_KILL
       could no longer EINTR-interrupt the worker's blocking syscall;
     - NO SA_RESTART (sa_flags=0): the interrupted syscall must return EINTR —
       with SA_RESTART it would auto-resume and THR_KILL would be a no-op;
     - NO SA_RESETHAND: the disposition stays installed across every delivery, so
       it survives the async-teardown burst of THR_KILL / broken-pipe SIGPIPEs.
   The older signal()-with-self-reinstall had ambiguous BSD/SysV semantics: under
   SysV reset-on-delivery there was a window where a second SIGPIPE hit SIG_DFL
   and killed the process at teardown (a rare rc=141), and re-arming from inside
   the handler is not async-signal-safe. */
static void ts_app_sigpipe_noop(int) { }
#endif

TS_STATE(INI_START)
{
  	stdObject::start();
#ifndef _WIN32
	{
	struct sigaction sa;
		sa.sa_handler = ts_app_sigpipe_noop;
		sigemptyset(&sa.sa_mask);	/* no :: — sigemptyset is a function-like macro on macOS/BSD */
		sa.sa_flags = 0;	/* no SA_RESTART → EINTR for THR_KILL; no SA_RESETHAND → persistent */
		::sigaction(SIGPIPE, &sa, NULL);
	}
#endif
	this->global = thNEW( stdQueue<tsApplicationGlobal>,());
	eventHandler_flag = 1;
	fwClass = thNEW(fwIO,(ifThis));
	gc = thNEW( tsGC,(ifThis));
	threadQueue = thNEW( tsThread,(ifThis));
	initial_lambda(ifThis);
	return rDO|INI_APPLICATION_START;
}
TS_STATE(INI_APPLICATION_START)
{
	return rDO|INI_TINYSTATE_FINISH;
}
TS_STATE(ACT_TINYSTATE_START)
{
sThreadMutexHandleRelease __hdr(mtx);
	if ( this->fwClass.is_notNull() && this->fwClass->loop(ifThis) > 0 )
		return rDO|ACT_START;
	return rDO|FIN_START;
}
TS_STATE(FIN_START)
{
	return rDO|FIN_TS_APPLICATION_START;
}
TS_STATE(FIN_TS_APPLICATION_START)
{
	threadQueue->listen(ifThis,TSE_DESTROY);
	threadQueue->destroy();
	return rDO|FIN_THREAD_ROOT_LOOP;
}
TS_STATE(FIN_THREAD_ROOT_LOOP)
{
	{
	sThreadMutexHandleRelease __hdr(mtx);
		if ( this->fwClass.is_notNull() && this->fwClass->loop(ifThis) > 0 )
			return rDO;
	}
	if ( !C_TEST(threadQueue->state(),C_ZOM) ) {
		/* threadQueue が畳み終わるのを待つ。待っている実体は tsThread::finish() の
		 * 3 条件 (ready 空・run 空・is_stable) の *同時* 成立で、そのうち is_stable の
		 * 待ちだけをここで mtx の外へ出している。理由は stdObject::wait_stable() の
		 * コメント参照。ここを脱した時点で is_stable は一度成立しており、3 条件の
		 * 同時性は finish() が mtx の下で確かめる。 */
		{
		sThreadMutexHandleRelease __hdr(mtx);
			stdObject::wait_stable();
		}
		if ( threadQueue->finish() == 0 ) {
		/* まだ FIN に入っていない、あるいは ready/run が空でない。従来どおり 10µs 譲る
		 * (ラッチが立った後は譲らない。そのときは実質動いている worker は居ない)。 */
		sThreadMutexHandleRelease __hdr(mtx);
			usleep(10);
		}
		return rDO;
	}
	/* ★ threadQueue コルーチンが zombie でも、最後の worker pthread は __tsThread_body の tail
	 * (THIS/target の relref)をまだ実行中のことがある。全 worker が完全終了(tsThreadLiveWorkers==0)
	 * するまで main(この状態は fw ループ=main 上)で待つ。ここを越えてから threadQueue 解放 →
	 * 後続の static 破棄が worker の tail relref と競合しない。FIN_WAIT(worker 上で走る)ではなく
	 * ここ(worker でない main)で待つのが肝(self-join/self-wait を避ける)。 */
	if ( tsThreadLiveWorkers != 0 ) {
	sThreadMutexHandleRelease __hdr(mtx);
		usleep(10);
		return rDO;
	}
	threadQueue = thNULL;
	
	eventHandler_flag = 0;
	gc->destroy();
	gc->listen(ifThis,TSE_DESTROY);
	return rDO|FIN_TS_APPLICATION_WAIT;
}
TS_STATE(FIN_TS_APPLICATION_WAIT)
{
	if ( C_TEST(gc->state(),C_ZOM) == 0 )
		return 0;
	return rDO|FIN_TS_APPLICATION_WAIT2;
}
TS_STATE(FIN_TS_APPLICATION_WAIT2)
{
	this->fwClass = thNULL;
	this->global = thNULL;
	gc = thNULL;
	threadQueue = thNULL;
	if ( restart_flag ) {
		restart_flag = 0;
		return rDO|INI_START;
	}
	/* appMtx を離してから gc_thread の終了を待つ。ディスパッチ(eventHandler)は非 C_THR
	 * 状態を appMtx 保持のまま連鎖実行するため、この状態も appMtx を握ったまま入る。だが
	 * gc_thread は idle 化した dying object の refEvent を dispatch する際 eventHandler()→
	 * appMtxLock() で *同じ* appMtx を要求する。appMtx を握ったまま finish()(=gc_thread の
	 * 終了待ち)に入ると、gc は appMtx を取れず gc(0) の中で詰まり、作業キューが空にならず
	 * finish_flag も読まれず両者永眠する(ロック順序逆転デッドロック。実アプリの soak
	 * テストで 1〜3% 再現)。finish() は appMtx 非保持で呼ぶ(CLAUDE.md 鉄則3)。すぐ上の
	 * FIN_THREAD_ROOT_LOOP が fwClass->loop を回すのと同じ sThreadMutexHandleRelease で
	 * appMtx を一時解放し、スコープ離脱で再取得する(mtxLock_flag は保持のまま=整合)。 */
	{
	sThreadMutexHandleRelease __hdr(mtx);
		stdObject::finish();
	}
	return rDO|FIN_TINYSTATE_START;
}

