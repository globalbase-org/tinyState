

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

int
stdInterval::v_printf(sPtr<tinyState>  THIS,const char * fmt,va_list arg)
{
sPtr<stdFrameWork> fw;
	fw = THIS->application->fw();
	return fw->v_printf(THIS,fmt,arg);
}

int
stdInterval::v_printf_err(sPtr<tinyState>  THIS,const char * fmt,va_list arg)
{
sPtr<stdFrameWork> fw;
	fw = THIS->application->fw();
	return fw->v_printf_err(THIS,fmt,arg);
}

int
stdInterval::printf(sPtr<tinyState>  THIS,const char * fmt,...)
{
sPtr<stdFrameWork> fw;
int 	ret;
va_list arg;
	fw = THIS->application->fw();
	va_start(arg,fmt);
	ret = fw->v_printf(THIS,fmt,arg);
	va_end(arg);
	return ret;
}

int
stdInterval::printf_err(sPtr<tinyState>  THIS,const char * fmt,...)
{
sPtr<stdFrameWork> fw;
int 	ret;
va_list arg;
	fw = THIS->application->fw();
	va_start(arg,fmt);
	ret = fw->v_printf(THIS,fmt,arg);
	va_end(arg);
	return ret;
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
