

#include	"mth2/c++/mComplex.h"

template<class __TYPE>
__TYPE
mComplex<__TYPE>::angle() const {
	if ( r == 0 && i == 0 )
		return 0;
	if ( r > 0 ) {
		if ( i > 0 ) {
			// +r +i
			// 0 -- 90
			if ( mFabs(r) > mFabs(i) ) {
			  	// 0 -- 45
				return mAtan(i/r);
			}
			else {
				// 45 -- 90
				return M_PI/2 - mAtan(r/i);
			}
		}
		else {
			// +r -i
			// 0 -- -90
			if ( mFabs(r) > mFabs(i) ) {
				// 0 -- -45
				return mAtan(i/r);
			}
			else {
				// -45 -- -90
				return -M_PI/2 - mAtan(r/i);
			}
		}
	}
	else {
		if ( i > 0 ) {
			// -r +i
			// 90 -- 180
			if ( mFabs(r) > mFabs(i) ) {
				// 135 -- 180
				return M_PI + mAtan(i/r);
			}
			else {
				// 90 -- 135
				return M_PI/2 - mAtan(r/i);
			}
		}
		else {
			// -r -i
			// -90 -- -180
			if ( mFabs(r) > mFabs(i) ) {
				// -135 -- -180
				return - M_PI + mAtan(i/r);
			}
			else {
				// -90 -- -135
				return - M_PI/2 - mAtan(r/i);
			}
		}
	}
}


template<class __TYPE>
mComplex<__TYPE>
mComplex<__TYPE>::exp() const {
mComplex<__TYPE> ret;
	ret.r = mExp(r)*mCos(i);
	ret.i = mExp(r)*mSin(i);
	return ret;
}


template<class __TYPE>
mComplex<__TYPE>
mComplex<__TYPE>::log() const {
mComplex<__TYPE> ret;
	ret.r = mLog(norm());
	ret.i = angle();
	return ret;
}


template class mComplex<float>;
template class mComplex<double>;
template class mComplex<long double>;
