

#include	"_ts2/c++/mthSurfaceMesh_.h"
#include	"mth2/c++/co_mthSurfaceMesh_write_STL.h"

CLASS_TINYSTATE(mth2/c++/mthSurfaceMesh,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT



class TS_THISCLASS : public TS_BASECLASS {
public:
	mthSurfaceMesh_(
		sPtr<tinyState> parent);
	mVertexIndex add_vertex(
		const mVector<double> v);
	mFaceIndex add_face(
		const mVertexIndex& v1,
		const mVertexIndex& v2,
		const mVertexIndex& v3,
		int reverse_flag=0);
	int write_STL(sPtr<stdString> filename);
	int write_STL(const char * filename);
	sPtr<mthSurfaceMesh> write_STL(sPtr<tinyState> caller,sPtr<stdString> filename);
	int does_self_intersect();
	int is_closed();
	sPtr<mthSurfaceMesh>
		OR(sPtr<mthSurfaceMesh> m1);
	sPtr<mthSurfaceMesh>
		AND(sPtr<mthSurfaceMesh> m1);
	sPtr<mthSurfaceMesh>
		SUB(sPtr<mthSurfaceMesh> m1);
	void move(mVector<double> v);
	void scale(mVector<double> v);
	void axisRotate(int axis,double theta);
	sPtr<mthSurfaceMesh> copy();

	Mesh * __get();
private:
protected:
	TS_DEFARGS
	Mesh * mesh;
  	static sThreadMutex		global;
	sThreadMutex			me;


};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/IO/STL.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Named_function_parameters.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Aff_transformation_3.h>
#include <CGAL/Polygon_mesh_processing/transform.h>

#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/connected_components.h>


#include <vector>
#include <fstream>

namespace PMP = CGAL::Polygon_mesh_processing;
namespace params = CGAL::parameters;
using Kernel = CGAL::Exact_predicates_exact_constructions_kernel;
using Point3  = Kernel::Point_3;
using Mesh   = CGAL::Surface_mesh<Point3>;


#include	"ts2/c++/sRptr.h"
#include	"mth2/c++/mVector.h"
class tinyState;

class mVertexIndex : public mObject {
public:
	mVertexIndex() {
	}
	mVertexIndex(Mesh::Vertex_index _ix) {
		ix = _ix;
	}
	const Mesh::Vertex_index& vtx() const {
		return ix;
	}
	mVector<double>	org;
protected:
	Mesh::Vertex_index ix;
};

class mFaceIndex : public mObject {
public:
	mFaceIndex(Mesh::Face_index _ix) {
		ix = _ix;
	}
	const Mesh::Face_index& face() const {
		return ix;
	}
protected:
	Mesh::Face_index ix;
};

TS_END_INTERFACE

#endif


mthSurfaceMesh_::mthSurfaceMesh_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/

sThreadMutex
mthSurfaceMesh_::global;


mVertexIndex
mthSurfaceMesh_::add_vertex(const mVector<double> v)
{
sThreadMutexHandle __hdr(me);
Point3 p3(v.x,v.y,v.z);
mVertexIndex ret(mesh->add_vertex(p3));
	ret.org = v;
	return ret;
}


mFaceIndex
mthSurfaceMesh_::add_face(
	const mVertexIndex& v1,
	const mVertexIndex& v2,
	const mVertexIndex& v3,
	int reverse)
{
sThreadMutexHandle __hdr(me);
	if ( reverse == 0 ) {
	mFaceIndex ret(mesh->add_face(v1.vtx(),v2.vtx(),v3.vtx()));
		return ret;
	}
	else {
	mFaceIndex ret(mesh->add_face(v1.vtx(),v3.vtx(),v2.vtx()));
		return ret;
	}
}

int
mthSurfaceMesh_::write_STL(sPtr<stdString> filename)
{
sThreadMutexHandle __hdr(me);
	std::ofstream out(filename->get_str(), std::ios::binary);
	CGAL::IO::set_binary_mode(out);
	CGAL::IO::write_STL(out, *mesh);
	return 0;
}

int
mthSurfaceMesh_::write_STL(const char * filename)
{
sThreadMutexHandle __hdr(me);
	std::ofstream out(filename, std::ios::binary);
	CGAL::IO::set_binary_mode(out);
	CGAL::IO::write_STL(out, *mesh);
	return 0;
}


sPtr<mthSurfaceMesh>
mthSurfaceMesh_::write_STL(sPtr<tinyState> caller,sPtr<stdString> filename)
{
	thNEW(co_mthSurfaceMesh_write_STL,(ifThis,caller,filename));
	return ifThis;
}

int
mthSurfaceMesh_::does_self_intersect()
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);

	return 	PMP::does_self_intersect(*mesh);
}
int
mthSurfaceMesh_::is_closed()
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);

	return 	CGAL::is_closed(*mesh);
}


sPtr<mthSurfaceMesh>
mthSurfaceMesh_::OR(sPtr<mthSurfaceMesh> m1)
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);
sPtr<mthSurfaceMesh> result;
	result = thNEW(mthSurfaceMesh,(ifThis));
	PMP::corefine_and_compute_union(*mesh,*m1->__get(),*result->__get());
	return result;
}

sPtr<mthSurfaceMesh>
mthSurfaceMesh_::AND(sPtr<mthSurfaceMesh> m1)
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);
sPtr<mthSurfaceMesh> result;
	result = thNEW(mthSurfaceMesh,(ifThis));
	PMP::corefine_and_compute_intersection(*mesh,*m1->__get(),*result->__get());
	return result;
}

sPtr<mthSurfaceMesh>
mthSurfaceMesh_::SUB(sPtr<mthSurfaceMesh> m1)
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);
sPtr<mthSurfaceMesh> result;
	result = thNEW(mthSurfaceMesh,(ifThis));
	PMP::corefine_and_compute_difference(*mesh,*m1->__get(),*result->__get());
	return result;
}

void
mthSurfaceMesh_::move(mVector<double> v)
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);

Kernel::Vector_3 t(v.x,v.y,v.z);
CGAL::Aff_transformation_3<Kernel> T(CGAL::TRANSLATION, t);
	PMP::transform(T, *mesh);
}

void
mthSurfaceMesh_::axisRotate(int axis,double theta)
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);

Kernel::FT c = Kernel::FT(std::cos(theta));
Kernel::FT s = Kernel::FT(std::sin(theta));
CGAL::Aff_transformation_3<Kernel> R;

	switch ( axis ) {
	case 0:
		R = CGAL::Aff_transformation_3<Kernel>(
		    1,  0, 0,
		    0,  c, -s,
		    0,  s,  c
		);
		break;
	case 1:
		R = CGAL::Aff_transformation_3<Kernel>(
		    c,  0, -s,
		    0,  1,  0,
		    s,  0,  c
		);
		break;
	case 2:
		R = CGAL::Aff_transformation_3<Kernel>(
		    c, -s, 0,
		    s,  c, 0,
		    0,  0, 1
		);
		break;
	}

	PMP::transform(R, *mesh);
}

void
mthSurfaceMesh_::scale(mVector<double> sc)
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);

Kernel::FT x = Kernel::FT(sc.x);
Kernel::FT y = Kernel::FT(sc.y);
Kernel::FT z = Kernel::FT(sc.z);
CGAL::Aff_transformation_3<Kernel> R;

	R = CGAL::Aff_transformation_3<Kernel>(
		    x,  0, 0,
		    0,  y, 0,
		    0,  0,  z
	);

	PMP::transform(R, *mesh);
}

sPtr<mthSurfaceMesh>
mthSurfaceMesh_::copy()
{
sThreadMutexHandle __hdr1(me);
sPtr<mthSurfaceMesh> ret;
	ret = thNEW(mthSurfaceMesh,(ifThis));
	*ret->__get() = *mesh;
	return ret;
}



Mesh *
mthSurfaceMesh_::__get()
{
	return mesh;
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	mesh = new Mesh();
	return rDO|ACT_START;
}


TS_THREAD(FIN_START)
{
sThreadMutexHandle __hdr1(me);
sThreadMutexHandle __hdr2(global);
	delete mesh;
	mesh = 0;
	return rDO|FIN_TINYSTATE_START;
}
