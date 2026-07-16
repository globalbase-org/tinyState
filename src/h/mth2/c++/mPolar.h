

#ifndef ___mPolar_cpp_H___
#define ___mPolar_cpp_H___


#include	"mth2/c++/mVector.h"

#define DR	(2*MTH_PI/360)

template<class __TYPE>
class mPolar : public mVector<__TYPE> {
public:
 	mPolar() {}
	mPolar(__TYPE lat,__TYPE lon,__TYPE alt)
		:
		mVector<__TYPE>(lat,lon,alt)
	{
	}
	
	mPolar(const mPolar & v)
	{
	int i;
		this->len = 3;
		for ( i = 0 ; i < this->len ; i ++ )
			this->e[i] = v.e[i];
	}

	mPolar operator=(mVector<__TYPE> v) {
	int i;
		for ( i = 0 ; i < this->len ; i++ )
			this->e[i] = v.e[i];
		return *this;
	}


	virtual __TYPE lat() const {
	  	return this->x;
	}
	virtual __TYPE lon() const {
		return this->y;
	}
	virtual __TYPE alt() const {
		return this->z;
	}
	virtual __TYPE roll() const {
		return this->z;
	}
	virtual void lat(__TYPE inp) {
	  	this->x = inp;
	}
	virtual void lon(__TYPE inp) {
	  	this->y = inp;
	}
	virtual void alt(__TYPE inp) {
	  	this->z = inp;
	}
	virtual void roll(__TYPE inp) {
	  	this->z = inp;
	}

	virtual __TYPE d_lat() const {
	  	return this->x/DR;
	}
	virtual __TYPE d_lon() const {
		return this->y/DR;
	}
	virtual __TYPE d_alt() const {
		return this->z;
	}
	virtual __TYPE d_roll() const {
	  	return this->z/DR;
	}
};

#endif

