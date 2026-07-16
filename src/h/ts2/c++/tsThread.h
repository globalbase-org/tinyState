
#ifndef ___tsThread_cpp_H___
#define ___tsThread_cpp_H___

#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/sThreadCond.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/stdThreadInfo.h"

#define THR_KILL(sig)	thrKill(sig)
#define THR_CATCH_KILL(__x,__y)	\
	if ( thrInfo->kill_begin() == KF_THR ) {__x} else {__y} \
	thrInfo->kill_finish();


class co_tsThreadKill;
class sCallSection;

class stdThread : public stdObject {
public:
	stdThread(sPtr<stdObject>  parent);
	~stdThread();
	virtual void setup();
	virtual void cleanup();
private:
	sPtr<stdObject> 		parent;
};

#define THR_CATCH(__TYPE,__err) \
	([](std::function<__TYPE()> ff) { \
		for ( ; ; ) {							\
			try {							\
				return ff();					\
			} catch ( sException& ex ) {				\
			sPtr<tinyState> caller = sCallSection::key->caller();	\
				caller->thrInfo->wait();			\
				if ( ex.is_ready(caller) )			\
					continue;				\
				errno = EINTR;					\
				return __err;					\
			}							\
		}								\
	})


class sThreadCond;
class sThreadMutex;
#include	"_ts2/c++/tsThread_pb.h"

#endif

