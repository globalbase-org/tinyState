


#include	"mth2/c++/mTangentCoordinate.h"
#include	"mth2/c++/mLongitudeLatitude.h"

template<class __TYPE>
mTangentCoordinate<__TYPE>::mTangentCoordinate(
		mEllipsoid<__TYPE> ellp,
		mPolar<__TYPE>& ll)
	:
	mCoordinate<__TYPE>(ellp)
{
mLongitudeLatitude<__TYPE> llc(ellp);
 
	org_ll = ll;
	org_ll.alt(0);
	org_xyz = llc.to_xyz(org_ll);

mQuaternion<__TYPE> qlon(-ll.lon(),mVector<__TYPE>(0,0,1));
mQuaternion<__TYPE> qlat(ll.lat()-MTH_PI/2,mVector<__TYPE>(0,1,0));
	roll = qlat*qlon;
	roll_t = roll.t();
	debug_qlat = qlat;
	debug_qlon = qlon;
}


template<class __TYPE>
mVector<__TYPE>
mTangentCoordinate<__TYPE>::to_local(mVector<__TYPE>& v)
{
mQuaternion<__TYPE> a,b,c;
	return (mVector<__TYPE>)(c=((b=roll*(a=mQuaternion<__TYPE>(v - org_xyz)))*roll_t));
}

template<class __TYPE>
mVector<__TYPE>
mTangentCoordinate<__TYPE>::to_xyz(mVector<__TYPE>& v)
{
	return ((mVector<__TYPE>)(roll_t*mQuaternion<__TYPE>(v)*roll)) + org_xyz;
}

template class mTangentCoordinate<float>;
template class mTangentCoordinate<double>;
template class mTangentCoordinate<long double>;
