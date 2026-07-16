

#include	"ts2/c++/stdMutex.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadMutexRecursive.h"


stdMutex::stdMutex()
{
	count = 0;
	this->wait = thNEW( stdQueue<tinyState>,());
}


stdMutex::~stdMutex()
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
stdMutex::read_lock()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		throw sException(0,EX_ERROR);
	if ( this->count >= 0 ) {
		this->count ++;
		return;				/* acquired read lock */
	}
	/* writer holds (count == -1) -> queue caller and yield via sException.
	 * is_ready: ready to retry once release() has dequeued this caller. */
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
stdMutex::write_lock()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		throw sException(0,EX_ERROR);
	if ( this->count == 0 ) {
		this->count = -1;
		return;				/* acquired write lock */
	}
	/* readers or a writer hold -> queue caller and yield via sException. */
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
stdMutex::release()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
sPtr<tinyState>  obj;
sPtr<stdQueue<tinyState> >  que;
	if ( this->wait == thNULL )
		return;
	if ( this->count == 0 )
		return;
	if ( this->count < 0 )
		this->count = 0;		/* writer released */
	else {
		this->count --;			/* one reader released */
		if ( this->count )
			return;			/* readers still hold -> keep waiters queued */
	}
	/* lock is now free -> wake ALL waiters so queued readers can re-acquire
	 * concurrently (a queued writer competes and re-queues itself).
	 * Swap in a fresh queue so the woken callers' is_ready predicate sees
	 * themselves de-queued (= ready to retry). */
	que = this->wait;
	this->wait = thNEW( stdQueue<tinyState>,());
	for ( ; ; ) {
		obj = que->del();
		if ( obj == thNULL )
			break;
		obj->wakeup();
	}
}
