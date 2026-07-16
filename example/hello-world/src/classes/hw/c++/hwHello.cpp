#include	"_ts2/c++/hwHello_.h"

CLASS_TINYSTATE(hw/c++/hwHello,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

class TS_THISCLASS : public TS_BASECLASS {
public:
	hwHello_(
		sPtr<tinyState> parent);
private:
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE

#endif


hwHello_::hwHello_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
}


/*******************************************
	STATE MACHINE
********************************************/


TS_STATE(INI_START)
{
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
	::printf("Hello, World!\n");
	return rDO|FIN_START;
}

TS_STATE(FIN_START)
{
	return rDO|FIN_TINYSTATE_START;
}
