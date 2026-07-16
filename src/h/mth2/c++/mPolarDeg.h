

#ifndef ___mPolarDeg_cpp_H___
#define ___mPolarDeg_cpp_H___

#include	"mth2/c++/mPolar.h"

template<class __TYPE>
class mPolarDeg : public mPolar<__TYPE> {
public:
	mPolarDeg() {}
	mPolarDeg(__TYPE lat,__TYPE lon,__TYPE alt)
		:
		mPolar<__TYPE>(lat,lon,alt)
	{
	}
	

	virtual __TYPE lat() const {
	  	return this->x*DR;
	}
	virtual __TYPE lon() const {
		return this->y*DR;
	}
	virtual __TYPE alt() const {
		return this->z;
	}
	virtual __TYPE roll() const {
		return this->z*DR;
	}

	virtual __TYPE d_lat() const {
	  	return this->x;
	}
	virtual __TYPE d_lon() const {
		return this->y;
	}
	virtual __TYPE d_alt() const {
		return this->z;
	}
	virtual __TYPE d_roll() const {
		return this->z;
	}
};

#endif

