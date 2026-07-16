

#ifndef ___mCoordinate_cpp_H___
#define ___mCoordinate_cpp_H___


#include	"mth2/c++/mVector.h"
#include	"mth2/c++/mEllipsoid.h"


/*
	Coordinate Axes
		X axis :: origin of the Eath to longitude 0 point on the equator.
		Y axis :: origin of the Eath to longitude 90E point on the equator.
		Z axis :: origin of the Eath to latitude 90N point (North pole).
 */


template<class __TYPE>
class mCoordinate : public mObject {
public:
  	mCoordinate() 
		: _ellp()
	{
	};
	mCoordinate(mEllipsoid<__TYPE> ellp) {
		_ellp = ellp;
	}
	virtual int is_enabled() {
		return _ellp.is_enabled();
	}

	virtual mVector<__TYPE> std_to_xyz(mVector<__TYPE> v) {
		return v;
	}
	virtual mVector<__TYPE> std_to_local(mVector<__TYPE> v) {
		return v;
	}
	mEllipsoid<__TYPE> ellp() {
		return _ellp;
	}
protected:
	mEllipsoid<__TYPE> _ellp;
};


#endif
