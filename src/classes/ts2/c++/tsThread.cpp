
#include	"ts2/c++/sRptr.h"
#include	"_ts2/c++/tsThread_.h"
#include	"ts2/c++/co_tsThreadKill.h"
#include	"ts2/c++/stdInterval.h"

#define THREAD_TOLL_DULATION		(10*1000*1000)
#define THREAD_UP_DULATION		(10*1000)
#define THREAD_DOWN_DULATION		(120*1000*1000)

#define THREAD_MAX_IDLE_THREADS		2

/* 生存 worker pthread 数(shutdown バリア)。create() の spawn 前に +1、__tsThread_body(void*)
 * の最後(THIS 等の tail relref をすべて終えた後)に -1。tsApplication の shutdown がこれが 0 に
 * なるまで待ってから静的破棄へ進むことで、detached worker の tail relref が静的 refMtx 破棄と
 * 競合してクラッシュするのを防ぐ。外部リンケージ(tsApplication.cpp から extern 参照)。 */
volatile int tsThreadLiveWorkers = 0;

CLASS_TINYSTATE(ts2/c++/tsThread,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


#include	"ts2/c++/sTimer.h"

class TS_THISCLASS : public TS_BASECLASS {
public:
	tsThread_(
		sPtr<tsApplication> parent);
	void inherit(
		sPtr<tsApplication> parent);
	sRptr<tsApplication,tinyState>		parent;

	void ins(sPtr<tinyState> thr);
	/** @brief Register a per-worker setup/cleanup hook. / worker ごとの setup/cleanup フックを登録する。
	 * @details
	 * Each worker thread calls @c setup() on every registered stdThread when it starts,
	 * and @c cleanup() on every registered stdThread when it exits.  Use it for state
	 * that has to exist once per worker thread.
	 *
	 * @warning <b>Both are called while the worker holds the pool's internal mutex</b>,
	 * which is an sThreadMutex and therefore <b>not recursive</b>.  Anything reached from
	 * a hook that takes that mutex again deadlocks the worker on itself — that includes
	 * ins(), limitThreadsNumber() and the rest of this class.  Do not call back into
	 * tsThread from a hook.
	 *
	 * @note Registration is not retroactive.  A worker runs the list as it stands when
	 * that worker starts, so a stdThread registered after workers already exist gets
	 * @c setup() only on workers created later, while @c cleanup() still runs on all of
	 * them at exit.  Register before the pool has grown — at application start — if you
	 * need the pairing to hold.
	 *
	 * ---
	 *
	 * 各 worker スレッドは、起動時に登録済み stdThread すべての @c setup() を、終了時に
	 * すべての @c cleanup() を呼ぶ。worker スレッドごとに 1 つ必要な状態の用意に使う。
	 *
	 * @warning <b>どちらも worker が pool 内部の mutex を保持したまま呼ばれる。</b>
	 * この mutex は sThreadMutex = <b>非再帰</b>なので、フックから同じ mutex を取る API を
	 * 呼ぶと worker が自分自身を待って固まる。ins() や limitThreadsNumber() をはじめ、
	 * 本クラスの API はフックの中から呼んではならない。
	 *
	 * @note 登録は遡及しない。worker は自分が起動した時点のリストを実行するので、worker が
	 * 既に存在する状態で登録した stdThread は、以後に生成された worker でしか @c setup() が
	 * 呼ばれない (@c cleanup() は終了時に全 worker で呼ばれる)。対で成立させたいなら、
	 * pool が増える前 — アプリケーション起動時 — に登録すること。
	 */
	void ins_setup(sPtr<stdThread> thr);
	/** @brief Unregister a hook registered with ins_setup(). / ins_setup() で登録したフックを外す。
	 * @details Same locking contract as ins_setup() — see its @c warning.
	 * / ロックの契約は ins_setup() と同じ。そちらの @c warning を参照。
	 */
	void del_setup(sPtr<stdThread> thr);
	/** @brief Get the current worker thread upper limit. / worker スレッド数の上限値を取得する。
	 * @return Current upper limit / 現在の上限値
	 */
	int limitThreadsNumber();
	/** @brief Set the worker thread upper limit. / worker スレッド数の上限値を設定する。
	 * @details
	 * The pool grows automatically as TS_THREAD bodies block; by default it is
	 * unbounded (@c MAX_INTEGER).  This caps that growth.
	 * Values below @c THREAD_MAX_IDLE_THREADS (2) are clamped to 2.
	 * Lowering the limit below the current target shrinks the target to @a lim.
	 *
	 * The cap applies to the @em target thread count, not to live threads: an
	 * existing worker only exits after finishing its current job, so the live
	 * count may briefly exceed @a lim after a shrink.
	 *
	 * @warning Capping the pool can deadlock.  If every worker blocks inside a
	 * TS_THREAD body and releasing them requires another TS_THREAD to make
	 * progress (blocking I/O, a semaphore wait under THR_CATCH, TS_THREADs
	 * waiting on each other), the unbounded default is what saves you today.
	 * Choosing a limit is the application's responsibility.
	 * To merely bound concurrency, prefer stdLimitSemaphore acquired in a
	 * TS_STATE before entering TS_THREAD -- that holds no worker, so the pool
	 * does not grow in the first place.
	 *
	 * ---
	 *
	 * pool は TS_THREAD がブロックするたび自動的に増える。既定は無制限
	 * (@c MAX_INTEGER)。本 API はその増加に上限を設ける。
	 * @c THREAD_MAX_IDLE_THREADS (2) 未満の値は 2 にクランプされる。
	 * 現在の目標値より小さい上限を設定した場合、目標値も @a lim まで切り下がる。
	 *
	 * 上限は<b>目標値</b>に対するものであり、live thread のハードキャップではない。
	 * 既存 worker は現在の job を終えてから終了するため、縮小直後は一時的に
	 * @a lim を上回ることがある。
	 *
	 * @warning 上限を絞ると deadlock しうる。全 worker が TS_THREAD 内でブロックし、
	 * その解除に別の TS_THREAD の進行を要する構成 (ブロッキング I/O、THR_CATCH 内の
	 * セマフォ待ち、TS_THREAD 同士の待ち合わせ) では、現状は既定の無制限成長が
	 * これを回避している。上限設定はアプリケーション側の責任である。
	 * 単に並列度を絞りたいだけなら、TS_STATE 内で stdLimitSemaphore を get() して
	 * から TS_THREAD に入る方が適切 (worker を占有しないので pool が膨らまない)。
	 *
	 * @param[in] lim New upper limit / 新しい上限値
	 * @see ins_setup() — must not be called from a per-worker hook (that runs under the
	 *      pool mutex) / worker フックの中からは呼べない (pool の mutex 保持下で走るため)
	 */
	void limitThreadsNumber(int lim);
	/** @brief Teardown gate: are we done? — teardown ゲート: 畳み終わったか
	 * @details Called by tsApplication's shutdown loop, under the application mutex.
	 * Returns 1 once this pool has nothing left to do, latching that answer so later
	 * calls keep returning 1.  Returns 0 while the caller should keep yielding.
	 * Not for application use.
	 *
	 * tsApplication の shutdown ループが app-mutex 保持下で呼ぶ。畳み終わっていれば 1 を
	 * 返し、その答えをラッチするので以後も 1 を返し続ける。0 の間は呼び手が譲る。
	 * アプリケーションから呼ぶものではない。 */
	int finish();

private:
protected:
	INTEGER64 readyQueueOldest();
	void create(int num);
	static void *__tsThread_body(void * arg);
	void __tsThread_body();
#if 0	/* realtime (SCHED_FIFO) スレッド対応 廃止 (2026-07-10) */
	static void * __tsThread_realTime(void * arg);
	void __tsThread_realTime(sPtr<stdThreadInfo> target);
#endif
	void runThreads(int num);
	int runThreads();
	void _runThreads_nolock(int num);
	int readysAndRuns();
	void _do_setup();
	void _do_cleanup();

	void setRefio();
	void resRefio();

	int				targetRunThreads;
	int				_limitThreadsNumber;
	int				currentIdleThreads;
	int				currentRunThreads;
	sThreadMutex			mtx;
	sThreadCond			cond;
	sPtr<stdQueue<stdThreadInfo> >	ready;
	sPtr<stdQueue<stdThreadInfo> >	run;
	sPtr<stdQueue<stdThread> >  		setup_list;

	int8_t				thread_stop;
	unsigned			finish_flag:1;

	sTimer				timer;

	unsigned			refio:1;
};

TS_END_IMPLEMENT

#endif




tsThread_::tsThread_(
		sPtr<tsApplication> _parent)
        : tinyState_(_parent),
	  parent(tinyState_::parent)
{
	/* 既定は無制限 = 現行動作 (TS_THREAD のブロックに応じて際限なく増える) の維持。
	 * ★ sObject::operator new はインスタンスをゼロクリアする (sObject.cpp) ので、
	 * ここで明示代入しないと 0 のまま _runThreads_nolock() のクランプが効き、
	 * INI_START の runThreads(THREAD_MAX_IDLE_THREADS) が 0 に潰れて worker が
	 * 1 本も生成されず、アプリ全体が停止する。 */
	_limitThreadsNumber = MAX_INTEGER;
}

void
tsThread_::inherit(
	sPtr<tsApplication> _parent)
{
	this->TS_BASECLASS::inherit(_parent);
}



/*******************************************
	RELATED CLASS AND FUNCTIONS
********************************************/

stdThreadInfo::stdThreadInfo(sPtr<tinyState> target)
{
	state = KF_IDLE;
	this->target = target;
	createTime = stdInterval::now();
}

stdThreadInfo::~stdThreadInfo()
{
	target = thNULL;
	pol = thNULL;
}


void
stdThreadInfo::setId()
{
	id = pthread_self();
	id_enabled = 1;
}

void
stdThreadInfo::resetId()
{
	id_enabled = 0;
}

void
stdThreadInfo::start()
{
sThreadMutexHandle __hdr(m);

	switch ( state ) {
	case KF_IDLE:
		state = KF_THR;
		break;
	case KF_THR:
		break;
	default:
		stdObject::panic("stdThreadInfo start");
	}
}

void
stdThreadInfo::finish()
{
sThreadMutexHandle __hdr(m);

	switch ( state ) {
	case KF_IDLE:
		break;
	case KF_THR:
	case KF_THR_REQUEST:
		state = KF_IDLE;
		break;
	default:
		stdObject::panic("stdThreadInfo dont call return in THR_CATCH");
	}
}


int
stdThreadInfo::kill_begin()
{
int ret;
sThreadMutexHandle __hdr(m);

	ret = state;
	switch ( state ) {
	case KF_IDLE:
	case KF_THR:
		state = KF_THR_CATCH;
		break;
	case KF_THR_REQUEST:
		state = KF_THR_CATCH_REQUEST;
		break;
	default:
		stdObject::panic("stdThreadInfo kill_begin is not nested");
	}
	return ret;
}

void
stdThreadInfo::kill_finish()
{
  	{
	sThreadMutexHandle __hdr(m);

		switch ( state ) {
		case KF_IDLE:
			stdObject::panic("stdThreadInfo thread is not started");
		case KF_THR_CATCH:
			state = KF_THR;
			break;
		case KF_THR_CATCH_REQUEST:
			state = KF_THR_REQUEST;
			break;
		default:
			stdObject::panic("stdThreadInfo kill_begin is not nested");
		}
	}
	if ( pol.is_notNull() ) {
		pol->destroy();
		pol = thNULL;
	}
}


void
stdThreadInfo::kill(int sig)
{
int pol_flag = 0;
	{
	sThreadMutexHandle __hdr(m);

		if ( id_enabled &&
				pthread_equal(id,pthread_self()) == 0 )
			pthread_kill(id,sig);
		switch ( state ) {
		case KF_IDLE:
		case KF_THR_REQUEST:
		case KF_THR_CATCH_REQUEST:
			break;
		case KF_THR:
			state = KF_THR_REQUEST;
			break;
		case KF_THR_CATCH:
			pol_flag = 1;
			state = KF_THR_CATCH_REQUEST;
			break;
		}
	}
	if ( pol_flag )
		pol = thNEW(co_tsThreadKill,(target,sig));
}


int
stdThreadInfo::wait()
{
sThreadMutexHandle __hdr(m);
int ret;
 	for ( ; ; ) {
		ret = signal_flag;
		signal_flag = 0;
		if ( ret )
			return ret;
		c.wait(m);
	}
}

void
stdThreadInfo::signal()
{
sThreadMutexHandle __hdr(m);
	if ( signal_flag )
		return;
	signal_flag = 1;
	c.signal();
}




stdThread::stdThread(sPtr<stdObject> parent)
{
	this->parent = parent;
}
stdThread::~stdThread()
{
	parent = thNULL;
}
void
stdThread::setup()
{
}
void
stdThread::cleanup()
{
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/


void
tsThread_::setRefio()
{
	if ( refio )
		return;
	refio = 1;
	application->fw()->addRefio();
}

void
tsThread_::resRefio()
{
	if ( refio == 0 )
		return;
	refio = 0;
	application->fw()->delRefio();
}


void
tsThread_::ins_setup(sPtr<stdThread> thr)
{
sThreadMutexHandle __hdr(mtx);

	if ( setup_list == thNULL )
		setup_list = (thNEW( stdQueue<stdThread>,()));
	setup_list->ins(MAX_INTEGER64,thr);
}

void
tsThread_::del_setup(sPtr<stdThread> thr)
{	
	sThreadMutexHandle __hdr(mtx);

	if ( setup_list.is_notNull() )
		setup_list->del(thr,0);
}

/* ★ _do_setup() / _do_cleanup() はどちらも __tsThread_body() が mtx を保持したまま
 * 呼ぶ。mtx は非再帰なので、フックの中から mtx を取る API (ins() / limitThreadsNumber()
 * 等) を呼ぶと worker が自分自身を待って固まる。ins_setup() の doxygen 参照。 */
void
tsThread_::_do_setup()
{
sPtr<stdQueueElement<stdThread> > elp;
	if ( setup_list.is_notNull() )
		for ( elp = setup_list->head ; elp.is_notNull() ; elp = elp->next )
			elp->data->setup();
}

void
tsThread_::_do_cleanup()
{
sPtr<stdQueueElement<stdThread> > elp;
	if ( setup_list.is_notNull() )
		for ( elp = setup_list->head ; elp.is_notNull() ; elp = elp->next )
			elp->data->cleanup();
}


void
tsThread_::ins(sPtr<tinyState> inp)
{
	{
	sThreadMutexHandle __hdr(mtx);
		ready->ins(MAX_INTEGER64,thNEW(stdThreadInfo,(inp)));
		cond.signal();
		setRefio();
	}
	/* 積んだことをプールの状態機械に知らせる。mtx の外で呼ぶこと (wakeup は
	 * 自分の eventHandler に入る)。
	 * 2025-01-04 に一度ここから呼び出し側へ移されたのは、ins() が tinyState の
	 * lm を保持したまま呼ばれており、wakeup() が他オブジェクトへ届くと
	 * lm -> fwIO::mu の順序を作ったため。ins() 自体を lm の外へ出したので、
	 * その理由は無くなった。 */
	wakeup();
}

void
tsThread_::create(int num)
{
pthread_attr_t 		phy_attr;
pthread_t		phy_thread;
int i;
	i = num;
	for ( ; i ; i -- ) {
		pthread_attr_init(&phy_attr);
		pthread_attr_setdetachstate(&phy_attr,PTHREAD_CREATE_DETACHED);
		__sync_fetch_and_add(&tsThreadLiveWorkers,1);	/* spawn 前に +1(存在するのに未カウントの窓を作らない) */
		pthread_create(&phy_thread,&phy_attr,__tsThread_body,(void*)this);
		pthread_attr_destroy(&phy_attr);
	}
	currentRunThreads += num;
}

/* targetRunThreads への唯一の書き込み口。mtx 保持が前提。
 * 上限クランプをここ 1 箇所に集約することで invariant
 *	targetRunThreads <= _limitThreadsNumber
 * が常に成立し、getter 側や呼び出し側でのクランプが一切不要になる。
 * invariant が破れうるのは上限を下げた瞬間だけで、それは
 * limitThreadsNumber(int) が現在値を再適用することで解消する。 */
void
tsThread_::_runThreads_nolock(int num)
{
	if ( num > _limitThreadsNumber )
		num = _limitThreadsNumber;
	targetRunThreads = num;
	if ( currentRunThreads < targetRunThreads )
		create(targetRunThreads - currentRunThreads);
	if ( currentRunThreads > targetRunThreads )
		cond.broadcast();
}

void
tsThread_::runThreads(int num)
{
sThreadMutexHandle __hdr(mtx);

	_runThreads_nolock(num);
}


int
tsThread_::runThreads()
{
sThreadMutexHandle __hdr(mtx);
	return targetRunThreads;
}


/* teardown ゲートの本体。待っているのは
 *
 *	ready->count == 0  &&  run->count == 0  &&  is_stable()
 *
 * の 3 つが *同時に* 成立することで、3 つはそれぞれ「投入者がどこに居るか」を 1 つずつ
 * 受け持っている: worker 上の連鎖 = run->count / main 上 = この関数と同一スレッドなので
 * 直列 / gc 上 = is_stable。投入者は必ず tinyState という前提なので、匿名スレッドからの
 * 飛び込みは無い。
 *
 * 呼び手 (tsApplication) は is_stable の待ちを stdObject::wait_stable() で mtx の外に
 * 出して済ませてから、ここで 3 条件の同時性を確かめる。成立したらラッチして wakeup し、
 * FIN_STABLE_WAIT を起こす。ラッチするのは、以後 is_stable が再び false に振れても
 * 判定を蒸し返さないため。 */
int
tsThread_::finish()
{
	if ( C_TEST(state(),C_ZOM) )
		return 1;
	if ( !C_TEST(state(),C_FIN) )
		return 0;		/* まだ FIN に入っていない */
	if ( finish_flag )
		return 1;		/* ラッチ済み */
	if ( ready->count == 0 && run->count == 0 && stdObject::is_stable() ) {
		finish_flag = 1;
		wakeup();
		return 1;
	}
	return 0;
}

int
tsThread_::limitThreadsNumber()
{
sThreadMutexHandle __hdr(mtx);
	return _limitThreadsNumber;
}

void
tsThread_::limitThreadsNumber(int lim)
{
	if ( lim < THREAD_MAX_IDLE_THREADS )
		lim = THREAD_MAX_IDLE_THREADS;
	/* mtx は非再帰 (sThreadMutex) なので、握ったまま runThreads() を呼んではならない。
	 * _runThreads_nolock() を直接呼ぶことで、上限の更新と目標値の切り下げも
	 * アトミックになる。 */
sThreadMutexHandle __hdr(mtx);
	_limitThreadsNumber = lim;
	_runThreads_nolock(targetRunThreads);	/* 再適用 → 上限超過分が切り下がる */
}


INTEGER64
tsThread_::readyQueueOldest()
{
INTEGER64 ret;
sThreadMutexHandle __hdr(mtx);

	ret = MAX_INTEGER64;
	ready->check([&ret](sPtr<stdThreadInfo> d) {
		if ( d->createTime < ret )
			ret = d->createTime;
		return (int)1;
	});
	return ret;
}

int
tsThread_::readysAndRuns()
{
sThreadMutexHandle __hdr(mtx);
	return ready->count + run->count;
}

#if 0	/* realtime (SCHED_FIFO) スレッド対応 廃止 (2026-07-10)。Windows 移植の障害 + 利用実績なし。 */
struct realTimeArg {
public:
	tsThread_ * THIS;
	sPtr<stdThreadInfo> * target;
};

void*
tsThread_::__tsThread_realTime(void * arg)
{
struct realTimeArg * _arg;
	_arg = (struct realTimeArg*)arg;
sPtr<tsThread_> THIS;

	_arg->THIS->__tsThread_realTime(*_arg->target);
	return 0;
}

void
tsThread_::__tsThread_realTime(sPtr<stdThreadInfo> target)
{
	{
	sThreadMutexHandle __hdr(mtx);

		_do_setup();
		target->setId();
		mtx.unlock();

		target->target->eventHandler(
			thNEW(stdEvent,(TSE_THREAD,ifThis,(INTEGER64)0)),
			target);

		mtx.lock();
		target->resetId();

		_do_cleanup();
	}
}
#endif

void *
tsThread_::__tsThread_body(void * arg)
{
	{
		sPtr<tsThread_> THIS = sPtr<tsThread_>((tsThread_*)arg);
		THIS->__tsThread_body();
	}	/* ★ ここで THIS のデストラクタ = tsThread_ への最後の relref が走る(refMtx 生存中)。
	 * メンバ本体内の target/prev_target の relref もこの内側ブロック内で完了済み。 */
	__sync_fetch_and_sub(&tsThreadLiveWorkers,1);	/* ★ 全 tail relref 完了後・絶対最後。以降は return のみ(静的状態に触れない) */
	return 0;
}



void
tsThread_::__tsThread_body()
{
sPtr<stdThreadInfo> target;
sPtr<stdThreadInfo> prev_target;
int crt;


	{
	sThreadMutexHandle __hdr(mtx);

		_do_setup();
		for ( ; ; ) {
			if ( targetRunThreads < currentRunThreads )
				break;
			prev_target = target;
			target = ready->del();
			if ( target == thNULL ) {
				if ( thread_stop )
					break;
				currentIdleThreads ++;
			int ret;
				ret = cond.timedwait(mtx,THREAD_DOWN_DULATION);
				currentIdleThreads --;
				if ( ret == 0 )
					continue;
				target = ready->del();
				if ( target == thNULL )
					break;
			}
			target->runStartTime = stdInterval::now();
			run->ins(MAX_INTEGER64,target);
		/* realtime (SCHED_FIFO) 専用スレッド経路は廃止 (2026-07-10): 通常ディスパッチに一本化。
		 * Windows 移植の障害 + 利用実績なし。旧経路は以下 #if 0 ブロック内に温存。 */
#if 0
		int rt_pri = target->target->realtime();
			if ( rt_pri ) {
			realTimeArg arg;
			pthread_attr_t 		phy_attr;
			pthread_t		phy_thread;
		       	struct sched_param param;
			int pri;
			int ret;
				arg.THIS = this;
				arg.target = &target;
				ret = pthread_attr_init(&phy_attr);
 				if ( ret < 0 ) {
					perror("pthread_attr_init");
					stdObject::panic("REALTIME ERROR\n");
				}
			        /* Set scheduler policy and priority of pthread */
			        ret = pthread_attr_setschedpolicy(&phy_attr, SCHED_FIFO);
 				if ( ret < 0 ) {
					perror("pthread_attr_setchedpolicy");
					stdObject::panic("REALTIME ERROR\n");
				}
				param.sched_priority = rt_pri;
			        ret = pthread_attr_setschedparam(&phy_attr, &param);
 				if ( ret < 0 ) {
					perror("pthread_attr_setchedparam");
					stdObject::panic("REALTIME ERROR\n");
				}
			        /* Use scheduling parameters of attr */
			        ret = pthread_attr_setinheritsched(&phy_attr, PTHREAD_EXPLICIT_SCHED);
 				if ( ret < 0 ) {
					perror("pthread_attr_setinheritsched");
					stdObject::panic("REALTIME ERROR\n");
				}
				mtx.unlock();
				ret = pthread_create(&phy_thread,&phy_attr,__tsThread_realTime,(void*)&arg);
 				if ( ret < 0 ) {
					perror("pthread_create");
					stdObject::panic("REALTIME ERROR\n");
				}
				for ( ; ; ) {
				int status;
					if ( pthread_join(phy_thread,NULL) < 0 ) {
						if ( errno == EINTR )
							continue;
						stdObject::panic("WHY");
					}
					break;
				}
				pthread_attr_destroy(&phy_attr);
				mtx.lock();
			}
			else
#endif
			{
				target->setId();
				mtx.unlock();
//::printf("THEAD >> %s\n",target->target->getClass());
				target->target->eventHandler(
					thNEW(stdEvent,(TSE_THREAD,ifThis,(INTEGER64)0)),
					target);
//::printf("THEAD    %s <<\n",target->target->getClass());

				mtx.lock();
				target->resetId();
			}

			run->del([target](sPtr<stdThreadInfo> t){
				if ( t == target )
					return 1;
				return 0;
			});
			if ( run->count == 0 && ready->count == 0 ) {
				resRefio();
				mtx.unlock();
				wakeup();
				mtx.lock();
			}
		}
	finish:
		_do_cleanup();

		currentRunThreads --;
		crt = currentRunThreads;
	}

	if ( crt == 0 )
		wakeup();
	return;
}

/*******************************************
	STATE MACHINE
********************************************/


TS_STATE(INI_START)
{
	ready = thNEW(stdQueue<stdThreadInfo>,());
	run = thNEW(stdQueue<stdThreadInfo>,());
	runThreads(THREAD_MAX_IDLE_THREADS);
	return rDO|ACT_START;
}


TS_STATE(ACT_TINYSTATE_START)
{
INTEGER64 age;
	if ( currentIdleThreads > THREAD_MAX_IDLE_THREADS )
		return rDO|ACT_DOWN;
	age = readyQueueOldest();
	if ( age == MAX_INTEGER64 )
		return ACT_START;
	age = stdInterval::now() - age;
	if ( age > THREAD_UP_DULATION )
		runThreads(runThreads()+1);
	timer.start(ifThis,THREAD_UP_DULATION);
	return rDO|ACT_WAIT_2;
}
TS_STATE(ACT_WAIT_2)
{
	if ( is_destroyed() )
		return rDO|FIN_START;
	if ( timer.is_expire(ifThis) )
		return rDO|ACT_START;
	return 0;
}

TS_STATE(ACT_DOWN)
{
	timer.start(ifThis,THREAD_DOWN_DULATION);
	return rDO|ACT_DOWN_WAIT;
}
TS_STATE(ACT_DOWN_WAIT)
{
INTEGER64 age;
	if ( is_destroyed() )
		return rDO|FIN_START;
	if ( currentIdleThreads == 0 )
		return rDO|ACT_START;
	if ( readysAndRuns() == 0 ) {
		timer.stop(ifThis);
		return ACT_START;
	}
	if ( timer.is_expire(ifThis) ) {
		if ( currentIdleThreads > THREAD_MAX_IDLE_THREADS ) {
			runThreads(runThreads()-1);
			return rDO|ACT_DOWN;
		}
		return rDO|ACT_START;
	}
	return 0;
}


/* Teardown must not free the worker pool while a refEvent (auto-teardown) can still
   fire and re-schedule a C_THR state onto it (that ins()es into `ready` and would
   NULL-deref once we drop it).  So first drain to a point where the pool is empty
   AND the GC is stable (no pending delete / refEvent); only then spin the workers
   down and free.  A refEvent arriving mid-drain simply re-fills ready/run or
   unsettles the GC, so the condition fails and we keep waiting until it converges.

   The three conditions are checked together by finish(), which tsApplication calls;
   the waiting for the is_stable() half happens outside the pool mutex, asleep in
   stdObject::wait_stable().  This state machine only waits for that latch.  It used
   to poll is_stable() on a 1ms timer, which was its own worst enemy: the timer's
   deliveries kept unsettling the very condition being polled. */
TS_STATE(FIN_START)
{
	return rDO|FIN_STABLE_WAIT;
}
/* 判定は自分ではせず、tsApplication 側の finish() がラッチするのを待つだけ。
 *
 * ここで is_stable() を 1ms タイマでポーリングしていた頃は、そのタイマ配送が生む
 * 配送オブジェクトの解放が is_stable() を false に戻す自己競合になっていて、
 * teardown が数百 ms〜秒単位で座り込んだ (30 回中 5 回が >300ms・最大 2.6 秒)。
 * 間隔を伸ばすと悪化する (10ms で中央 15.9 秒) のがその証拠で、待つ機構そのものが
 * 待ち条件を壊していた。今は待ちを stdObject::wait_stable() の眠りに寄せてある。 */
TS_STATE(FIN_STABLE_WAIT)
{
	if ( finish_flag )
		return rDO|FIN_DRAINED;
	return 0;
}
TS_STATE(FIN_DRAINED)
{
	runThreads(0);
	timer.start(ifThis,1000*1000);
	return rDO|FIN_WAIT;
}
TS_STATE(FIN_WAIT)
{

	if ( currentRunThreads ) {
		if ( timer.is_expire(ifThis) )
			timer.start(ifThis,1000*1000);
		return 0;
	}
	timer.stop(ifThis);
	ready = thNULL;
	run = thNULL;
	setup_list = thNULL;
	return rDO|FIN_TINYSTATE_START;
}
