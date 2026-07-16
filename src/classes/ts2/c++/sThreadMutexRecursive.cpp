

#include	"ts2/c++/sThreadMutexRecursive.h"
#include	"ts2/c++/stdObject.h"

sThreadMutexRecursive::sThreadMutexRecursive()
	: sThreadMutex(1)
{
pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&m,&attr);
	count = 0;
}

sThreadMutexRecursive::~sThreadMutexRecursive()
{
	if ( count )
		stdObject::panic("mutex is used");
}


int
sThreadMutexRecursive::lock()
{
int ret;
	ret = sThreadMutex::lock();
	if ( ret < 0 )
		return ret;
	count ++;
	id = pthread_self();
	return ret;
}

int 
sThreadMutexRecursive::unlock()
{
	if ( count <= 0 )
		stdObject::panic("unlock");
	count --;
	return sThreadMutex::unlock();
}

int
sThreadMutexRecursive::trylock()
{
int ret;
	ret = sThreadMutex::trylock();
	if ( ret == 0 ) {
		count ++;
		id = pthread_self();
	}
	return ret;
}

int
sThreadMutexRecursive::is_locked()
{
int ret;
	ret = sThreadMutex::trylock();
	if ( ret < 0 ) {
		if ( errno == EBUSY )
			return 0;
		return -1;
	}
	if ( count )
		ret = 1;
	else	ret = 0;
	sThreadMutex::unlock();
	return ret;
}

int
sThreadMutexRecursive::counter()
{
int ret;
	sThreadMutex::lock();
	ret = count;
	sThreadMutex::unlock();
	return ret;
}


int 
sThreadMutexRecursive::waitWrap(std::function<int()> func)
{
int _count;
int ret;
	sThreadMutex::lock();
	if ( count == 0 )
		stdObject::panic("non locked cond");
	_count = count;
	count = 0;
	ret = func();
	count = _count;
	id = pthread_self();
	sThreadMutex::unlock();
	return ret;
}
