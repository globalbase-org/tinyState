

#include	"mth2/c++/mthSurfaceMesh.h"
#include	"_ts2/c++/co_mthSurfaceMesh_write_STL_.h"

CLASS_TINYSTATE(mth2/c++/co_mthSurfaceMesh_write_STL,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


class TS_THISCLASS : public TS_BASECLASS {
public:
	co_mthSurfaceMesh_write_STL_(
		sPtr<mthSurfaceMesh> parent,
		sPtr<tinyState> caller,
		sPtr<stdString> filename);
	
	sRptr<mthSurfaceMesh,tinyState>		parent;
private:
protected:
	TS_DEFARGS
	int ret;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class mthSurfaceMesh;
TS_END_INTERFACE

#endif


co_mthSurfaceMesh_write_STL_::co_mthSurfaceMesh_write_STL_(TS_ARGS0)
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

TS_THREAD(ACT_START)
{
	caller->eventHandler(thNEW(stdEvent,
		(TSE_RETURN,parent,parent->write_STL(filename))));
	return rDO|FIN_START;
}

