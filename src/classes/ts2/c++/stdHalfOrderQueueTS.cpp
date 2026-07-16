

#include	"ts2/c++/stdHalfOrderQueueTS.h"

stdHalfOrderQueueTS::stdHalfOrderQueueTS()
{
}

stdHalfOrderQueueTS::stdHalfOrderQueueTS(sPtr<stdHalfOrderQueueTS>  q,sPtr<tinyState>  caller)
{
sPtr<tinyState>  ts;
	for ( ; ; ) {
		ts = q->del(caller);
		if ( ts == thNULL )
			break;
		ins(ts->priority(thNULL),ts);
	}
}

sPtr<tinyState> 
stdHalfOrderQueueTS::del(sPtr<tinyState>  caller)
{
sPtr<tinyState>  ret;
int key,key2;
sPtr<stdHalfOrderQueueTS>  hoq;
	ret = sPtr<tinyState>::d_cast(stdHalfOrderQueue::del(&key));
	if ( ret == thNULL )
		return thNULL;
	key2 = ret->priority(caller);
	if ( caller == thNULL || key == key2 )
		return ret;
	hoq = thNEW( stdHalfOrderQueueTS,(this,caller));
	hoq->ins(key2,ret);
	top = hoq->__top();
	return sPtr<tinyState>::d_cast(stdHalfOrderQueue::del(&key));
}
