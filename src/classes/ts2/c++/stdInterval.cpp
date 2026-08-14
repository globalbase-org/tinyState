

#include 	<sys/time.h>
#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/stdFrameWork.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/sThreadMutexHandle.h"

sThreadMutex
stdInterval::m;
INTEGER64
stdInterval::lastAccessTime;

int
stdInterval::wait(sPtr<tinyState>  THIS,INTEGER64 tm,int type)
{
sPtr<stdFrameWork> fw;
	fw = THIS->application->fw();
	return fw->wait(THIS,tm,type);
}

int
stdInterval::detach(sPtr<tinyState>  THIS)
{
sPtr<stdFrameWork> fw;
	fw = THIS->application->fw();
	return fw->detach(THIS);
}



INTEGER64
stdInterval::now()
{
struct timeval tm;
INTEGER64 ret;
	gettimeofday(&tm,0);
	ret = ((INTEGER64)tm.tv_usec) + ((INTEGER64)tm.tv_sec)*1000000;
	{
	sThreadMutexHandle __hdr(m);
		if ( lastAccessTime < ret ) {
			lastAccessTime = ret;
			return ret;
		}
		return lastAccessTime;
	}
}
