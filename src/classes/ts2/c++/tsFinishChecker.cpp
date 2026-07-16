

#include	"_ts2/c++/tsFinishChecker_.h"

CLASS_TINYSTATE(ts2/c++/tsFinishChecker,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


#include	"ts2/c++/sTimer.h"

class TS_THISCLASS : public TS_BASECLASS {
public:
	tsFinishChecker_(
		sPtr<tinyState>  parent,
		int sec);
	void inherit(
		sPtr<tinyState>  parent,
		int sec);
private:
protected:
	sTimer		timer;
	int 		sec;
};

TS_END_IMPLEMENT

#endif


tsFinishChecker_::tsFinishChecker_(
		sPtr<tinyState>  _parent,
		int sec)
        : tinyState_(_parent)
{
	this->sec = sec;
}

void
tsFinishChecker_::inherit(
	sPtr<tinyState>  _parent,
	int sec)
{
	this->TS_BASECLASS::inherit(_parent);
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/



/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	parent->listen(ifThis,TSE_DESTROY);
	timer.start(ifThis,sec*1000*1000);
	return rDO|ACT_START;
}
TS_STATE(ACT_START)
{
	if ( C_TEST(parent->state(),C_ZOM) )
		return rDO|FIN_START;
	if ( timer.is_expire(ifThis) ) {
		parent->trace("CHECK");
		return rDO|FIN_START;
	}
	return 0;
}

