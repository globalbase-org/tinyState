

#include	"_ts2/c++/mthPrism_.h"

CLASS_TINYSTATE(mth2/c++/mthPrism,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include	"mth2/c++/mthSurfaceMesh.h"


class TS_THISCLASS : public TS_BASECLASS {
public:
	mthPrism_(
		sPtr<tinyState> parent,
		double height,
		double radius,
		double hat,
		double hat_radius,
		int num);
	
private:
protected:
	TS_DEFARGS
	sPtr<mthSurfaceMesh> target;


	sPtr<stdArray<mVertexIndex> >
		genRoundPoints(
			double height,
			double radius);

	void addFaces(sPtr<stdArray<mVertexIndex> > p,sPtr<stdArray<mVertexIndex> > n);

};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
class mthSurfaceMesh;
TS_END_INTERFACE

#endif


mthPrism_::mthPrism_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/


sPtr<stdArray<mVertexIndex> >
mthPrism_::genRoundPoints(
		double height,
		double radius)
{
int i;
sPtr<stdArray<mVertexIndex> > ret;
	ret = thNEW(stdArray<mVertexIndex>,(num));
	for ( i = 0 ; i < num ; i ++ ) {
	double theta = 2*M_PI*i/num;
		ret->ary[i] = target->add_vertex(
			mVector(mCos(theta)*radius,mSin(theta)*radius,height));
	}
	return ret;
}

void
mthPrism_::addFaces(sPtr<stdArray<mVertexIndex> > p,sPtr<stdArray<mVertexIndex> > n)
{
int i;
	for ( i = 0 ; i < num ; i ++ ) {
		target->add_face(n->ary[i],p->ary[i],n->ary[(i+1) % num]);
		target->add_face(p->ary[i],p->ary[(i+1) % num],
				n->ary[(i+1) % num]);
	}
}


/*******************************************
	STATE MACHINE
********************************************/



TS_THREAD(ACT_START)
{
mVertexIndex st;
mVertexIndex end;
sPtr<stdArray<mVertexIndex> > bottom;
sPtr<stdArray<mVertexIndex> > top;
sPtr<stdArray<mVertexIndex> > hat_bottom;
sPtr<stdArray<mVertexIndex> > hat_top;
int i;
	target = thNEW(mthSurfaceMesh,(ifThis));
	st = target->add_vertex(mVector<double>(0,0,-hat));
	end = target->add_vertex(mVector<double>(0,0,height+hat));
	bottom = genRoundPoints(0,radius);
	top = genRoundPoints(height,radius);
	if ( hat_radius ) {
		hat_bottom = genRoundPoints(-hat,hat_radius);
		hat_top = genRoundPoints(height+hat,hat_radius);
		for ( i = 0 ; i < num ; i ++ )
			target->add_face(hat_bottom->ary[i],st,hat_bottom->ary[(i+1) % num]);
		addFaces(hat_bottom,bottom);
	}
	else {
		for ( i = 0 ; i < num ; i ++ )
			target->add_face(bottom->ary[i],st,bottom->ary[(i+1) % num]);
	}
	addFaces(bottom,top);
	if ( hat_radius ) {
		addFaces(top,hat_top);
		for ( i = 0 ; i < num ; i ++ )
			target->add_face(hat_top->ary[i],hat_top->ary[(i+1) % num],end);
	}
	else {
		for ( i = 0 ; i < num ; i ++ )
			target->add_face(top->ary[i],top->ary[(i+1) % num],end);
	}
	if ( !target->is_closed() )
		stdObject::panic("NOT CLOSED\n");
	if ( target->does_self_intersect() )
		stdObject::panic("INTERSECTED\n");
	parent->eventHandler(thNEW(stdEvent,(TSE_RETURN,ifThis,target)));
	return rDO|FIN_START;
}
