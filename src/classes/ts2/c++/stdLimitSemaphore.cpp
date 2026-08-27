

#include	"ts2/c++/stdLimitSemaphore.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadMutexRecursive.h"

stdLimitSemaphore::stdLimitSemaphore(int lim)
{
	v_limit = lim;
	this->wait = thNEW( stdQueue<tinyState>,());
	/* 同じキーで ins したとき、既定 (insNeq=0) は「同キーの前」に入る = 後から来た待ち手が
	 * 先に待っていた側を追い越す。enablePriority を立てると priority() の既定値 10000 が
	 * 全員同じキーになるので、これを立てないと待ち行列が LIFO になり先着が飢える。
	 * insNeq=1 なら「同キーの後ろ」に入るので、優先度が違えば優先度順・同じなら先着順。
	 * enablePriority が偽のときは key=MAX_INTEGER64 の末尾追加経路に入るので参照されない。 */
	this->wait->insNeq = 1;
}


stdLimitSemaphore::~stdLimitSemaphore()
{
sPtr<tinyState>  obj;
sPtr<stdQueue<tinyState> >  q;
	q = this->wait;
	this->wait = thNULL;
	for ( ; ; ) {
		obj = q->del();
		if ( obj == thNULL )
			break;
		obj->wakeup();
	}
}


void
stdLimitSemaphore::get()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		throw sException(0,EX_ERROR);
	if ( count < v_limit ) {
		count ++;
		return;
	}
	if ( this->wait->check(me,0).is_notNull() )
		throw sException([this](sPtr<tinyState> caller) {
			if ( this->wait->check(caller,0).is_notNull() )
				return 0;
			return 1;
		});
	if ( enablePriority )
		this->wait->ins(me->priority(thNULL),me);
	else	this->wait->ins(MAX_INTEGER64,me);
	throw sException([this](sPtr<tinyState> caller) {
		if ( this->wait->check(caller,0).is_notNull() )
			return 0;
		return 1;
	});
}


void
stdLimitSemaphore::release()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
sPtr<tinyState>  obj;
	if ( this->wait == thNULL )
		return;
	if ( count <= 0 )
		stdObject::panic("cannot release");
	count --;
	if ( count >= v_limit )
		return;
	obj = this->wait->del();
	if ( obj.is_notNull() )
		obj->wakeup();
}

int
stdLimitSemaphore::limit()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	return v_limit;
}

void
stdLimitSemaphore::limit(int lim)
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
sPtr<tinyState>  obj;
int _lim;
	if ( lim < 1 )
		return;
	if ( v_limit == lim )
		return;
	if ( v_limit > lim ) {
		v_limit = lim;
		return;
	}
	_lim = v_limit;
	v_limit = lim;
	for ( ; _lim < lim ; _lim ++ ) {
		obj = this->wait->del();
		if ( obj == thNULL )
			break;
		obj->wakeup();
	}
}
