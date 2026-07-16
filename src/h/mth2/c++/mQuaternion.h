


#ifndef ___mQuaternion_cpp_H___
#define ___mQuaternion_cpp_H___

#include	"mth2/c++/mObject.h"
#include	"mth2/c++/mVector.h"

template<class __TYPE>
class mQuaternion : public mObject {
public:
	mQuaternion() {
		r = i = j = k = 0;
	}
	mQuaternion(
		__TYPE _r,
		__TYPE _i=0,
		__TYPE _j=0,
		__TYPE _k=0) {
		r = _r;
		i = _i;
		j = _j;
		k = _k;
	}
	mQuaternion(
		__TYPE theta,
		mVector<__TYPE> v) {

	__TYPE n,d;
		d = v.norm();
		if ( d == 0 ) {
			r = 1;
			i = j = k = 0;
		}
		else {
			n = 1/d;
			theta = theta/2;
			r = mCos(theta);
			i = v.x*mSin(theta)*n;
			j = v.y*mSin(theta)*n;
			k = v.z*mSin(theta)*n;
		}
	}
	mQuaternion(mPolar<__TYPE> p) {
		*this = mQuaternion<__TYPE>(p.lon(),mVector<__TYPE>(0,0,1))
			* mQuaternion<__TYPE>(p.lat(),mVector<__TYPE>(0,1,0));
	}
	mQuaternion(
		mVector<__TYPE> v) {

		r = 0;
		i = v.x;
		j = v.y;
		k = v.z;
	}
	operator mVector<__TYPE> () const {
		return mVector<__TYPE>(i,j,k);
	}
	operator mPolar<__TYPE> () const {
	mVector<__TYPE> x(1,0,0);
	__TYPE lat,lon,roll,r;
		x = *this*x*this->t();
		r = x.x*x.x + x.y*x.y;
		if ( r < x.z )
			lat = MTH_PI/2 - mAtan(r/x.z);
		else	lat = mAtan(x.z/r);
		if ( x.x < x.y )
			lon = MTH_PI/2 - mAtan(x.x/x.y);
		else	lon = mAtan(x.x/x.y);
	mQuaternion q(mPolar<__TYPE>(lat,lon,1));
		q = q.inv()*(*this);
	mVector<__TYPE> y(0,1,0);
		y = q*y*q.t();
	__TYPE theta;
		if ( y.y < y.z )
			theta = MTH_PI/2 - mAtan(y.y/y.z);
		else	theta = mAtan(y.z/y.y);
		return mPolar<__TYPE>(lat,lon,theta);
	}

	mVector<__TYPE> torque() const {
	mQuaternion tmp;
	__TYPE theta,rr,n;
		n = norm();
		if ( n == 0 )
			return mVector<__TYPE>(0,0,0);
		tmp = *this/n;
		rr = mSqrt(tmp.i*tmp.i + tmp.j*tmp.j + tmp.k*tmp.k);
		if ( rr == 0 )
			return mVector<__TYPE>(0,0,0);
		if  ( tmp.r >= 0 ) {
			if ( tmp.r < 0.5 )
				theta = 2*mAcos(tmp.r);
			else 	theta = 2*mAsin(rr);
		}
		else {
			if ( tmp.r > -0.5 )
				theta = 2*(M_PI-mAcos(-tmp.r));
			else 	theta = 2*(M_PI-mAsin(rr));
		}
		theta = theta/rr;
		return mVector<__TYPE>(tmp.i*theta,tmp.j*theta,tmp.k*theta);
	}
	mVector<__TYPE> q2rpy() const {
	mVector<__TYPE> vz1,vz2,vz3,vz4;
	__TYPE ns,nc,theta,phi;
		vz1 = mVector<__TYPE>(0,0,1);
		vz3 = vz2 = *this*vz1*this->t();

		vz3.z = 0;
		ns = vz3.norm();

		if ( ns == 0 ) {
			theta = 0;
			vz3 = mVector<__TYPE>(1,0,0);
		}
		else {
			if ( vz2.z > 0 ) {
				if ( ns < vz2.z ) {
					theta = mAtan(ns/vz2.z);
				}
				else {
					theta = M_PI/2 - mAtan(vz2.z/ns);
				}
			}
			else {
				if ( ns < -vz2.z ) {
					theta = M_PI - mAtan(-ns/vz2.z);
				}
				else {
					theta = mAtan(-vz2.z/ns) + M_PI/2;
				}
			}
			vz3.x = -vz2.y;
			vz3.y = vz2.x;
			vz3 /= ns;
		}
		vz4 = this->t()*vz3*(*this);
		vz1 = vz4^vz3;
		ns = vz1.norm();
		nc = vz4*vz3;
		if ( ns > mFabs(nc) )
			phi = mAcos(nc);
		else if ( nc > 0 )
			phi = mAsin(ns);
		else	phi = M_PI-mAsin(ns);
		if ( vz1.z < 0 )
			phi = -phi;
		vz4.z = 0;
		return vz4 * theta + mVector<__TYPE>(0,0,phi);
	}

	bool operator==(mQuaternion q) const {
	  	if ( r != q.r )
			return false;
	  	if ( i != q.i )
			return false;
	  	if ( j != q.j )
			return false;
	  	if ( k != q.k )
			return false;
		return true;
	}
	bool operator!=(mQuaternion q) const {
		return ! (*this == q);
	}
	mQuaternion operator+(mQuaternion q) const {
	mQuaternion ret;
		ret.r = r + q.r;
		ret.i = i + q.i;
		ret.j = j + q.j;
		ret.k = k + q.k;
		return ret;
	}
	mQuaternion operator+(__TYPE a) const {
	mQuaternion ret;
		ret.r = r + a;
		ret.i = i;
		ret.j = j;
		ret.k = k;
		return ret;
	}
	mQuaternion operator-(mQuaternion q) const {
	mQuaternion ret;
		ret.r = r - q.r;
		ret.i = i - q.i;
		ret.j = j - q.j;
		ret.k = k - q.k;
		return ret;
	}
	mQuaternion operator-(__TYPE a) const {
	mQuaternion ret;
		ret.r = r - a;
		ret.i = i;
		ret.j = j;
		ret.k = k;
		return ret;
	}
	mQuaternion operator*(mQuaternion q) const {
	mQuaternion ret;
		ret.r = r*q.r - i*q.i - j*q.j - k*q.k;
		ret.i = r*q.i + i*q.r + j*q.k - k*q.j;
		ret.j = r*q.j - i*q.k + j*q.r + k*q.i;
		ret.k = r*q.k + i*q.j - j*q.i + k*q.r;
		return ret;
	}
	mQuaternion operator*(__TYPE a) const {
	mQuaternion ret;
		ret.r = r*a;
		ret.i = i*a;
		ret.j = j*a;
		ret.k = k*a;
		return ret;
	}
	mQuaternion t() const {
	mQuaternion ret;
		ret.r = r;
		ret.i = -i;
		ret.j = -j;
		ret.k = -k;
		return ret;
	}
	__TYPE norm2() const {
		return r*r + i*i + j*j + k*k;
	}
	__TYPE norm() const {
		return mSqrt(norm2());
	}
	mQuaternion inv() const {
		return this->t()/norm2();
	}
	mQuaternion operator/(mQuaternion q) const {
		return (*this)*q.inv();
	}
	mQuaternion operator/(__TYPE a) const {
		return (*this)*(1/a);
	}


	virtual void print() {
		printf("Q(%lf,%lf,%lf,%lf)",
			(double)r,
			(double)i,
			(double)j,
			(double)k);
	}

	static void printLoop_sub(
			mVector<__TYPE> ax,
			mVector<__TYPE> v) {
	mVector<__TYPE> v1;
	mQuaternion<__TYPE> a;
		a = mQuaternion<__TYPE>(M_PI/2,ax);
		v1 = a*v*a.t();
		printf("R(pi/2 - %lf %lf %lf) : (%lf %lf %lf) -> (%lf %lf %lf)\n",
			ax.x,ax.y,ax.z,
			v.x,v.y,v.z,
			v1.x,v1.y,v1.z);
	}
	static void printLoop() {
		printLoop_sub(mVector<__TYPE>(1,0,0),
				mVector<__TYPE>(0,1,0));
		printLoop_sub(mVector<__TYPE>(1,0,0),
				mVector<__TYPE>(0,0,1));
		printf("\n");

		printLoop_sub(mVector<__TYPE>(0,1,0),
				mVector<__TYPE>(1,0,0));
		printLoop_sub(mVector<__TYPE>(0,1,0),
				mVector<__TYPE>(0,0,1));
		printf("\n");

		printLoop_sub(mVector<__TYPE>(0,0,1),
				mVector<__TYPE>(1,0,0));
		printLoop_sub(mVector<__TYPE>(0,0,1),
				mVector<__TYPE>(0,1,0));
		printf("\n");
	}

	__TYPE	r,i,j,k;
};

#endif

