

#include	"ts2/c++/stdLimitSemaphore.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadMutexRecursive.h"

stdLimitSemaphore::stdLimitSemaphore(int lim)
{
	v_limit = lim;
	this->wait = thNEW( stdQueue<tinyState>,());
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
	this->wait->ins(MAX_INTEGER64,me);
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
