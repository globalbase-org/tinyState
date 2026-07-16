


#include	"mth2/c++/mLongitudeLatitude.h"
#include	"ts2/c++/stdObject.h"

template<class __TYPE>
mVector<__TYPE>
mLongitudeLatitude<__TYPE>::to_xyz(mPolar<__TYPE>& v)
{
__TYPE theta,r,f;
	f = this->ellp().b()/this->ellp().a();
	if ( -MTH_PI/4 < v.lat() && v.lat() < MTH_PI/4 ) {
		theta = mAtan(f*mTan(v.lat()));
	}
	else {
		theta = MTH_PI/2 - mAtan(mCos(v.lat())/(mSin(v.lat())*f));
	}
	r = this->ellp().a()*mCos(theta) + v.alt()*mCos(v.lat());
	return mVector<__TYPE>(
		r*mCos(v.lon()),
		r*mSin(v.lon()),
		this->ellp().b()*mSin(theta) 
				+ v.alt()*mSin(v.lat()));
}


template<class __TYPE>
mPolar<__TYPE>
mLongitudeLatitude<__TYPE>::to_local(mVector<__TYPE>& v)
{
__TYPE phi,lambda,r;
__TYPE alt;

	r = mSqrt(v.x*v.x + v.y*v.y);
	if ( v.x == 0 ) {
		if ( v.y == 0 ) {
			lambda = 0;
			if ( v.z == 0 ) {
				phi = 0;
				alt = -this->ellp().b();
			}
			else if ( v.z > 0 ) {
				phi = MTH_PI/2;
				alt = v.z - this->ellp().b();
			}
			else {
				phi = -MTH_PI/2;
				alt = this->ellp().b() - v.z;
			}
		}
		else {
			newton(&phi,&alt,r,v.z);
			if ( v.y >= 0 )
				lambda = MTH_PI/2;
			else	lambda = -MTH_PI/2;
		}
	}
	else if ( v.y == 0 ) {
		newton(&phi,&alt,r,v.z);
		if ( v.x >= 0 )
			lambda = 0;
		else	lambda = MTH_PI;
	}
	else {
		newton(&phi,&alt,r,v.z);
		if ( v.x < 0 ) {
			if ( v.y < 0 ) {
				if ( mFabs(v.x) < mFabs(v.y) ) {
					lambda = - MTH_PI/2 - mAtan(v.x/v.y);
				}
				else {
					lambda = -MTH_PI + mAtan(v.y/v.x);
				}
			}
			else if ( mFabs(v.x) < mFabs(v.y) ) {
				lambda = MTH_PI/2 - mAtan(v.x/v.y);
			}
			else {
				lambda = MTH_PI + mAtan(v.y/v.x);
			}
		}
		else if ( v.y < 0 ) {
			if ( mFabs(v.x) < mFabs(v.y) ) {
				lambda = MTH_PI/2 + mAtan(v.x/v.y);
			}
			else {
				lambda = mAtan(v.y/v.x);
			}
		}
		else if ( mFabs(v.x) < mFabs(v.y) ) {
			lambda = MTH_PI/2 - mAtan(v.x/v.y);
		}
		else {
			lambda = mAtan(v.y/v.x);
		}
	}

	return mPolar<__TYPE>(phi,lambda,alt);
}

template<class __TYPE>
__TYPE
mLongitudeLatitude<__TYPE>::newton_one(
	__TYPE * ret_alt,
	__TYPE r,__TYPE z,
	__TYPE theta)
{

__TYPE a = this->ellp().a();
__TYPE b = this->ellp().b();
__TYPE f = b/a;
__TYPE f2 = f*f;

__TYPE c = mCos(theta);
__TYPE s = mSin(theta);

__TYPE A = f2*z*a;
__TYPE B = r*b;
__TYPE C = (1-f2)*a*b;

__TYPE ff = A*c - B*s + C*c*s;
__TYPE fd =  - A*s - B*c +C*(2*c*c - 1);

	theta = theta - ff/fd;
	if ( theta > MTH_PI )
		theta = MTH_PI;
	else if ( theta < -MTH_PI )
		theta = -MTH_PI;

__TYPE Z = z - b*s;
__TYPE R = r - a*c;
	*ret_alt = mSqrt(Z*Z + R*R);
	return theta;
}


template<class __TYPE>
__TYPE
mLongitudeLatitude<__TYPE>::newton_estimate(
	__TYPE * ret_lat,
	__TYPE theta,__TYPE alt,
	__TYPE r,__TYPE z)
{
__TYPE c = mCos(theta);
__TYPE s = mSin(theta);
__TYPE a = this->ellp().a();
__TYPE b = this->ellp().b();
__TYPE f = b/a;

__TYPE lat;
	if ( -MTH_PI/4 < theta && theta < MTH_PI/4 ) {
		lat = mAtan(s/(c*f));
	}
	else {
		lat = MTH_PI/2 - mAtan(c/s*f);
	}

__TYPE R = a*c + alt*mCos(lat);
__TYPE Z = b*s + alt*mSin(lat);
__TYPE dR = R-r;
__TYPE dZ = Z-z;
	*ret_lat = lat;
	return dR*dR* + dZ*dZ;
}

template<class __TYPE>
void
mLongitudeLatitude<__TYPE>::newton(
	__TYPE * ret_lat,__TYPE * ret_alt,
	__TYPE r,__TYPE z)
{
__TYPE theta = 0;
__TYPE alt,lat,delta;
__TYPE prev_theta,prev_alt,prev_lat;
int count = 5;

	delta = -1;
//::printf("NEWTON-BEGIN %lf %lf\n",(double)r,(double)z);
	for ( ; ; count -- ) {
	__TYPE d;
//::printf("NEWTON-1\n");
		prev_theta = theta;
		prev_alt = alt;
		prev_lat = lat;
		theta = newton_one(&alt,r,z,theta);
		if ( count > 0 )
			continue;
		d = newton_estimate(&lat,theta,alt,r,z);
//::printf("NEWTON-2 %Lg %Lg\n",(long double)delta,(long double)d);
if ( isnan(delta) )
stdObject::panic("STOP");
		if ( delta >= 0 && d >= delta ) {
			*ret_alt = prev_alt;
			*ret_lat = prev_lat;
			return;
		}
		delta = d;
	}
::printf("NEWTON-END\n");
}

template class mLongitudeLatitude<float>;
template class mLongitudeLatitude<double>;
template class mLongitudeLatitude<long double>;
