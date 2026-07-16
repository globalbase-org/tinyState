

#include	"_ts2/c++/co_tsThreadKill_.h"
#include	"ts2/c++/stdInterval.h"

CLASS_TINYSTATE(ts2/c++/co_tsThreadKill,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


class TS_THISCLASS : public TS_BASECLASS {
public:
	co_tsThreadKill_(
		sPtr<tinyState>  parent,
		int sig);
	void inherit(
		sPtr<tinyState>  parent,
		int sig);
private:
protected:
	int sig;
};

TS_END_IMPLEMENT

#endif


co_tsThreadKill_::co_tsThreadKill_(
		sPtr<tinyState>  _parent,
		int sig)
        : tinyState_(_parent)
{
	this->sig = sig;
}

void
co_tsThreadKill_::inherit(
	sPtr<tinyState>  _parent,
	int sig)
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
	return rDO|ACT_START;
}
TS_STATE(ACT_START)
{
	stdInterval::wait(ifThis,10*1000,TSE_TIMER);
	return ACT_WAIT;
}
TS_STATE(ACT_WAIT)
{
	if ( C_TEST(parent->state(),C_ZOM|C_FIN) )
		return rDO|FIN_START;
	parent->thrKill(sig);
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_START;
}
