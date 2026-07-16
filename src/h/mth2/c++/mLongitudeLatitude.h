

#ifndef ___mLongitudeLatitude_cpp_H__
#define ___mLongitudeLatitude_cpp_H__

#include	"mth2/c++/mCoordinate.h"
#include	"mth2/c++/mPolar.h"

/*
	mPolar coordinate
		latitude and longitude and altitude.
 */

template<class __TYPE>
class mLongitudeLatitude : public mCoordinate<__TYPE> {
public:
  	mLongitudeLatitude() {}
	mLongitudeLatitude(mEllipsoid<__TYPE> ellp)
		:
		mCoordinate<__TYPE>(ellp) {
	}
	mVector<__TYPE> to_xyz(mPolar<__TYPE>& v);
	mPolar<__TYPE> to_local(mVector<__TYPE>& v);

	virtual mVector<__TYPE> std_to_xyz(mVector<__TYPE> v) {
	mPolar<__TYPE> p;
		p.x = v.x;
		p.y = v.y;
		p.z = v.z;
	  	return to_xyz(p);
	}
	virtual mVector<__TYPE> std_to_local(mVector<__TYPE> v) {
	mPolar<__TYPE> p;
	mVector<__TYPE> ret;
		p = to_local(v);
		ret.x = p.x;
		ret.y = p.y;
		ret.z = p.z;
		return ret;
	}
private:
	__TYPE newton_one(__TYPE * ret_alt,__TYPE r,__TYPE z,__TYPE theta);
	__TYPE newton_estimate(__TYPE * ret_lat,
		__TYPE theta,__TYPE alt,__TYPE r,__TYPE z);
	void newton(__TYPE * ret_lat,__TYPE * ret_alt,
			__TYPE r,__TYPE z);
};

#endif

