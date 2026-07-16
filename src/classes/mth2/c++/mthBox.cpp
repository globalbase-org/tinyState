

#include	"_ts2/c++/mthBox_.h"
#include	"mth2/c++/mthSurfaceMesh.h"

CLASS_TINYSTATE(mth2/c++/mthBox,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

class mthSurfaceMesh;

class TS_THISCLASS : public TS_BASECLASS {
public:
	mthBox_(
		sPtr<tinyState> parent,
		mVector<double> size);

private:
protected:
	TS_DEFARGS
	sPtr<mthSurfaceMesh>	mesh;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
#include	"mth2/c++/mVector.h"
class tinyState;
TS_END_INTERFACE

#endif


mthBox_::mthBox_(TS_ARGS0)
        : tinyState_(parent)
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
	mesh = thNEW(mthSurfaceMesh,(ifThis));
sArray<mVector<double> > p;
sArray<mVertexIndex> vix;
	p.length(8);
	p[0] = mVector<double>(0,0,0);
	p[1] = mVector<double>(size.x,0,0);
	p[2] = mVector<double>(size.x,size.y,0);
	p[3] = mVector<double>(0,size.y,0);
int i;
mVector<double> shift(0,0,size.z);
	for ( i = 0 ; i < 4 ; i ++ )
		p[i+4] = p[i] + shift;
	for ( i = 0 ; i < 8 ; i ++ )
		vix[i] = mesh->add_vertex(p[i]);
int rf;
	rf = 1;
	mesh->add_face(vix[0],vix[1],vix[2],rf);
	mesh->add_face(vix[2],vix[3],vix[0],rf);

	mesh->add_face(vix[0],vix[5],vix[1],rf);
	mesh->add_face(vix[5],vix[0],vix[4],rf);

	mesh->add_face(vix[2],vix[1],vix[6],rf);
	mesh->add_face(vix[1],vix[5],vix[6],rf);

	mesh->add_face(vix[3],vix[2],vix[6],rf);
	mesh->add_face(vix[7],vix[3],vix[6],rf);

	mesh->add_face(vix[7],vix[0],vix[3],rf);
	mesh->add_face(vix[0],vix[7],vix[4],rf);

	mesh->add_face(vix[5],vix[4],vix[6],rf);
	mesh->add_face(vix[6],vix[4],vix[7],rf);

	return rDO|FIN_START;
}

TS_STATE(FIN_START)
{
	parent->eventHandler(thNEW(stdEvent,
		(TSE_RETURN,ifThis,mesh)));
	return rDO|FIN_TINYSTATE_START;
}


