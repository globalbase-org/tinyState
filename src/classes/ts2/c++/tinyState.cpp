


#include	"_ts2/c++/tinyState_.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sThreadMutexRecursive.h"
#include	"ts2/c++/sThreadMutexHandleRelease.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/tsProbe.h"	/* 撤収プローブ: 撤収中に積みに来た相手を記録する */
#include	"ts2/c++/stdInterval.h"	/* eventHandler() 入場時刻 (enter_time) の now() */
#include	"ts2/c++/tsGC.h"	/* 深すぎる入れ子の逃がし先 gc->exe(obj,ev) */
#include	"ts2/c++/ts2Revision.h"
#include	"tinyState_revision.h"	/* ★ 版の文字列を引くのはツリー内でここだけ */

#include	<cstdio>

/* 版の文字列を **成果物に焼き込む**。ヘッダのマクロだけだと「何を建てたか」は
 * 分かるが「何が動いているか」が分からない (古い lib に新しいヘッダで建てた、
 * 静的と共有を別の木から建てた、生成ヘッダが stale — 実際に起きている)。
 *
 * ここ (tinyState.cpp) に置くのは **常に引かれる TU だから**。専用の TU に分けると
 * 誰も参照しないので静的リンクで落ちる。外部リンケージの配列にしてあるので
 * `nm` でも `strings` でも消費側のバイナリから読める。 */
const char	ts2_revision_string[] = "tinyState " TS_REVISION;

const char *
ts2_revision()
{
	/* 先頭の "tinyState " を飛ばして describe の出力そのままを返す。
	 * 接頭辞は strings で拾うときの目印で、返す値には含めない。 */
	return ts2_revision_string + sizeof("tinyState ") - 1;
}

CLASS_TINYSTATE(tinyState,)

#if 0

TS_BEGIN_IMPLEMENT

#include	<atomic>
#include	"ts2/c++/tsThread.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadMutexRecursive.h"
#include	"ts2/c++/sException.h"
#include	<functional>

class tinyState_;
class tsApplication;

#define ifThis		(this->ifp)

#define CLASS_TINYSTATE(type,base) \
TS_IMPLEMENT



/**
 * @brief tinyState フレームワークの基底クラス。/ Base class of the tinyState state-machine framework.
 * @details
 * 全ての tinyState オブジェクトはこのクラスを継承し、`TS_STATE` / `TS_THREAD` マクロで
 * 定義した状態関数を `eventHandler` 経由で実行する。
 *
 * **ライフサイクル**: `INI_START` → `ACT_START` → `FIN_START` → `FIN_TINYSTATE_START` → ZOM 状態
 *
 * **重要なメンバ**:
 * - `application` — `tsApplication` への参照。フレームワーク全体で共有されるリソース (fw, gc, mtx) にアクセスする。
 * - `parent` — 親 tinyState。TSE_RETURN / TSE_DESTROY をここに通知する。
 * - `listen()` — 他 tinyState のイベントを購読する。
 * - `invoke_listen()` — 自分のリスナーへイベントを配信する。
 * - `destroy()` — 安全な終了シーケンスを開始する。
 * - `wakeup()` — スリープ中の tinyState を起こす。
 *
 * 詳細は CLAUDE.md / COOKBOOK.md 参照。
 * / Base class for all tinyState objects. State functions defined with `TS_STATE`/`TS_THREAD`
 * are executed by `eventHandler`. See CLAUDE.md for usage rules.
 */
class tinyState_ : public stdObject {
public:
	sPtr<tsApplication>			application;
	sPtr<tinyState> 			parent;
	int				objId;

	tinyState_(sPtr<tinyState>  parent);
	IMP_PRIVATE virtual ~tinyState_();
	int printParent();

	virtual int eventHandler(sPtr<stdEvent>  ev,sPtr<stdThreadInfo> __thrInfo=thNULL);
	static int static_eventHandler(sPtr<tinyState> ,sPtr<stdEvent> );
	void inherit(sPtr<tinyState>  parent);
	virtual int priority(sPtr<tinyState>  caller=thNULL);
	virtual INTEGER64 realTimeLimit(sPtr<tinyState>  caller=thNULL);
	virtual void wakeup();
	void remove_listener(sPtr<stdEventHandle> );
	virtual void destroy(int delayFlag=0);
	void destroyStop();
  	int is_destroyed();
	void trace();
	void trace(sPtr<stdString>  msg);
	void trace(const char* msg);
	void trace(INTEGER64 msg);
	void trace(int msg);
	int realtime();
	void realtime(int f);
	sPtr<stdString>  is_traced();
	int remove_handle(sPtr<stdEventHandle>  eh);
	void ins_handle_list(sPtr<stdEventHandle>  eh);
	sPtr<stdEventHandle>  listen(
		sPtr<tinyState>  listener,
		int type,
		TS_HANDLER_FUNC handler);
	sPtr<stdEventHandle>  listen(
		sPtr<tinyState>  listener,
		int type);
	sPtr<stdEventHandle>  get_stdEventHandle(
		sPtr<tinyState>  listener,
		int type);
	sPtr<stdQueue<stdEventHandle> > 
		get_handle_list(int type=0);
	void clean_stdEventHandle(int type);
  	int listenerCounter(int type);

	const char * getStateName();
  	TS_STATE_TYPE			state();

	static int getSeq();
	void thrKill(int sig);

	virtual void refEvent();

	/**
	 * @brief Send an event to all registered listeners. / 登録済みリスナー全員にイベントを送る。
	 * @details
	 * Dispatches `ev` to every listener registered for `ev->type` via `listen()`.<br>
	 * If `clearFlag` is non-zero the listener list for that type is cleared after dispatch.<br>
	 * <br>
	 * Four overloads are provided:<br>
	 * - **ev** variants: pass a pre-built stdEvent.<br>
	 * - **fnc** variants: pass a lazy factory lambda to avoid allocating the event object
	 *   when no listener is registered.<br>
	 *   The lambda is called in two modes:<br>
	 *   &nbsp;&nbsp;`fnc(int* tp)` where `tp != nullptr` — type probe: set `*tp` to the event type and return `thNULL`.<br>
	 *   &nbsp;&nbsp;`fnc(nullptr)` — create and return the actual `sPtr<stdEvent>`.<br>
	 * - **except** variants: skip the specified listener.<br>
	 * <br>
	 * @code{.cpp}
	 * // send a pre-built event to all listeners
	 * invoke_listen(thNEW(stdEvent,(TSE_UPDATED, ifThis, thNULL)));
	 *
	 * // lazy version: event object is only created if a listener exists
	 * invoke_listen([this](int* tp) -> sPtr<stdEvent> {
	 *     if (tp) { *tp = TSE_UPDATED; return thNULL; }
	 *     return thNEW(stdEvent,(TSE_UPDATED, ifThis, thNULL));
	 * });
	 *
	 * // send to all except self
	 * invoke_listen(ev, ifThis);
	 * @endcode
	 *
	 * ---
	 *
	 * `listen()` で登録されたリスナー全員に `ev` を送る。<br>
	 * `clearFlag` が非ゼロの場合、送信後にそのイベントタイプのリスナーリストをクリアする。<br>
	 * <br>
	 * 4 つのオーバーロードがある:<br>
	 * - **ev** 版: 構築済みの stdEvent を渡す。<br>
	 * - **fnc** 版: リスナーが存在しない場合にイベントオブジェクトの生成コストを避けるための遅延 factory lambda。<br>
	 *   lambda の呼ばれ方:<br>
	 *   &nbsp;&nbsp;`fnc(int* tp)` かつ `tp != nullptr` → タイプ問い合わせ: `*tp` にイベントタイプをセットして `thNULL` を返す。<br>
	 *   &nbsp;&nbsp;`fnc(nullptr)` → 実際の `sPtr<stdEvent>` を生成して返す。<br>
	 * - **except** 版: 指定したリスナーをスキップする。<br>
	 * <br>
	 * @code{.cpp}
	 * // 構築済みイベントを全リスナーへ送る
	 * invoke_listen(thNEW(stdEvent,(TSE_UPDATED, ifThis, thNULL)));
	 *
	 * // 遅延版: リスナーが存在する場合のみイベントオブジェクトを生成
	 * invoke_listen([this](int* tp) -> sPtr<stdEvent> {
	 *     if (tp) { *tp = TSE_UPDATED; return thNULL; }
	 *     return thNEW(stdEvent,(TSE_UPDATED, ifThis, thNULL));
	 * });
	 *
	 * // 自分以外の全リスナーへ送る
	 * invoke_listen(ev, ifThis);
	 * @endcode
	 */
	int invoke_listen(sPtr<stdEvent>, int clearFlag=0);
	int invoke_listen(sPtr<stdEvent>, sPtr<tinyState> except, int clearFlag=0);
	int invoke_listen(tinyStateListenerFn fnc, int clearFlag=0);
	int invoke_listen(tinyStateListenerFn fnc, sPtr<tinyState> except, int clearFlag=0);

private:
	sThreadMutexRecursive			lm;
		// lock for state, state_lock que thrInfo

	/* reactor が fwIO::mu を保持したまま state() を呼ぶため、state() で lm を
	 * 取ると mu -> lm の順序ができ、lm -> mu と AB-BA になる。atomic にして
	 * state() からロックを外し、「fwIO::mu の内側で fwIO の外を呼ばない」を
	 * 例外なしの規律にする。std::atomic<INTPTR> は lock-free で、素の INTPTR と
	 * サイズもアラインも同じなのでオブジェクトの配置は変わらない。
	 * 注意: C_TEST/C_NAME は引数を 2 回評価するので、必ずローカルへ
	 * スナップショットしてから渡すこと (2 回目が別値だと (TS_TRANS*)0 になる)。 */
	std::atomic<TS_STATE_TYPE>	_state;

	static int			seqNo;

	unsigned			state_lock:1;
	unsigned			invoke_state_flag:1;
	unsigned			enter_zom:1;

	unsigned			destroy_flag:1;
	unsigned			ref_destroy_flag:1;

	unsigned			mtxLock_flag:1;
	unsigned			thrQueueing_flag:1;

	INTEGER64			onlyOneEvent_mask;
	INTEGER64			onlyOneEvent_occur;

	void appMtxLock();
	void appMtxUnlock();

	sPtr<stdEventHandle>  add_listener(
		sPtr<tinyState>  listener,
		int type,
		TS_HANDLER_FUNC handler);


	sPtr<stdString>  			trace_msg;
  	sPtr<stdQueue<stdEvent> > 		que;
	sArray<sPtr<stdQueue<stdEventHandle> > >
					event_listener;
	sPtr<stdQueue<stdEventHandle> > 	handle_list;

	void invoke_state();
	void invoke_check(TS_STATE_TYPE);
	void print_trace(const char*,TS_STATE_TYPE);
	sPtr<stdEventHandle> 
	search_listen(int type,sPtr<tinyState>  listener);
	static inline TS_STATE_FUNC TS_FORCEINLINE getFunc(TS_STATE_TYPE,TS_TRANS**);

protected:
	sPtr<stdThreadInfo> 			thrInfo;

	/**
	 * @brief この eventHandler() が入場した時刻 (マイクロ秒)。協調的に長いループを
	 *        切り上げるために状態関数から読む。
	 *        / Microsecond timestamp at which this eventHandler() dispatch was entered;
	 *        read it from a state function to break out of a long loop cooperatively.
	 * @details
	 * `stdInterval::now()` と同じ尺度・同じ単位 (マイクロ秒) なので、状態関数の中から
	 * そのまま差を取れる。<br>
	 * <br>
	 * **用途**: 状態遷移関数の中に長いループ (生成コードの for/while など) が要ることが
	 * ある。長い、まして無限のループに入ると、その間この スレッドは占有されたままなので
	 * 他の tinyState 派生クラスが動けなくなる。これはユーザが書くコードなので
	 * フレームワーク側では防げない。そこで、そうなり得るループを書く側がこの値を読んで、
	 * 予算を超えたら自分でループを抜ける。<br>
	 * <br>
	 * **抜ける書き方**: `rDO` **無し**の `return` がスレッドを返す道。`rDO|` 付きの
	 * return は同じディスパッチの中で次の状態へ進むだけで、スレッドは手放さない。
	 * 起こし直しは `application->gc->exe(ifThis)` (他の仕事の後ろに並び直す) か
	 * `stdInterval::wait()` を使う。<br>
	 * <br>
	 * @code{.cpp}
	 * TS_STATE(ACT_LOOP)
	 * {
	 *     for ( ; ; ) {
	 *         if ( enter_time + 100*1000 < stdInterval::now() )
	 *             break;              // 0.1 秒使った → いったん譲る
	 *         if ( one_step() == DONE )
	 *             return rDO|ACT_DONE;
	 *     }
	 *     application->gc->exe(ifThis);   // 順番の最後に並び直す
	 *     return ACT_LOOP_WAIT;           // ★ rDO 無し = ここでスレッドを返す
	 * }
	 * TS_STATE(ACT_LOOP_WAIT)
	 * {
	 *     R_TEST                          // gc からの TSE_RETURN を待つ
	 *     return rDO|ACT_LOOP;            // 入場し直しているので enter_time は更新済み
	 * }
	 * @endcode
	 *
	 * @note 打たれるのは**最外周の入場時だけ**。state_lock を既に持っている間の
	 *       再入呼び出しはイベントを積んですぐ戻るので、更新しない。したがってこの値が
	 *       表すのは「このオブジェクトがスレッドを連続して占有し始めた時刻」であって、
	 *       個々の状態関数の開始時刻ではない。`rDO` の連鎖も、キューに溜まった複数
	 *       イベントの処理も、同じ入場の内側なので同じ値のままになる。占有時間の予算と
	 *       しては、これがそのまま測りたいものになっている。
	 * @note 取得コストは 1 ディスパッチあたり約 13ns (実測。1 回分の差は
	 *       ディスパッチのばらつきに埋もれるので、状態関数から 20 回呼んで傾きから
	 *       割り出した: +261.4ns / 20)。ディスパッチ本体が約 2.16us なので **+0.6%**。
	 *       `stdInterval::now()` は 2026-10-02 に無ロック化 (CAS) してあり、
	 *       スレッド数を増やしても劣化しない (mutex 版は 8 スレッドで約 86ns、
	 *       CAS 版は約 1.3ns)。
	 */
	INTEGER64			enter_time;

	virtual sPtr<stdEvent> 	filter(sPtr<stdEvent>  inp);
	sPtr<stdEvent> _delEvent();
	sPtr<stdEvent> delEvent();
	int _insEvent(sPtr<stdEvent> ev);

 	unsigned			check_listener:1;
	unsigned			destroy_stop:1;
	unsigned			filter_lock:1;
	int				realtime_pri;
};
TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
#include	<functional>
class stdEvent;
using tinyStateListenerFn = std::function<sPtr<stdEvent>(int*)>;
class tinyState : public stdObject {
public:
	virtual ~tinyState();
	void _nRefEvent(int n) {
		nRefEvent(n);
	}
	static const char * trace_all;
	static TS_REFER *referList;
};
TS_END_INTERFACE

#endif

/*******************************************
	IMPLEMENT
********************************************/



const char *
tinyState::trace_all;
TS_REFER *
tinyState::referList = 0;

tinyState::~tinyState()
{
	this->impl = thNULL;
}


int
tinyState_::realtime()
{
	return realtime_pri;
}
void
tinyState_::realtime(int pri)
{
	realtime_pri = pri;
}

tinyState_::tinyState_(sPtr<tinyState>  parent)
	:
	event_listener(0)
{
	this->ref_destroy_flag = 0;
}


tinyState_::~tinyState_()
{
}

void
tinyState_::refEvent()
{
	/* refEvent() is dispatched from the GC while a relref on the *interface* is in
	   progress: the drop watch is armed on the interface (inherit(): ifp->_nRefEvent(0)),
	   and relref() queues the object to the refEvent list WITHOUT decrementing (it defers
	   the decrement until after this callback).  So at this point ifp->getref() still
	   includes the reference now being released — "no external holder remains after this
	   release" is ifp->getref()==1 (the last handle, about to go), NOT ==0 (unreachable
	   here) and NOT the impl's getref() (which stays pinned by e.g. fwIO I/O registration
	   while I/O is live).  Detecting it here lets an object auto-start teardown once its
	   last public handle is dropped. */
	if ( ifp->getref() == 1 ) {
		this->ref_destroy_flag = 1;
	sPtr<stdEvent>  ev;
		ev = thNEW( stdEvent,(TSE_INVOKE,ifThis,(INTEGER64)0));
		this->eventHandler(ev);
	}
	return;
}




void
tinyState_::inherit(sPtr<tinyState>  parent)
{
	ifp->_nRefEvent(0);
	onlyOneEvent_mask = 
		TSE_EVM(TSE_WAKEUP)|
		TSE_EVM(TSE_STATE)|
		TSE_EVM(TSE_DESTROY)|
		TSE_EVM(TSE_PRIORITY);
	this->que = thNEW( stdQueue<stdEvent>,());
	this->parent = parent;
	if ( parent.is_notNull() )
		this->application = parent->application;
	else
		this->application = sPtr<tsApplication>::d_cast(ifThis);

	{
	sThreadMutexHandle __hdr(lm);

		this->_state.store(INI_START,std::memory_order_release);
		this->state_lock = 0;
		this->enter_time = 0;
		this->invoke_state_flag = 0;
		this->objId = getSeq();

	TS_REFER * rf;
		rf = this->getRefer();
		rf->count ++;
		if ( tinyState::referList == 0 ||
				tinyState::referList->count < rf->count )
			tinyState::referList = rf;
		try {
			filter(thNULL);
			filter_lock = 1;
		} catch ( sException& ex ) {
			filter_lock = 0;
		}
	}
	this->eventHandler(thNEW( stdEvent,(TSE_INIT,ifThis,(INTEGER64)0)));
}

int
tinyState_::seqNo;

int
tinyState_::getSeq()
{
int ret;
	ret = seqNo ++;
	if ( ret <= 0 ) {
		ret = 1;
		seqNo = 2;
	}
	return ret;
}


void
tinyState_::appMtxLock()
{
	if ( mtxLock_flag )
		return;
	mtxLock_flag = 1;
	/* ★ 戻り値を見ていなかった。失敗は *正の errno* で来るので、取れていない
	 * のに flag を立てたまま先へ進む形になっていた。 */
	if ( application->mtx.lock() != 0 )
		stdObject::panic("appMtxLock failed");
}

void
tinyState_::appMtxUnlock()
{
	if ( mtxLock_flag == 0 )
		return;
	mtxLock_flag = 0;
	/* ★★ ここが「レベルの置き忘れ」を現場で捕まえる唯一の点。flag が立って
	 * いるのに unlock が EPERM で断られたら、この object の分として数えた
	 * レベルを実は別スレッドが持っている = 対応が崩れている。旧コードは
	 * 戻り値を捨てていたので、崩れたまま静かに走り続けた。 */
	if ( application->mtx.unlock() != 0 )
		stdObject::panic("appMtxUnlock: not the owner — app-mutex bookkeeping is broken");
}

const char *
tinyState_::getStateName()
{
sThreadMutexHandle __hdr(lm);
TS_STATE_TYPE st = this->_state;
	return C_NAME(st);
}

TS_STATE_TYPE
tinyState_::state()
{
	/* ロックを取らない。単一ワードの読み出しで、呼び出し側は返った時点の値が
	 * その後も有効だとは仮定できない (lm 版でもできなかった: 状態関数の実行中は
	 * lm が解放されている)。lm を取ると mu -> lm の順序ができるので取らない。 */
	return _state.load(std::memory_order_acquire);
}


int
tinyState_::printParent()
{
sPtr<stdString>  p;
sPtr<tinyState>  pp, * obj;
  	p = (thNEW( stdString,(this->getClass())))->add("[");
	pp = this->parent;
	for ( ; pp.is_notNull() ; pp = pp->parent ) {
	  	p = p->add("((")
	    		->add(pp->getClass())
			->add("*)")->add(thNEW( stdString,("%p",pp.__get())))
			->add(")<")->add(pp->getStateName())->add(">,");
	}
	p = p->add("TOP]");
	::printf("   %s %p %s\n",p->get_str(),this,this->getStateName());
	return 0;
}


sPtr<stdEvent> 
tinyState_::filter(sPtr<stdEvent>  ev)
{
	if ( ev == thNULL )
		throw sException(0);
	return ev;
}

void
tinyState_::wakeup()
{
	this->eventHandler(thNEW( stdEvent,(TSE_WAKEUP,ifThis,(INTEGER64)0)));
}

int
tinyState_::priority(sPtr<tinyState>  caller)
{
	return TS_DEFAULT_PRIORITY;
}

INTEGER64
tinyState_::realTimeLimit(sPtr<tinyState>  caller)
{
	return 2*1000*1000;
}




/* TS_SELF_GUARD — 自分を生かしておくピン。
 *
 * eventHandler() の中で listener へ TSE_DESTROY が配送され、その先で自分を指す最後の
 * ハンドルが落とされることがある (sPtr::operator-> は addref しないので、p->destroy() の
 * p 自身が消え得る)。eventHandler 自身は sCallSectionNode が ifThis を強参照するため
 * 生き延びるが、そこから戻った後の THR_KILL(SIGPIPE) → thrKill() →
 * sThreadMutexHandle(lm) が解放済みの lm を叩いていた。
 *
 * ★ interface (ifThis) を取ること。tinyState_ の this は impl 側で、interface は impl を
 *    強参照するが逆は sWptr なので、sPtr(this) では interface の死を止められない。
 * ★ 宣言はこの関数の先頭 = 下の sThreadMutexHandle より前でなければならない。ローカルは
 *    逆順に壊れるので、後ろに置くとピンが先に落ちて unlock が UAF になる。
 *
 * 他の listener 系 (remove_listener / remove_handle / clean_stdEventHandle /
 * stdEventHandle::remove) は「外部呼び出しを末尾へ寄せ、以後 this を触らない」構造に
 * 直したのでピンは要らない。destroy() だけは THR_KILL を eventHandler の後に置くこと
 * 自体が目的なので、構造では消せずここだけピンで受ける。 */
void
tinyState_::destroy(int delayFlag)
{
TS_SELF_GUARD;
	{
	sThreadMutexHandle __hdr(lm);
		if ( destroy_stop )
			stdObject::panic("destroy STOP");
		/* 既に ZOM = 破棄し終えた相手への destroy() は no-op にする。is_destroyed() は
		 * C_ZOM も見る (下) のに、こちらは destroy_flag しか見ていなかったため、自然完了
		 * したオブジェクトへの destroy() が destroy_flag の書き込み・TSE_INVOKE の投入・
		 * THR_KILL(SIGPIPE) まで走っていた。いずれも eventHandler 側の C_ZOM ガードで
		 * 吸収されるので観測可能な挙動は変わらないが、死んだ相手への destroy は本当に
		 * 何もしないのが正しい。
		 * destroy_stop の panic より後に置いてある。ZOM でも「destroy 禁止の相手を
		 * destroy しようとした」呼び出し側のバグは捕まえたいため。
		 * ref_destroy_flag は足さない。あれは「参照先が destroy 進行中」であって、
		 * ここで早期 return すると本当に必要な destroy を握り潰す。 */
		if ( C_TEST(this->_state,C_ZOM) )
			return;
		if ( this->destroy_flag )
			return;
		this->destroy_flag = 1;
	}
	if ( delayFlag )
		application->fw()->wait(ifThis,0,TSE_INVOKE);
	else	this->eventHandler(thNEW( stdEvent,(TSE_INVOKE,ifThis,(INTEGER64)0)));
#ifndef _WIN32
	/* Interrupt a worker blocked in a heavy syscall so it re-checks destroy.
	   On Windows there is no SIGPIPE and no thread-directed signal; the
	   equivalent (cancel a blocked overlapped op) is handled by the IO objects'
	   overlapped model + CloseHandle/CancelIoEx at FIN.  See Windows-port design memo §4. */
	THR_KILL(SIGPIPE);
#endif
}

void
tinyState_::destroyStop()
{
sThreadMutexHandle __hdr(lm);
	destroy_stop = 1;
}

int
tinyState_::is_destroyed()
{
sThreadMutexHandle __hdr(lm);
TS_STATE_TYPE st = _state;
	if ( C_TEST(st,C_ZOM) )
		return 1;
	if ( destroy_flag )
		return 1;
	if ( ref_destroy_flag )
		return 1;
	return 0;
}

void
tinyState_::trace()
{
sThreadMutexHandle __hdr(lm);
	this->trace_msg = thNULL;
}

sPtr<stdString> 
tinyState_::is_traced()
{
sThreadMutexHandle __hdr(lm);
	if ( trace_msg.is_notNull() )
		return trace_msg;
	return thNULL;
}

void
tinyState_::trace(sPtr<stdString>  msg)
{
sThreadMutexHandle __hdr(lm);
	if ( msg.is_notNull() ) {
		this->trace_msg = thNEW( stdString,(msg));
		this->print_trace("TRACE-START>> ",this->_state);
	}
	else {
		this->trace_msg = thNULL;
	}
}


void
tinyState_::trace(const char * msg)
{
	trace(thNEW( stdString,(msg)));
}


void
tinyState_::trace(int id)
{
char buf[60];
	snprintf(buf,sizeof(buf),"%i/%x",id,id);
	trace(buf);
}

void
tinyState_::trace(INTEGER64 id)
{
char buf[70];
	snprintf(buf,sizeof(buf),"%lli/%llx",id,id);
	trace(buf);
}



void
tinyState_::invoke_check(TS_STATE_TYPE state)
{
	if ( state == 0 )
        	return;
	if ( this->_state == state )
        	return;
TS_STATE_TYPE st = this->_state;
        if ( C_TEST(st,C_ZOM) == 0 &&
			C_TEST (state,C_ZOM) )
                this->enter_zom = 1;
        this->_state.store(state,std::memory_order_release);
	this->invoke_state_flag = 1;
}
void
tinyState_::invoke_state()
{
	if ( this->invoke_state_flag == 0 )
		return;
	invoke_state_flag = 0;
	this->invoke_listen([this](int*tp) {return tp ? *tp = TSE_STATE, thNULL : thNEW( stdEvent,(TSE_STATE,ifThis,_state));});
	if ( this->enter_zom )
{
		this->invoke_listen([this](int*tp) {
			return tp ? *tp = TSE_DESTROY,thNULL : thNEW( stdEvent,(TSE_DESTROY,ifThis,(INTEGER64)0));});
}
}



sPtr<stdEvent>
tinyState_::_delEvent()
{
sPtr<stdEvent> ret;
	ret = que->del();
	if ( ret == thNULL )
		return ret;
	onlyOneEvent_occur &= ~(TSE_EVM(ret->type));
	return ret;
}

sPtr<stdEvent>
tinyState_::delEvent()
{
sPtr<stdEvent> ret;
	{
	sThreadMutexHandle __hdr(lm);
		ret = _delEvent();
	}
	if ( filter_lock == 0 )
		return ret;
sThreadMutexHandle __hdr(application->mtx);
	return filter(ret);
}

int
tinyState_::_insEvent(sPtr<stdEvent> ev)
{
INTEGER64 evm;
TS_TRANS * tinfo;

	evm = TSE_EVM(ev->type);
	if ( !(evm & onlyOneEvent_mask) ) {
		this->que->ins(MAX_INTEGER64,ev);
		getFunc(this->_state,&tinfo);
		if ( thrInfo.is_notNull() && (tinfo->name[0] & C_THR) ) {
		 	thrInfo->signal();
			return 1;
		}
	}
	else if ( !(onlyOneEvent_occur & evm ) ) {
		onlyOneEvent_occur |= evm;
		this->que->ins(MAX_INTEGER64,ev);
		getFunc(this->_state,&tinfo);
		if ( thrInfo.is_notNull() && (tinfo->name[0] & C_THR) ) {
		 	thrInfo->signal();
			return 1;
		}
	}
	return 0;
}

void
tinyState_::thrKill(int sig)
{
sThreadMutexHandle __hdr(lm);
	if ( thrInfo.is_notNull() ) thrInfo->kill(sig);
}

int
tinyState_::static_eventHandler(sPtr<tinyState>  THIS,sPtr<stdEvent>  ev)
{
	return THIS->eventHandler(ev);
}


int
tinyState_::eventHandler(sPtr<stdEvent>  ev,sPtr<stdThreadInfo> _thrInfo)
{
sPtr<stdEvent>  ret;
TS_STATE_FUNC func;
TS_STATE_TYPE state;
sCallSectionNode csn(ifThis);
/* 入れ子の深さを数えるガード。入口で +1 / この関数の**本当の出口**で -1。
 *
 * ★ sCallSection の push/pop に連動させてはいけない。pop は下の invoke_state()
 * の **前**にあるので、listener 連鎖では C スタックが伸び続けるのに段数が戻り、
 * カウンタが浅いまま張り付く。RAII なら早期 return (C_ZOM / state_lock 再入 /
 * 下の逃がし) でも正しく戻る。
 *
 * TLS を引くのはここの 1 回だけ。push/pop もこの csec を使い回すので、
 * 従来 (sCallSection::key-> を 2 回) より引く回数は増えない。 */
sCallSectionDepth __depth;
sCallSection * csec = __depth.section();
/* lm 内で他オブジェクトを呼ばないため、スレッドプールへの投入と起床は
 * スコープ外へ回す。この呼び出しで実際にキューへ積んだときだけ真。 */
int do_ins = 0;

	/* ---- 入れ子が深すぎたら、イベントごと gc へ逃がす ----
	 *
	 * eventHandler は入れ子に呼ばれる (親への TSE_RETURN・listener 通知)。
	 * state_lock は**同一オブジェクトの再入**しか止めないので A→B→C の連鎖は
	 * 無制限に深くなれ、スタックを食い尽くすと**何も出さずに** SIGSEGV で死ぬ。
	 * 上限を超えた入場は ev ごと gc のキューへ回す。gc 経由の再投入は深さ 1 から
	 * 始まるので、長い連鎖は「上限 × gc 往復」に分割されて前進する。
	 *
	 * ★ 判定は _insEvent(ev) の**前**。ここで逃がす ev はこのオブジェクトの
	 * キューには入れず、gc が持って行く (二重に配送しないため)。
	 *
	 * ★ lm は取っていない。gc->exe() は他オブジェクトの呼び出しなので lm の下では
	 * 呼べず (鉄則 3 / 末尾の do_ins と同じ理由)、ここは lm を取る前に済ませる。
	 * そのぶん application の読み出しは無同期で、撤収中に thNULL へ変わり得る
	 * 窓がある — 末尾の do_ins ブロックが既に抱えている窓と同じ性質のもので、
	 * null チェック付きで読み、null なら「逃がさず従来どおり再帰する」に倒す。
	 * 落とすのは不可。
	 *
	 * ★ C_ZOM の判定より前にある。既に ZOM のオブジェクトを深い所で叩くと gc 経由に
	 * 1 往復するが、入場し直した先の C_ZOM 判定が 0 を返して終わるだけで無害。
	 *
	 * ★ _thrInfo != thNULL のときは逃がさない。それは**プール自身のディスパッチ**で、
	 * worker は既に自分のキューから降ろしている。ここで ev へ化かすと
	 * thrQueueing_flag が「プールが持っている」と言い続け、state_lock を持つ
	 * スレッドが TS_THREAD 状態で止まったまま誰も ins() し直さない。
	 * そもそも worker の最外周は深さ 1 なので、下の条件には届かない。 */
	if ( _thrInfo == thNULL && __depth.depth() > 1 ) {
	int limit = 0;
	/* ★ 1 回だけ読んでローカルに取る。C_TEST は引数を **2 回評価する**マクロで、
	 * 1 回目が非 0・2 回目が 0 になると (TS_TRANS*)0 を参照して落ちる。 */
	TS_STATE_TYPE st_now = this->_state;
		/* 深さ 1 はここに来ない。上限は必ず 1 以上なので最外周は絶対に超えず、
		 * 判定そのものが要らない (= 既定構成のディスパッチの大半で
		 * application の参照と上限の取得をまるごと省ける)。
		 * これは前進の保証でもある: 最外周は必ず走る。 */
		if ( this->application.is_notNull() )
			limit = this->application->evh_depth;	/* 明示指定が勝つ */
		if ( limit <= 0 )
			limit = __depth.limit();		/* 無指定 = 機ごとに自動 */
		/* ★★ **INI 段のディスパッチは逃がさない。**
		 *
		 * ★ **仕様**: `TS_STATE(INI_START)` とそこから `rDO` で繋がった連鎖は
		 * **`thNEW` の中で走り切る** (COOKBOOK §10 / CLAUDE.md)。呼び手は次の行から
		 * 使える。`TS_THREAD(INI_START)` と「INI_START の中で yield した場合」だけが
		 * その限りではない (どちらも元から非同期で、呼び手も待つ形に書く)。
		 *
		 * TSE_INIT を gc へ回すと thNEW は **INI_START が走る前に**返り、
		 * **その保証が壊れて**中身が空のオブジェクトが配られる。実際に落ちた形 (B 検証):
		 *
		 *   tsSignal の INI_START が tsSignalCore::ins_front() を呼ぶ
		 *     -> その tsSignalCore は直前に thNEW されたもので、**TSE_INIT が
		 *        逃がされていた**ので INI_START がまだ走っておらず、内部の
		 *        stdQueue が未生成
		 *     -> stdQueue_<tsSignal>::ins() -> stdObject::addref() で SEGV
		 *
		 * 利用側の実アプリでも同じ顔 (tsSignal / tsSignalCore など、
		 * すべて起動時の INI) で落ち、深さを変えると結果が単調に変わらない = レース
		 * だった。⇒ **構築中は再帰するしかない。**
		 *
		 * ⚠ 代償: **構築の連鎖 (INI_START の中で thNEW する形) は守れない。**
		 * ★ これは直せない穴ではなく、**上の仕様を守った結果**である (仕様として決着)。
		 * 保証を取るか深さを取るかの択で、保証を取っている。深さが要る利用側は
		 * `TS_THREAD(INI_START)` にするか「作ってから起こす」に分ける。
		 * この機構が守るのは「出来上がったオブジェクト同士のイベント連鎖」
		 * (親への TSE_RETURN ・ listener 通知 ・ wakeup ・ gc/fwIO からの配送) で、
		 * そこが「再帰の発生源」として挙がっているものと一致する。
		 * 構築の連鎖は本質的に同期なので、深さを稼ぎたければ利用側が
		 * 「作ってから起こす」形に分けるしかない。
		 *
		 * ★ 0 も INI とみなされる (C_TEST は x==0 を C_INI として扱う) ので、
		 * state がまだ設定されていない最初期も自動的にここで止まる。 */
		if ( __depth.depth() > limit && C_TEST(st_now,C_INI) ) {
			/* ★★ 上限を超えているが **INI 段なので逃がせない** (理由は下の
			 * 長いコメント)。逃がせないからといって**黙って進んではいけない**:
			 * 構築の連鎖はこのまま深くなり、スタックを使い切ったところで
			 * **何も出さずに** SIGSEGV で死ぬ。この安全網の題は「沈黙死を防ぐ」
			 * なので、**逃がせない側も沈黙はやめる**。
			 *
			 * ★ panic ではなく記録に留める理由: 1 段の見込み 4KB は実測 672B の
			 * 約 6 倍なので、上限を超えても**まだ実容量には遠い** (8MB の機では
			 * 上限 1500 に対して実際は約 12,800 段入る)。ここで止めると、
			 * 動いていたアプリを落とすことになる。⇒ 早めに**警告**し、
			 * 本当に足りなければ従来どおり落ちる。
			 *
			 * ★ 出すのは一度だけ。構築の連鎖は 1 段ごとにここを通るので、
			 * 毎回出したら出力で埋まる。 */
		static int reported_ini = 0;
			if ( reported_ini == 0 ) {
				reported_ini = 1;
				::fprintf(stderr,
					"tinyState: eventHandler nesting is at %d,"
					" past the cap of %d, in an INI dispatch of %s."
					" A construction chain cannot be deferred to the GC,"
					" so a deeper one dies without a message."
					" Split it into \"build, then fire\".\n",
					__depth.depth(),limit,this->getClass());
			}
		}
		else if ( __depth.depth() > limit ) {
		sPtr<tsGC>  gc;
			if ( this->application.is_notNull() )
				gc = this->application->gc;
			/* ★★ gc 自身は逃がさない。逃がし先が自分なので、
			 *
			 *   eventHandler(深い) -> gc->exe(obj,ev) -> wakeup()
			 *     -> gc の eventHandler(深さ +1 ・ これも上限超え)
			 *       -> gc->exe(gc,ev) -> wakeup() -> ... 無限再帰
			 *
			 * となってスタックを食い尽くす (実測: evh_depth=2 の 100 段で
			 * 即 SIGSEGV。bt は _ins -> exe -> wakeup -> eventHandler の
			 * 4 フレーム周期)。gc の入場は上限を無視して従来どおり再帰する。
			 * gc の状態機械は自分の予算 (interval()) で切り上げるので、
			 * ここで深さを見なくても スレッドを抱え込み続けはしない。
			 *
			 * exe() の wakeup() がこの深いスタックで gc の状態機械を
			 * 走らせてしまう心配は要らない: gc は ACT_RET / ACT_START で
			 * TSE_RETURN を待っているので、TSE_WAKEUP は R_TEST に弾かれて
			 * すぐ戻る。実際の配送は 0 遅延タイマの TSE_RETURN を届ける
			 * スレッドが、深さ 1 から行う。 */
			if ( gc.is_notNull() && gc != ifThis ) {
				/* 逃がしたことを記録する。既定は数えるだけで、
				 * TS2_EVH_PROBE を設定すると 1 件 1 行 stderr に出る。
				 * 狙いは「このアプリは本当に上限へ届いているのか」を
				 * 測れるようにすること。 */
			INTEGER64 seq = tsProbeEvhEscape(ifThis,
						ev.is_notNull() ? ev->type : 0,
						__depth.depth(),limit);
				gc->exe(ifThis,ev,seq);
				return 0;	/* 戻り値を見ている呼び出し元は無い */
			}
			/* 逃がし先が無い (撤収中で application / gc が畳まれた) →
			 * 従来どおり再帰する。深い鎖のまま進むしかないが、落とさない。 */
		}
	}

	{
	sThreadMutexHandle __hdr(lm);

	TS_STATE_TYPE st0 = this->_state;
		if ( C_TEST(st0,C_ZOM) )
			return 0;
		_insEvent(ev);
		if ( this->state_lock ) {
			/* A non-null _thrInfo means this call IS the pool's dispatch -- the
			   worker has already taken us off its queue.  Turning it into a mere
			   event would leave thrQueueing_flag claiming "the pool has me", so
			   the thread that owns state_lock parks on the TS_THREAD state and
			   nobody ever ins() again.  Retract the claim: the next evaluation
			   of the state queues us afresh. */
			if ( _thrInfo != thNULL )
				this->thrQueueing_flag = 0;
			return 0;
		}
		this->state_lock = 1;
		/* 入場時刻を打つ。状態関数が長いループを自分で切り上げるための基準で、
		 * 読み手は enter_time のコメント参照。state_lock を取れた呼び出し =
		 * 最外周だけがここへ来るので、再入では更新されない (それが欲しい意味:
		 * 「このオブジェクトがスレッドを占有し始めた時刻」)。 */
		this->enter_time = stdInterval::now();
		thrInfo = _thrInfo;
		csec->push(&csn);

		for ( ; ; ) {
			for ( ; ; ) {
				ret = _delEvent();
				if ( ret == thNULL )
					break;
				if ( filter_lock ) {
				sThreadMutexHandleRelease __hdr(lm);
				sThreadMutexHandle __hdr2(application->mtx);
					ret = filter(ret);
					if ( ret == thNULL )
						break;
				}
				for ( ; ; ) {
				TS_TRANS * tinfo;
					if ( this->trace_msg.is_notNull() || tinyState::trace_all )
						this->print_trace(">>>",this->_state);
					func = getFunc(this->_state,&tinfo);
					if ( func == 0 ) {
						state = rDO|ERR_START;
					}
					else {
						try {
							if ( !(tinfo->name[0] & C_THR) ) {
						  		thrQueueing_flag = 0;
								sThreadMutexHandleRelease __hdr(lm);
								appMtxLock();
								state = (*func)(this,ret);
							}
							else if ( thrInfo == thNULL ) {
								/* ワーカ未割当。ここで getThread()->ins() を
								 * 直接呼ぶと lm を保持したまま fwIO::addRefio()
								 * → fwIO::mu を取りに行き、mu を保持して
								 * state() (= lm) を待つ reactor と AB-BA になる。
								 * 実際の投入は lm 解放後に行う。2025-01-04 に
								 * wakeup() を同じ理由でスコープ外へ移したのと
								 * 同じ扱い。 */
								if ( thrQueueing_flag == 0 ) {
									thrQueueing_flag = 1;
									do_ins = 1;
								}
								state = 0;
							}
							else {
							  	thrQueueing_flag = 0;
								sThreadMutexHandleRelease __hdr(lm);
								appMtxUnlock();
								thrInfo->start();
								state = (*func)(this,ret);
								thrInfo->finish();
							}
						}
						catch (sException & ex) {
							switch ( ex.type ) {
							case EX_STAY:
								state = 0;
								break;
							case EX_ERROR:
							default:
								state = rDO|ERR_START;
							}
						}
					}
					if ( this->trace_msg.is_notNull() || tinyState::trace_all )
						this->print_trace("\t\t<<<",state);
					if ( state == ZOM )
						break;
					if ( state & rDO ) {
						state &= ~rDO;
						this->invoke_check(state);
						continue;
					}
					break;
				}
				this->invoke_check(state);
				if ( state == ZOM )
					break;
			}
			if ( this->que == thNULL ||
					this->que->count == 0 )
				break;
		}

		csec->pop(&csn);
		appMtxUnlock();
		thrInfo = thNULL;
		this->state_lock = 0;
		if ( this->_state == ZOM ) {
			application = thNULL;
			parent = thNULL;
		}
	}
	/* ここから先は lm を解放済み。どれも他オブジェクトを呼ぶので lm 内では
	 * 実行できない (invoke_state は listener へ、ins/wakeup は fwIO::mu へ届く)。
	 *
	 * 起床は ins() が行う。投入したときだけでよく、既にキュー済みの再入では
	 * プール側の状態は何も変わっていないので起こす意味がない。旧コードは
	 * thrQueueing_flag が立っている限り毎回起こしており、`&& thrInfo == thNULL`
	 * でそれを「投入したときだけ」に絞ろうとしたように見えるが、thrInfo は直前に
	 * 無条件で thNULL にされるため常に真で、条件として機能していなかった
	 * (しかも lm 外の読み出しなので、別スレッドが thrInfo を書くと起床を
	 * 取りこぼす側に倒れた)。 */
	this->invoke_state();
	if ( do_ins ) {
	sPtr<tsThread>  th;
		/* ★ ここは撤収中に両方とも thNULL になり得る。sPtr::operator-> は null 検査を
		 * しないので、素で書くと落ちる。
		 *
		 *   application  … すぐ上の ZOM 分岐でこの dispatch 中に thNULL にされる
		 *   getThread()  … tsApplication の FIN_THREAD_ROOT_LOOP が threadQueue を
		 *                  thNULL にした後 (tsApplication.cpp:301 / 319)
		 *
		 * 後者は実測されている。gc スレッドが dying object の refEvent を配送し、
		 * そこから状態機械が C_THR 状態へ入ってここに来る:
		 *
		 *   #0 sPtr<tsThread_>::operator->   #1 tsThread::ins (this=0x0)
		 *   #2 eventHandler  #3 refEvent  #4.. stdObject::gc  gc_thread
		 *
		 * tsThread_::ins() 側の防御 (ready が thNULL なら積まない) には**到達しない**。
		 * interface の forwarder が this=0x0 で落ちるので、プール本体に入る前。
		 *
		 * プールが無い = この TS_THREAD 状態は走らない。捨てるしかないが黙っては捨てず、
		 * 誰の仕事だったかを名指しする。撤収を止めない理由は tsThread_::ins() のコメント
		 * (gc の終了条件に載らないので is_stable() は真になる) と同じ。 */
		if ( application.is_notNull() )
			th = application->getThread();
		if ( th.is_notNull() )
			th->ins(ifThis);		/* ins() がプールを起こす */
		else {
			/* 窓 C = プールそのものが既に無い。ここも「積みに来た側」なので、
			 * 有効なら素性とスタックを固定しておく (撤収プローブ)。 */
			if ( tsProbeTeardown_enabled() )
				tsProbeTeardown(ifThis,"C",
					application.is_null()
						? "pool=gone (application cleared)"
						: "pool=gone (threadQueue folded)");
			::printf("tinyState: no thread pool for this TS_THREAD state"
				" — it will not run\n");
			this->printParent();
		}
	}

	return 0;
}

int
tinyState_::listenerCounter(int type)
{
sThreadMutexHandle __hdr(lm);
	if ( type < 0 || type >= TSE_MAX )
		return 0;
	if ( this->_state == ZOM )
		return 0;
	if ( this->event_listener.length() == 0 )
		return 0;

	if ( this->event_listener[type] == thNULL )
		return 0;
	return this->event_listener[type]->count;
}

sPtr<stdEventHandle> 
tinyState_::add_listener(
	sPtr<tinyState>  listener,int type,TS_HANDLER_FUNC handler)
{
sPtr<stdEventHandle>  eh;
sThreadMutexHandle __hdr(lm);
	if ( type < 0 || type >= TSE_MAX )
		return thNULL;
	if ( this->_state == ZOM )
		return thNULL;
	if ( listener->state() == ZOM )
		return thNULL;
	this->event_listener.length(TSE_MAX);
	if ( this->event_listener[type] == thNULL )
		this->event_listener[type] = 
				thNEW( stdQueue<stdEventHandle>,());
	if ( handler == 0 )
		handler = &tinyState::static_eventHandler;
	eh = thNEW( stdEventHandle,(ifThis,
				listener,type,handler));
	this->event_listener[type]->ins(MAX_INTEGER64,eh);
	listener->ins_handle_list(eh);
	if ( check_listener )
		wakeup();
	return eh;
}


/* 外部呼び出し (listener 側の remove_handle) は lm を放した後の末尾に置く。それ以降
 * this を触らない — sThreadMutexHandle の unlock も含めて — ので、この取り外しで自分の
 * 最後の参照が落ちても安全に抜けられる。eventHandler 末尾と同じ規律。
 *
 * eh->listener は stdEventHandle::remove() 経由なら既に thNULL で、その場合 listener 側は
 * remove() 自身が外す。直接呼ばれた場合 (public API) は従来どおりここが両端を外す。
 *
 * check_listener はツリー内のどこでも代入されていない = 常に偽だった。ここの読み出しは
 * 上記の規律 (lm 下で外を呼ばない) と両立しないので落とす。add_listener 側の読み出しは
 * 残置 — あちらは構造上安全で、この変更の対象ではない。member 自体の扱いは別途。 */
void
tinyState_::remove_listener(sPtr<stdEventHandle>  eh)
{
sPtr<tinyState>  lsn;
	{
	sThreadMutexHandle __hdr(lm);
		if ( this->event_listener.length() &&
				this->event_listener[eh->type].is_notNull() )
			this->event_listener[eh->type]->del(eh,0);
		lsn = eh->listener;
	}
	if ( lsn.is_notNull() )
		lsn->remove_handle(eh);
}

/* eh 指定は自分のリストを触るだけの葉。eh == thNULL の全掃除は、lm の下でリストを
 * 丸ごと引き取ってから lm を放し、その後 this を触らずに回す。
 *
 * 旧実装は lm を保持したまま eh->source->remove_listener() を呼び、毎周
 * this->handle_list を読み直していた。最後の 1 本を外した時点で自分の最後の参照が
 * 落ちると、__hdr のデストラクタが解放済みの lm を unlock する。
 * ついでに handle_list == thNULL (listen を一度も張っていない) での null 参照も直る。 */
int
tinyState_::remove_handle(sPtr<stdEventHandle>  eh)
{
sPtr<stdQueue<stdEventHandle> >  drained;
	if ( eh != thNULL ) {
	sThreadMutexHandle __hdr(lm);
		if ( this->handle_list.is_notNull() )
			this->handle_list->del(eh,0);
		return 0;
	}
	{
	sThreadMutexHandle __hdr(lm);
		drained = this->handle_list;
		this->handle_list = thNULL;
	}
	if ( drained == thNULL )
		return 0;
	for ( ; (eh = drained->del()).is_notNull() ; )
		eh->remove();
	return 0;
}

void
tinyState_::ins_handle_list(sPtr<stdEventHandle>  eh)
{
sThreadMutexHandle __hdr(lm);
	if ( this->handle_list == thNULL )
		this->handle_list = 
				thNEW( stdQueue<stdEventHandle>,());
	this->handle_list->ins(0,eh);
}

/* get_handle_list() はコピーを返すので lm はその取得だけで足りる。ループを lm の外へ
 * 出すと以降 this を触らずに済み (unlock も済んでいる)、最後のハンドルを外した時点で
 * 自分の最後の参照が落ちても安全に抜けられる。 */
void
tinyState_::clean_stdEventHandle(int type)
{
sPtr<stdQueue<stdEventHandle> >  q;
sPtr<stdEventHandle>  hdr;
	{
	sThreadMutexHandle __hdr(lm);
		q = get_handle_list(type);
	}
	if ( q == thNULL )
		return;
	for ( ; (hdr = q->del()).is_notNull() ; )
		hdr->remove();
}

sPtr<stdQueue<stdEventHandle> > 
tinyState_::get_handle_list(int type)
{
sPtr<stdQueueElement<stdEventHandle> > elp;
sPtr<stdQueue<stdEventHandle> >  ret;
sThreadMutexHandle __hdr(lm);
TS_STATE_TYPE st = _state;
	if ( C_TEST(st,C_ZOM) || handle_list == thNULL )
		return thNULL;
 	if ( type == 0 )
	 	return thNEW( stdQueue<stdEventHandle>,(handle_list));
	ret = thNEW( stdQueue<stdEventHandle>,());
	for ( elp = handle_list->head ; elp.is_notNull() ; elp = elp->next )
		if ( elp->data->type == type )
			ret->ins(MAX_INTEGER64,elp->data);
	return ret;
}

sPtr<stdEventHandle> 
tinyState_::get_stdEventHandle(sPtr<tinyState>  listener,int type)
{
	return search_listen(type,listener);
}


sPtr<stdEventHandle> 
tinyState_::search_listen(int type,sPtr<tinyState>  listener)
{
sPtr<stdQueue<stdEventHandle> >  q;
sThreadMutexHandle __hdr(lm);
	if ( this->event_listener.length() == 0 )
		return thNULL;
	if ( type <= 0 || type >= this->event_listener.length() )
		return thNULL;
	q = this->event_listener[type];
	if ( q == thNULL )
		return thNULL;
	return sPtr<stdEventHandle>::d_cast
			(q->check([listener](sPtr<stdEventHandle> eh){
					if ( eh->listener == listener )
						return 1;
					return 0;
				}));
}

sPtr<stdEventHandle> 
tinyState_::listen(sPtr<tinyState>  listener,int type,TS_HANDLER_FUNC handler)
{
sPtr<stdEventHandle>  eh;
sThreadMutexHandle __hdr(lm);
	if ( listener == thNULL )
		return thNULL;
	eh = this->search_listen(type,listener);
	if ( eh.is_notNull() )
		return eh;
	return this->add_listener(listener,type,handler);
}

sPtr<stdEventHandle> 
tinyState_::listen(sPtr<tinyState>  listener,int type)
{
sPtr<stdEventHandle>  eh;
sThreadMutexHandle __hdr(lm);
 	if ( listener == thNULL )
		return thNULL;
	eh = this->search_listen(type,listener);
	if ( eh.is_notNull() )
		return eh;
	return this->add_listener(listener,type,0);
}



int
tinyState_::invoke_listen(std::function<sPtr<stdEvent>(int*)> fnc,int clearFlag)
{
	return invoke_listen(fnc,thNULL,clearFlag);
}

int
tinyState_::invoke_listen(sPtr<stdEvent> ev,sPtr<tinyState>  except,int clearFlag)
{
	return invoke_listen([&ev](int*tp) {return tp ? *tp=ev->type,thNULL : ev;},except,clearFlag);
}

int
tinyState_::invoke_listen(std::function<sPtr<stdEvent>(int *)> fnc,sPtr<tinyState>  except,int clearFlag)
{
int type;
sPtr<stdQueue<stdEventHandle> >  q, nq;
sPtr<stdEvent>  _ev,ev;

	{
	sThreadMutexHandle __hdr(lm);

		fnc(&type);
		if ( this->event_listener.length() == 0 )
			return 0;
		if ( type <= 0 || type >= this->event_listener.length() )
			return 0;
		q = this->event_listener[type];
		if ( q == thNULL )
			return 0;
		if ( q->count == 0 )
			return 0;
		ev = fnc(0);
		if ( clearFlag ) {
			nq = q;
			this->event_listener[type] = thNULL;
		}
		else{
			nq = thNEW( stdQueue<stdEventHandle>,(q));
		}
	}

	nq->check([&ev,&except](sPtr<stdEventHandle> eh) {
		if ( eh->listener == except )
			return 0;
		(*eh->handler)(eh->listener,ev);
		return 0;
	});
	return 0;
}

int
tinyState_::invoke_listen(sPtr<stdEvent>  ev,int clearFlag)
{
	return invoke_listen(ev,thNULL,clearFlag);
}

inline TS_STATE_FUNC TS_FORCEINLINE
tinyState_::getFunc(TS_STATE_TYPE inp,TS_TRANS**trs)
{
TS_STATE_TYPE state;
TS_TRANS *tr;

	state = inp;
	state &= TS_STATE_BIT;
	tr = (TS_TRANS*)state;
	*trs = tr;
	return tr->func;
}


void
tinyState_::print_trace(const char * ind,TS_STATE_TYPE state)
{
	if ( trace_msg == thNULL && tinyState::trace_all )
		trace_msg = thNEW( stdString,(tinyState::trace_all));
	state = state & (~rDO);
	printf("[%s:%s:(%p/%p)] (%i/%i) %i:%i %s %s\n",
		this->getClass(),
		&this->trace_msg->ary[0],
		this,ifThis.__get(),
		this->getref(),
		this->ifp->getref(),
		this->ref_destroy_flag,
		this->destroy_flag,
		ind,
	        (state ? C_NAME(state) :
			(this->_state ? "" : C_NAME(0))
			));
	fflush(stdout);
}

/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	return rDO|INI_TINYSTATE_FINISH;
}
TS_STATE(INI_TINYSTATE_FINISH)
{
	return rDO|ACT_START;
}
TS_STATE(ACT_START)
{
	return rDO|ACT_TINYSTATE_CHECK1;
}
TS_STATE(ACT_TINYSTATE_CHECK1)
{
	if ( this->ref_destroy_flag )
		return rDO|FIN_START;
	return rDO|ACT_TINYSTATE_CHECK2;
}
TS_STATE(ACT_TINYSTATE_CHECK2)
{
	if ( this->destroy_flag )
		return rDO|FIN_START;
	return rDO|ACT_TINYSTATE_START;
}
TS_STATE(ACT_TINYSTATE_START)
{
	return ACT_START;
}
TS_STATE(FIN_START)
{
	return rDO|FIN_TINYSTATE_START;
}
TS_STATE(FIN_TINYSTATE_START)
{
	return rDO|ZOM_START;
}
TS_STATE(ZOM_START)
{
	return rDO|ZOM_TINYSTATE_LAST;
}
TS_STATE(ZOM_TINYSTATE_LAST)
{
	return rDO|ZOM_TINYSTATE_LAST2;
}
TS_STATE(ZOM_TINYSTATE_LAST2)
{
int i;
sPtr<stdQueue<stdEventHandle> >  q;
sPtr<stdEventHandle>  eh;

	this->invoke_state_flag = 1;
	this->invoke_state();
	if ( this->handle_list.is_notNull() )
		for ( ; ; ) {
			eh = sPtr<stdEventHandle>::d_cast
				(this->handle_list->del());
			if ( eh == thNULL )
				break;
			eh->remove();
		}
	if ( this->event_listener.length()) {
		int len = this->event_listener.length();
		for ( i = 0 ; i < len ; i ++ ) {
			q = this->event_listener[i];
			if ( q == thNULL )
				continue;
			for ( ; ; ) {
				eh = sPtr<stdEventHandle>::d_cast(q->del());
				if ( eh == thNULL )
					break;
				eh->remove();
			}
		}
	}
	this->que = thNULL;
	this->handle_list = thNULL;


	TS_REFER * rf = getRefer();
	rf->count --;
	ifp->_nRefEvent(-1);
	return ZOM;
}
TS_STATE(ZOM)
{
	this->trace_msg = thNULL;
	return 0;
}
TS_STATE(ERR_START)
{
	return 0;
}


