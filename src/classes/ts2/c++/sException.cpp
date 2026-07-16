

#include	"ts2/c++/sException.h"
#include	"ts2/c++/tinyState.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadMutexRecursive.h"
#include	"ts2/c++/tsThread.h"

sException::sException(std::function<int(sPtr<tinyState>)> r,int type)
{
	this->type = type;
	this->is_ready = r;
}

sException::~sException()
{
}

