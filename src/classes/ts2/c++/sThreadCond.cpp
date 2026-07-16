

#include	"ts2/c++/sThreadCond.h"
#include	"ts2/c++/sCallSection.h"

sThreadCond::sThreadCond()
{
	pthread_cond_init(&cond,0);
}

sThreadCond::~sThreadCond()
{
	pthread_cond_destroy(&cond);
}


int
sThreadCond::signal()
{
	return pthread_cond_signal(&cond);
}

int
sThreadCond::broadcast()
{
	return pthread_cond_broadcast(&cond);
}

int
sThreadCond::wait(sThreadMutex & m)
{
	return m.waitWrap([this,&m]() {
		return pthread_cond_wait(&cond,&m.m);
	});
}

int
sThreadCond::timedwait(sThreadMutex & m,INTEGER64 tim)
{
struct timespec timeout;
struct timeval now;
int ret;
	gettimeofday(&now,0);
	timeout.tv_sec = now.tv_sec + tim/(1000*1000);
	timeout.tv_nsec = (now.tv_usec + tim % (1000*1000) )* 1000;
	for ( ; ; ) {
	INTEGER64 info;
		ret = m.waitWrap([this,&m,&timeout]() {
			return pthread_cond_timedwait(&cond,&m.m,&timeout);
		});
		switch ( ret ) {
		case ETIMEDOUT:
			return -1;
		case EINTR:
			continue;
		default:
			return 0;
		}
		break;
	}
	return 0;
}

