

#ifndef ___stdHalfOrderQueueTS_H___
#define ___stdHalfOrderQueueTS_H___

#include	"ts2/c++/stdHalfOrderQueue.h"
#include	"ts2/c++/tinyState.h"

class stdHalfOrderQueueTS : public stdHalfOrderQueue {
public:
	stdHalfOrderQueueTS();
	stdHalfOrderQueueTS(sPtr<stdHalfOrderQueueTS>  q,sPtr<tinyState>  caller);
	sPtr<tinyState>  del(sPtr<tinyState>  caller);
	sPtr<tinyState>  getTop() {
	  	return sPtr<tinyState>::d_cast(stdHalfOrderQueue::getTop());
	}
};

#endif
