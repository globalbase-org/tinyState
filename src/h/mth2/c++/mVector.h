

#ifndef ___mVector_cpp_H___
#define ___mVector_cpp_H___

#include	"mth2/c++/mObject.h"

template<class __TYPE>
class mPolar;

template<class __TYPE>
class mVector : public mObject {
public:
  	mVector() :
		x(e[0]),
		y(e[1]),
		z(e[2])
	{
		x = y = z = 0;
		len = 3;
	}
	mVector(
		__TYPE _x,
		__TYPE _y,
		__TYPE _z) :
		x(e[0]),
		y(e[1]),
		z(e[2])
	{
		x = _x;
		y = _y;
		z = _z;
		len = 3;
	}
	mVector(const mVector & v) :
		x(e[0]),
		y(e[1]),
		z(e[2])
	{
	int i;
		len = 3;
		for ( i = 0 ; i < len ; i ++ )
			e[i] = v.e[i];
	}


	mVector operator=(mVector v) {
	int i;
		for ( i = 0 ; i < len ; i++ )
			e[i] = v.e[i];
		return *this;
	}

	mVector operator+(mVector v) const {
	mVector ret;
		ret.x = x + v.x;
		ret.y = y + v.y;
		ret.z = z + v.z;
		return ret;
	}
	mVector operator-(mVector v) const {
	mVector ret;
		ret.x = x - v.x;
		ret.y = y - v.y;
		ret.z = z - v.z;
		return ret;
	}
	mVector operator*(__TYPE a) const {
	mVector ret;
		ret.x = x*a;
		ret.y = y*a;
		ret.z = z*a;
		return ret;
	}
	__TYPE operator*(mVector v) const {
		return x*v.x + y*v.y + z*v.z;
	}
	mVector operator^(mVector v) const {
	mVector<__TYPE> ret;
		ret.x = y*v.z - z*v.y;
		ret.y = z*v.x - x*v.z;
		ret.z = x*v.y - y*v.x;
		return ret;
	}
	mVector operator/(__TYPE a) const {
	mVector ret;
		ret.x = x/a;
		ret.y = y/a;
		ret.z = z/a;
		return ret;
	}
	mVector& operator+=(mVector v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	mVector& operator-=(mVector v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	mVector& operator*=(__TYPE c) {
		x *= c;
		y *= c;
		z *= c;
		return *this;
	}
	mVector& operator/=(__TYPE c) {
		x /= c;
		y /= c;
		z /= c;
		return *this;
	}
	bool operator==(mVector v) const {
		if ( x != v.x )
			return false;
		if ( y != v.y )
			return false;
		if ( z != v.z )
			return false;
		return true;
	}
	bool operator!=(mVector v) const {
	  	return !(*this==v);
	}
	__TYPE norm2() const {
		return x*mT(x) + y*mT(y) + z*mT(z);
	}
	__TYPE norm() const {
		return mSqrt(this->norm2());
	}
	mPolar<__TYPE> polar() {
	__TYPE r,lat,lon;
		r = mSqrt(x*x + y*y);
		if ( r < z )
			lat = MTH_PI/2 - mTan(r/z);
		else	lat = mTan(z/r);
		if ( x > y )
			lon = mTan(y/x);
		else	lon = MTH_PI/2 - mTan(x/y);
		return mPolar<__TYPE>(lat,lon,norm());
	}
	mVector vsquare() const {
	mVector ret;
		ret.x = x*x;
		ret.y = y*y;
		ret.z = z*z;
		return ret;
	}
	mVector vsqrt() const {
	mVector ret;
		ret.x = mSqrt(x);
		ret.y = mSqrt(y);
		ret.z = mSqrt(z);
		return ret;
	}
	mVector vinverse() const {
	mVector ret;
		ret.x = 1/x;
		ret.y = 1/y;
		ret.z = 1/z;
		return ret;
	}
	mVector vfabs() const {
	mVector ret;
	int i;
		for ( i = 0 ; i < len ; i ++ )
			ret.e[i] = mFabs(e[i]);
		return ret;
	}
	mVector vmul(mVector<__TYPE> inp) const {
	mVector ret;
		ret.x = x*inp.x;
		ret.y = y*inp.y;
		ret.z = z*inp.z;
		return ret;
	}
	void vmax(mVector<__TYPE> inp) const {
		if ( x < inp.x )
			x = inp.x;
		if ( y < inp.y )
			y = inp.y;
		if ( z < inp.z )
			z = inp.z;
	}
	void vmin(mVector<__TYPE> inp) const {
		if ( x > inp.x )
			x = inp.x;
		if ( y > inp.y )
			y = inp.y;
		if ( z > inp.z )
			z = inp.z;
	}
	mQuaternion<__TYPE> rpy2q() const {
		mVector<__TYPE> tmp1,tmp2,tmp3;
		tmp1 = mVector<__TYPE>(x,y,0);
		tmp2 = mVector<__TYPE>(0,0,z);
		return mQuaternion<__TYPE>(tmp1.norm(),tmp1)
			* mQuaternion<__TYPE>(tmp2.norm(),tmp2);
	}

	virtual void print() const {
		::printf("V(%.20lg %.20lg %.20lg)",
			(double)x,(double)y,(double)z);
	}
	
  	__TYPE&	x;
  	__TYPE&	y;
  	__TYPE&	z;
	__TYPE	e[3];
	int		len;
};


#endif

