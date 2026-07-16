

#include	"ts2/c++/tsApplication.h"
#include	"_ts2/c++/tsHoge_.h"

CLASS_TINYSTATE(ts2/c++/tsHoge,ts2/c++/ts2IO)


#if 0

TS_BEGIN_IMPLEMENT


class TS_THISCLASS : public TS_BASECLASS {
public:
	tsHoge_(
		sPtr<tsApplication> parent);
	
	sRptr<tsApplication,tinyState>		parent;
private:
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tsApplication;
TS_END_INTERFACE

#endif


tsHoge_::tsHoge_(TS_ARGS0)
        : ts2IO_(parent),
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



