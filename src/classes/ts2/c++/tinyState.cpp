


#include	"_ts2/c++/tinyState_.h"
#include	"ts2/c++/tsFinishChecker.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sThreadMutexRecursive.h"
#include	"ts2/c++/sThreadMutexHandleRelease.h"
#include	"ts2/c++/sCallSection.h"

CLASS_TINYSTATE(tinyState,)

#if 0

TS_BEGIN_IMPLEMENT

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

	TS_STATE_TYPE			_state;

	static int			seqNo;

	unsigned			state_lock:1;
	unsigned			invoke_state_flag:1;
	unsigned			enter_zom:1;

	unsigned			destroy_flag:1;
	unsigned			ref_destroy_flag:1;

	unsigned			mtxLock_flag:1;
	unsigned			thrQueueing_flag:1;
  	unsigned			thrIns_flag:1;

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

	virtual sPtr<stdEvent> 	filter(sPtr<stdEvent>  inp);
	sPtr<stdEvent> _delEvent();
	sPtr<stdEvent> delEvent();
	int _insEvent(sPtr<stdEvent> ev);

 	unsigned			check_listener:1;
	unsigned			destroy_stop:1;
	unsigned			finish_checker:1;
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
	static int	finish_checker;
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
int
tinyState::finish_checker = 0;

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
	if ( getref() == 0 ) {
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

		this->_state = INI_START;
		this->state_lock = 0;
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
	application->mtx.lock();
}

void
tinyState_::appMtxUnlock()
{
	if ( mtxLock_flag == 0 )
		return;
	mtxLock_flag = 0;
	application->mtx.unlock();
}

const char *
tinyState_::getStateName()
{
sThreadMutexHandle __hdr(lm);
	return C_NAME(this->_state);
}

TS_STATE_TYPE
tinyState_::state()
{
sThreadMutexHandle __hdr(lm);
	return _state;
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




void
tinyState_::destroy(int delayFlag)
{
	{
	sThreadMutexHandle __hdr(lm);
		if ( destroy_stop )
			stdObject::panic("destroy STOP");
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
	if ( C_TEST(_state,C_ZOM) )
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
        if ( C_TEST(this->_state,C_ZOM) == 0 &&
			C_TEST (state,C_ZOM) )
                this->enter_zom = 1;
        this->_state = state;
	this->invoke_state_flag = 1;
}
void
tinyState_::invoke_state()
{
	if ( this->invoke_state_flag == 0 )
		return;
	invoke_state_flag = 0;
	if ( finish_checker && tinyState::finish_checker && C_TEST(_state,C_FIN) ) {
		finish_checker = 0;
		thNEW( tsFinishChecker,(ifThis,
			tinyState::finish_checker));
	}
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

	{
	sThreadMutexHandle __hdr(lm);

		if ( C_TEST(this->_state,C_ZOM) )
			return 0;
		_insEvent(ev);
		if ( this->state_lock )
			return 0;
		this->state_lock = 1;
		thrInfo = _thrInfo;
		sCallSection::key->push(&csn);

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
								if ( thrQueueing_flag == 0 ) {
									thrQueueing_flag = 1;
									thrIns_flag = 1;
									application->getThread()->ins(ifThis);
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
			this->invoke_state();
			if ( this->que == thNULL ||
					this->que->count == 0 )
				break;
		}

		sCallSection::key->pop(&csn);
		appMtxUnlock();
		thrInfo = thNULL;
		this->state_lock = 0;
		if ( this->_state == ZOM ) {
			application = thNULL;
			parent = thNULL;
		}
	}
	if ( thrQueueing_flag && thrInfo == thNULL )
//	if ( thrIns_flag && thrInfo == thNULL )
		application->getThread()->wakeup();

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


void
tinyState_::remove_listener(sPtr<stdEventHandle>  eh)
{
sThreadMutexHandle __hdr(lm);
	if ( this->event_listener[eh->type].is_notNull() )
		this->event_listener[eh->type]->del(eh,0);
	eh->listener->remove_handle(eh);
	if ( check_listener )
		wakeup();
}

int
tinyState_::remove_handle(sPtr<stdEventHandle>  eh)
{
sThreadMutexHandle __hdr(lm);
	if ( eh == thNULL ) {
		for ( ; this->handle_list->count ; ) {
			eh = sPtr<stdEventHandle>::d_cast
				(this->handle_list->check(thNULL,
				stdQueue<stdEventHandle>::head_stdQueue));
			eh->source->remove_listener(eh);
		}
	}
	else	this->handle_list->del(eh,0);
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

void
tinyState_::clean_stdEventHandle(int type)
{
sPtr<stdQueue<stdEventHandle> >  q;
sPtr<stdEventHandle>  hdr;
sThreadMutexHandle __hdr(lm);
	q = get_handle_list(type);
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
	if ( C_TEST(_state,C_ZOM) || handle_list == thNULL )
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


