

#include	"ts2/c++/stdSemaphore.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadMutexRecursive.h"

stdSemaphore::stdSemaphore(int _count)
{
	this->count = _count;
	this->wait = thNEW( stdQueue<tinyState>,());
}


stdSemaphore::~stdSemaphore()
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
stdSemaphore::get()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		throw sException(0,EX_ERROR);
	if ( this->count > 0 ) {
		this->count --;
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
stdSemaphore::release()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
sPtr<tinyState>  obj;
	if ( this->wait == thNULL )
		return;
	this->count ++;
	obj = this->wait->del();
	if ( obj.is_notNull() )
		obj->wakeup();
}

int
stdSemaphore::waitCount()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( wait == thNULL )
		return -1;
	return wait->count;
}
