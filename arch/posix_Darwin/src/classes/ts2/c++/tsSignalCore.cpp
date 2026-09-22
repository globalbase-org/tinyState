
#include	<unistd.h>
#include	<signal.h>
#include	<errno.h>
#include	<fcntl.h>
#include	"_ts2/c++/tsSignalCore_.h"



CLASS_TINYSTATE(tsSignalCore,tinyState)

#if 0
TS_BEGIN_IMPLEMENT

#include	"ts2/c++/sImmortal.h"

class TS_THISCLASS : public TS_BASECLASS {
public:
	tsSignalCore_(
		sPtr<tinyState>  parent,
		int sig);
	void inherit(
		sPtr<tinyState>  parent,
		int sig);

	static tsSignalCore * 	search_signal(int sig);
	void			ins_front(sPtr<tsSignal>  obj);
	void			del_front(sPtr<tsSignal>  obj);

	tsSignalCore * 		next;
	sPtr<tsSignalCore>	_next;
	int			sig;

	int			pipe_write;
	int			sig_count;
private:
	int			pipe_read;

	/* ACT_PREV でハンドラを載せる直前の disposition。FIN_START はこれを書き戻す。
	   以前は無条件に SIG_DFL を書いており、アプリが自分で張っていたハンドラを
	   tsSignal 1 本の生成→破棄で踏み潰していた。tsApplication が起動時に張る
	   SIGPIPE の no-op ハンドラがその犠牲になり、tsSignal(SIGPIPE) を
	   畳んだ後の destroy() が出す THR_KILL(SIGPIPE) が SIG_DFL に当たって
	   プロセスを殺していた (rc=141)。 */
	struct sigaction	prev_act;
	int			prev_act_valid;

	sPtr<fwIO> 			io;

	static void		signal_handler_active(int sig);
	static void		signal_handler_empty(int sig);

	sPtr<stdQueue<tsSignal> > 	front_list;

	static tsSignalCore * 	signal_list;
	/* ★ 不滅 (デストラクタを登録しない)。このデストラクタこそが relref() を呼び、
	 * 破棄済みの stdObject::refMtx[] を叩いて 0 番地へ飛んでいた経路そのもの。
	 * abort() では FIN が走らないので、FIN 側の切り離しでは塞げない。sImmortal.h 参照。 */
	static sImmortal<sPtr<tsSignalCore> >	_signal_list;
	void			ins_signal();
	void			del_signal();
};
TS_END_IMPLEMENT
#endif


/*******************************************
	PUBLIC FUNCTIONS
********************************************/

tsSignalCore_::tsSignalCore_(
	sPtr<tinyState>  parent,
	int sig)
	:
	tinyState_(parent->application)
{
	this->sig = sig;
	this->prev_act_valid = 0;
}

void
tsSignalCore_::inherit(
	sPtr<tinyState>  parent,
	int sig)
{
	this->TS_BASECLASS::inherit(parent);
}

#define R	(0)
#define W	(1)


tsSignalCore *
tsSignalCore_::signal_list;
sImmortal<sPtr<tsSignalCore> >
tsSignalCore_::_signal_list;

tsSignalCore *
tsSignalCore_::search_signal(int sig)
{
tsSignalCore *  ret;
	for ( ret = tsSignalCore_::signal_list;
			ret ;
			ret = ret->next )
		if ( ret->sig == sig )
			return ret;
	return 0;
}

void
tsSignalCore_::ins_signal()
{
	this->next = tsSignalCore_::signal_list;
	tsSignalCore_::signal_list = ifThis.__get();
	this->_next = *tsSignalCore_::_signal_list;
	*tsSignalCore_::_signal_list = ifThis;
}

void
tsSignalCore_::del_signal()
{
tsSignalCore ** ip;
	for ( ip = &tsSignalCore_::signal_list;
			(*ip) && *ip != ifThis.__get();
			ip = &(*ip)->next );
	if ( *ip == 0 )
		return;
	*ip = this->next;
}

void
tsSignalCore_::signal_handler_active(int sig)
{
tsSignalCore *  target;
char ch;
int ret;
struct sigaction act;


	target = search_signal(sig);
	if ( target ) {
		ch = 0;
		target->sig_count ++;
		for ( ; ; ) {
			ret = ::write(target->pipe_write,&ch,1);
			if ( ret == 1 || ret < 0 )
				break;
		}
	}
	memset(&act,0,sizeof(act));
	act.sa_flags = 0;
	act.sa_handler = signal_handler_active;
	sigemptyset(&act.sa_mask);
	::sigaction(sig,&act,0);

//	::signal(sig,signal_handler_active);
}

void
tsSignalCore_::signal_handler_empty(int sig)
{
	::signal(sig,signal_handler_empty);
}

void
tsSignalCore_::ins_front(sPtr<tsSignal>  obj)
{
	this->front_list->ins(0,obj);
	if ( this->front_list->count == 1 )
		wakeup();
}

void
tsSignalCore_::del_front(sPtr<tsSignal>  obj)
{
	this->front_list->del(obj,0);
	if ( this->front_list->count == 0 )
		this->wakeup();
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
int pp[2];
	this->io =  sPtr<fwIO>::d_cast
		(this->application->fw());
	this->front_list = thNEW( stdQueue<tsSignal>,());
	soPIPE(pp);
	this->pipe_write = pp[W];
	this->pipe_read = pp[R];
	fcntl(this->pipe_read,F_SETFL,O_NONBLOCK);

	this->ins_signal();

	return rDO|ACT_PREV;
}
TS_STATE(ACT_PREV)
{
struct sigaction act;
	memset(&act,0,sizeof(act));
	act.sa_flags = 0;
	act.sa_handler = signal_handler_active;
	sigemptyset(&act.sa_mask);
	/* 旧 disposition を控える。FIN_SLEEP -> ACT_PREV の再活性化でもここを通るので、
	   そのたびに取り直す (直前に FIN_START が書き戻した値を拾うだけで整合する)。 */
	::sigaction(sig,&act,&this->prev_act);
	this->prev_act_valid = 1;
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
	this->io->read(ifThis,this->pipe_read);
	return ACT_START_RET;
}
TS_STATE(ACT_START_RET)
{
char buffer[10];
sPtr<stdQueue<tsSignal> >  qs;
sPtr<stdQueueElement<tsSignal> > el;
int ret;
	if ( this->front_list->count == 0 )
		return rDO|FIN_START;
	if ( ev->type != TSE_RETURN )
		return 0;
	ret = ::read(this->pipe_read,buffer,10);
	if ( ret >= 0 )
		this->sig_count -= ret;
	else if ( ret < 0 ) {
		if ( errno == EAGAIN ||
				errno == EWOULDBLOCK )
			return rDO|ACT_FINISH;
	}
	qs = thNEW( stdQueue<tsSignal>,(this->front_list));
	for ( el = qs->head ; el.is_notNull() ; el = el->next )
		el->data->send_signal();
	return rDO|ACT_FINISH;
}
TS_STATE(ACT_FINISH)
{
	if ( this->front_list->count == 0 )
		return rDO|FIN_START;
	return rDO|ACT_START;
}

TS_STATE(FIN_START)
{
	io->detach(ifThis);
struct sigaction act;
	signal(sig,signal_handler_empty);

	/* Restore the default disposition -- and nothing more.  This used to add
	   SA_NOCLDWAIT, which only means anything for SIGCHLD: it tells the kernel
	   to reap children itself rather than leave them as zombies.  Between the
	   last tsSignal(SIGCHLD) going away and the next one being built, that flag
	   stayed armed, so a child that exited in the gap was collected before
	   ts2System's waitpid could look at it.  waitpid then returned ECHILD and
	   the status it never read defaulted to 0, i.e. a failed child reported
	   success.  Short-lived children hit that gap nearly every time.

	   A generic signal core has no business setting a SIGCHLD-specific policy,
	   and whoever spawns a child it will never wait for is the one who knows
	   the child needs reaping.  Cygwin has run without the flag all along
	   (its signal.h has no SA_NOCLDWAIT, so the local #define made it a no-op)
	   and shows none of this. */
	if ( this->prev_act_valid ) {
		/* ACT_PREV で控えた disposition へ戻す。SIG_DFL 固定にすると、アプリが
		   起動時に張ったハンドラ (tsApplication の SIGPIPE no-op 対策)
		   まで剥がれ、以降の destroy() の THR_KILL(SIGPIPE) がプロセスを殺す。 */
		sigaction(sig,&this->prev_act,0);
	}
	else {
		memset(&act,0,sizeof(act));
		act.sa_flags = 0;
		act.sa_handler = SIG_DFL;
		sigaction(sig,&act,0);
	}
	return rDO|FIN_SLEEP;
}
TS_STATE(FIN_SLEEP)
{
	if ( front_list->count == 0 )
		return 0;
int ret;
char buffer[10];
	for ( ; ; ) {
		ret = ::read(this->pipe_read,buffer,10);
		if ( ret <= 0 )
			break;
	}
	sig_count = 0;
	return rDO|ACT_PREV;
}
