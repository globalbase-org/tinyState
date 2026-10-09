

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
	/* ★ pthread_mutex_lock の失敗は *正の errno*。`< 0` で見ていたので、
	 * 失敗したのに count++ / id=self へ落ちて「持っていないのに持った」
	 * ことになっていた。 */
	if ( ret != 0 )
		return ret;
	count ++;
	id = pthread_self();
	return ret;
}

int 
sThreadMutexRecursive::unlock()
{
int ret;
	if ( count <= 0 )
		stdObject::panic("unlock");
	/* count を先に減らすのは意図どおり (counter() は mutex 下で読むので、
	 * 最後のレベルを返した後に減らすと他スレッドが古い値を読む)。
	 * ★ ただし旧コードは戻り値を捨てていたので、持ち主でないスレッドの
	 * unlock が EPERM で静かに失敗したまま count だけ狂った。
	 * 返せなかったら count を戻し、エラーを呼び手へ渡す。 */
	count --;
	ret = sThreadMutex::unlock();
	if ( ret != 0 )
		count ++;
	return ret;
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
	/* ★ trylock の失敗は *正の errno*。旧コードは `ret < 0` と errno を見て
	 * いたので他スレッド保持中を検出できず、しかも**持っていない mutex を
	 * unlock して**いた。基底 (sThreadMutex) と同じ意味に揃える:
	 * 1 = ロック中 / 0 = 空き / -1 = エラー。 */
	ret = sThreadMutex::trylock();
	if ( ret == EBUSY )
		return 1;
	if ( ret != 0 )
		return -1;
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
