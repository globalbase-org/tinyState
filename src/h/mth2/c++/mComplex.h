

#ifndef ___mComplex_cpp_H___
#define ___mComplex_cpp_H___


#include	"mth2/c++/mObject.h"


template <class __TYPE>
class mComplex : public mObject {
public:
	mComplex(__TYPE r,__TYPE i) {
		this->r = r;
		this->i = i;
	}
	mComplex() {
		r = i = 0;
	}
	mComplex operator + (mComplex inp) const {
	mComplex ret;
		ret.r = r + inp.r;
		ret.i = i + inp.i;
		return ret;
	}
	mComplex operator + (__TYPE inp) const {
	mComplex ret;
		ret.r = r + inp;
		return ret;
	}
	mComplex operator - (mComplex inp) const {
	mComplex ret;
		ret.r = r - inp.r;
		ret.i = i - inp.i;
		return ret;
	}
	mComplex operator - (__TYPE inp) const {
	mComplex ret;
		ret.r = r - inp;
		return ret;
	}
	mComplex operator * (mComplex inp) const {
	mComplex ret;
		ret.r = r * inp.r - i * inp.i;
		ret.i = r * inp.i + i * inp.r;
		return ret;
	}
	mComplex operator * (__TYPE inp) const {
	mComplex ret;
		ret.r = r * inp;
		ret.i = i * inp;
		return ret;
	}

	mComplex operator = (mComplex inp) {
		r = inp.r;
		i = inp.i;
		return *this;
	}

	mComplex operator = (__TYPE inp) {
		r = inp;
		i = 0;
		return *this;
	}

	bool operator == (mComplex<__TYPE> inp) {
		if ( r == inp.r && i == inp.i )
			return 1;
		return 0;
	}
	bool operator != (mComplex<__TYPE> inp) {
		if ( r == inp.r && i == inp.i )
			return 0;
		return 1;
	}

	mComplex t() const {
	mComplex ret;
		ret.r = r;
		ret.i = - i;
		return ret;
	}
	__TYPE norm2() const {
		return r*r + i*i;
	}
	__TYPE norm() const {
		return mSqrt(norm2());
	}
	mComplex inv() const {
		return t()/norm2();
	}
	mComplex operator / (mComplex inp) const {
		return (*this) * inp.inv();
	}
	mComplex operator / (__TYPE inp) const {
	mComplex ret;
		ret.r = r/inp;
		ret.i = i/inp;
		return ret;
	}
	__TYPE angle() const;
	mComplex exp() const;
	mComplex log() const;

	__TYPE r,i;
};


#endif
