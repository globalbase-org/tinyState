
#ifndef ___mTangentCoodinate_cpp_H___
#define ___mTangentCoodinate_cpp_H___


#include	"mth2/c++/mCoordinate.h"
#include	"mth2/c++/mQuaternion.h"
#include	"mth2/c++/mPolar.h"

/*
	Coordinate Axes
		origin point is given by the value ll latitude/longitude.
		X :: origin point to direction of South.
		Y :: origin point to direction of East.
		Z :: origin point to direction of zenith.
*/

template<class __TYPE>
class mTangentCoordinate : public mCoordinate<__TYPE> {
public:
  	mTangentCoordinate() {}
	mTangentCoordinate(
		mEllipsoid<__TYPE> ellp,
		mPolar<__TYPE>& ll);
	mVector<__TYPE> to_local(mVector<__TYPE>& v);
	mVector<__TYPE> to_xyz(mVector<__TYPE>& v);

	virtual mVector<__TYPE> std_to_xyz(mVector<__TYPE> v) {
	  	return to_xyz(v);
	}
	virtual mVector<__TYPE> std_to_local(mVector<__TYPE> v) {
		return to_local(v);
	}
protected:
	mVector<__TYPE> org_xyz;
	mPolar<__TYPE> org_ll;
	mQuaternion<__TYPE> roll;
	mQuaternion<__TYPE> roll_t;

	mQuaternion<__TYPE> debug_qlon;
	mQuaternion<__TYPE> debug_qlat;
};

#endif



