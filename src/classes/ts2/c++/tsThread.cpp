
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
	void ins_setup(sPtr<stdThread> thr);
	void del_setup(sPtr<stdThread> thr);

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
	int readysAndRuns();
	void _do_setup();
	void _do_cleanup();

	void setRefio();
	void resRefio();

	int				targetRunThreads;
	int				currentIdleThreads;
	int				currentRunThreads;
	sThreadMutex			mtx;
	sThreadCond			cond;
	sPtr<stdQueue<stdThreadInfo> >	ready;
	sPtr<stdQueue<stdThreadInfo> >	run;
	sPtr<stdQueue<stdThread> >  		setup_list;

	int8_t				thread_stop;

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
	//	wakeup();
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

void
tsThread_::runThreads(int num)
{
sThreadMutexHandle __hdr(mtx);

	targetRunThreads = num;
	if ( currentRunThreads < targetRunThreads )
		create(targetRunThreads - currentRunThreads);
	if ( currentRunThreads > targetRunThreads )
		cond.broadcast();
}


int
tsThread_::runThreads()
{
sThreadMutexHandle __hdr(mtx);
	return targetRunThreads;
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


TS_STATE(FIN_START)
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
