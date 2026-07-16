

#ifndef ___tsApplication_cpp_H___
#define ___tsApplication_cpp_H___

#include <functional>
#include	"ts2/c++/stdFrameWork.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/tsGC.h"
#include	"ts2/c++/fwIO.h"

class tsApplication;
class tsApplication_;
class tsThread;
class fwIO;

typedef std::function<void(sPtr<tsApplication>)> TS_APPLICATION_FUNC;
class sThreadMutexRecursive;
#include	"_ts2/c++/tsApplication_pb.h"

#endif


