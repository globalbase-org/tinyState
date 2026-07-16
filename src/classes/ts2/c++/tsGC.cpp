

#include	"_ts2/c++/tsGC_.h"
#include	"ts2/c++/stdInterval.h"

CLASS_TINYSTATE(ts2/c++/tsGC,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


#include	"ts2/c++/stdHalfOrderQueueTS.h"
#include	"ts2/c++/sTimer.h"

class TS_THISCLASS : public TS_BASECLASS {
public:
	tsGC_(
		sPtr<tinyState>  parent);
	void inherit(
		sPtr<tinyState>  parent);

	void exe(sPtr<tinyState>  caller);
	virtual sPtr<stdEvent>  filter(sPtr<stdEvent>  ev);

private:
protected:
	sPtr<stdHalfOrderQueueTS> 	que;
	int 		last_pri;

	unsigned	priority_flag:1;
	sTimer		timer;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE

class TS_INTERFACE : public TS_BASEINTERFACE {
public:
	static INTEGER64 timer_mode;
};

TS_END_INTERFACE

#endif


tsGC_::tsGC_(
		sPtr<tinyState>  _parent)
        : tinyState_(_parent)
{
}

void
tsGC_::inherit(
	sPtr<tinyState>  _parent)
{
	this->TS_BASECLASS::inherit(_parent);
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/

INTEGER64
tsGC::timer_mode = 0;


void
tsGC_::exe(sPtr<tinyState>  caller)
{
int pri;
	que->ins(caller->priority(ifThis),caller);
	wakeup();
}


sPtr<stdEvent> 
tsGC_::filter(sPtr<stdEvent>  ev)
{
	if ( ev == thNULL )
		return ev;
	if ( ev->type == TSE_PRIORITY )
		priority_flag = 1;
	return TS_BASECLASS::filter(ev);
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	que = (thNEW( stdHalfOrderQueueTS,()));
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
	if ( que->count() == 0 )
		return rDO|ACT_TINYSTATE_CHECK1;
	stdInterval::wait(ifThis,0,TSE_RETURN);
	return ACT_RET;
}
TS_STATE(ACT_RET)
{
	R_TEST
	last_pri = 0;
	timer.start(ifThis,tsGC::timer_mode);
	return rDO|ACT_EXE;
}
TS_STATE(ACT_EXE)
{
sPtr<tinyState>  obj,ok;
int pri;
	if ( tsGC::timer_mode  && timer.is_expire(ifThis) )
		return rDO|ACT_START;
	if ( priority_flag ) {
		priority_flag = 0;
		que = thNEW( stdHalfOrderQueueTS,(que,ifThis));
	}
	ok = que->del(thNULL);
	if ( ok == thNULL )
		return rDO|ACT_START;
	pri = ok->priority(thNULL);
	if ( tsGC::timer_mode == 0 && last_pri && last_pri != pri ) {
		que->ins(ok->priority(ifThis),ok);
		return rDO|ACT_START;
	}
	ok->eventHandler(
		thNEW( stdEvent,(TSE_RETURN,ifThis,(INTEGER64)0)));
	last_pri = pri;
	return rDO;
}
TS_STATE(FIN_START)
{
	que = thNULL;
	return rDO|FIN_TINYSTATE_START;
}

