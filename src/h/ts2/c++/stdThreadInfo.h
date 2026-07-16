

#ifndef __stdThreadInfo_cpp_H___
#define __stdThreadInfo_cpp_H___

#include		"ts2/c++/stdObject.h"
#include		"ts2/c++/sThreadMutex.h"
#include		"ts2/c++/sThreadCond.h"

class tinyState;
class co_tsThreadKill;
class stdThreadInfo : public stdObject {
public:
	stdThreadInfo(sPtr<tinyState>);
	~stdThreadInfo();
	void start();
      	void finish();
	int kill_begin();
	void kill_finish();
	void kill(int sig);
	void setId();
	void resetId();
	int wait();
	void signal();


	sPtr<tinyState>			target;
	INTEGER64			createTime;
	INTEGER64			runStartTime;
protected:
	int			state;
#define KF_IDLE			0
#define KF_THR			1
#define KF_THR_REQUEST		2
#define KF_THR_CATCH		3
#define KF_THR_CATCH_REQUEST	4

  	unsigned		id_enabled:1;
	pthread_t		id;
       	sThreadMutex		m;	// mutex for KILL
	sPtr<co_tsThreadKill>	pol;
	sThreadCond		c;
	unsigned		signal_flag:1;
};



#endif
