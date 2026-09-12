

#include	"ts2/c++/stdSemaphore.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadMutexRecursive.h"

stdSemaphore::stdSemaphore(int _count)
{
	this->count = _count;
	this->wait = thNEW( stdQueue<tinyState>,());
	/* 同じキーで ins したとき、stdQueue の既定 (insNeq=0) は「同キーの前」に入る =
	 * 後から来た待ち手が先に待っていた側を追い越す。enablePriority を立てると
	 * priority() の既定値 10000 が全員同じキーになるので、これを立てないと待ち行列が
	 * LIFO になり先着が飢える。insNeq=1 なら「同キーの後ろ」に入るので、優先度が違えば
	 * 優先度順・同じなら先着順。enablePriority が偽のときは key=MAX_INTEGER64 の
	 * 末尾追加経路に入るので参照されない。
	 * 2026-09-10: それまで本クラスだけ設定漏れで LIFO だった。同名フラグで
	 * stdLimitSemaphore と挙動が割れていたので 1 に揃えた。変えたい場合は insNeq(0)。 */
	this->wait->insNeq = 1;
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

void
stdSemaphore::insNeq(int v)
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		return;
	this->wait->insNeq = ( v != 0 );
}

int
stdSemaphore::insNeq()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		return 1;
	return this->wait->insNeq;
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
