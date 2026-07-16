

#include	"ss/c++/ssObject.h"
#include	"_ts2/c++/ssObject_.h"
#include	"ss/c++/ssApplication.h"

CLASS_TINYSTATE(ss/c++/ssObject,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

class TS_THISCLASS : public TS_BASECLASS {
public:
	ssObject_(
		sPtr<ssObject> parent);
	ssObject_(
		sPtr<tsApplication> parent);
	
	sRptr<ssObject,tinyState>		parent;
	sPtr<ssApplication>			ssApp;

private:
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class ssObject;
class ssApplication;
TS_END_INTERFACE

#endif


ssObject_::ssObject_(sPtr<ssObject> parent)
        : tinyState_(parent),
	  parent(tinyState_::parent)
{
    TS_CPARGS0
	ssApp = parent->ssApp;
}

ssObject_::ssObject_(sPtr<tsApplication> parent)
        : tinyState_(parent),
	  parent(tinyState_::parent)
{
    TS_CPARGS0
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/



/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
  	return rDO|INI_ssObject_START;
}
TS_STATE(INI_ssObject_START)
{
	return rDO|ACT_START;
}


TS_STATE(FIN_START)
{
	return rDO|FIN_ssObject_START;
}
TS_STATE(FIN_ssObject_START)
{
	ssApp = thNULL;
	return rDO|FIN_TINYSTATE_START;
}
