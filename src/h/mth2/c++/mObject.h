
#ifndef ___mObject_cpp_H___
#define ___mObject_cpp_H___

#include	"std2/includes.h"
#include	"ts2/c++/pObject.h"


#define MTH_PI	((__TYPE)M_PI)

#define __mOP1(__op)							\
	static float __op(float inp) {return ::__op##f(inp);};			\
	static double __op(double inp) {return ::__op(inp);};			\
	static long double __op(long double inp) {return ::__op##l(inp);};

#define mSqrt	mObject::sqrt
#define mSin	mObject::sin
#define mCos	mObject::cos
#define mTan	mObject::tan
#define mAtan	mObject::atan
#define mAcos	mObject::acos
#define mAsin	mObject::asin
#define mExp	mObject::exp
#define mLog	mObject::log
#define mFabs	mObject::fabs
#define mNorm	mObject::norm
#define mNorm2	mObject::norm2
#define mT	mObject::t

#define __mOP_1(__op,__type1,__type2)	\
	static __type1 __op(__type2<__type1> inp);
#define __mOP_2(__op,__exp,__type)		\
	static __type __op(__type inp) {	\
		return __exp;	\
	}
#define __mOP_3(__op,__type1,__type2)	\
	static __type2<__type1> __op(__type2<__type1> inp);

#define __mOP_0(__op,__exp,__type1)	\
	__mOP_2(__op,__exp,__type1)		\
	__mOP_1(__op,__type1,mQuaternion)	\
	__mOP_1(__op,__type1,mComplex)

#define __mOP_30(__op,__exp,__type1)	\
	__mOP_2(__op,__exp,__type1)		\
	__mOP_3(__op,__type1,mQuaternion)	\
	__mOP_3(__op,__type1,mComplex)

#define __mOP_EXP(__op,__exp)	\
	__mOP_0(__op,__exp,float)		\
	__mOP_0(__op,__exp,double)		\
	__mOP_0(__op,__exp,long double)

#define __mOP_EXP3(__op,__exp)	\
	__mOP_30(__op,__exp,float)		\
	__mOP_30(__op,__exp,double)		\
	__mOP_30(__op,__exp,long double)

#define __mOP_EXP_BODY_2(__op,__type1,__type2)	\
	__type2					\
	mObject::__op(__type1<__type2> inp) {	\
		return inp.__op();		\
	}
#define __mOP_EXP_BODY_3(__op,__type1,__type2)	\
	__type1<__type2>			\
	mObject::__op(__type1<__type2> inp) {	\
		return inp.__op();		\
	}

#define __mOP_EXP_BODY_1(__op,__type1)	\
	__mOP_EXP_BODY_2(__op,__type1,float)		\
	__mOP_EXP_BODY_2(__op,__type1,double)		\
	__mOP_EXP_BODY_2(__op,__type1,long double)

#define __mOP_EXP_BODY_31(__op,__type1)	\
	__mOP_EXP_BODY_3(__op,__type1,float)		\
	__mOP_EXP_BODY_3(__op,__type1,double)		\
	__mOP_EXP_BODY_3(__op,__type1,long double)

#define __mOP_EXP_BODY(__op)	\
	__mOP_EXP_BODY_1(__op,mQuaternion)	\
	__mOP_EXP_BODY_1(__op,mComplex)

#define __mOP_EXP_BODY3(__op)	\
	__mOP_EXP_BODY_31(__op,mQuaternion)	\
	__mOP_EXP_BODY_31(__op,mComplex)



template <class __TYPE>
class mQuaternion;
template <class __TYPE>
class mComplex;

class mObject : public pObject {
public:
	__mOP1(sqrt)
	__mOP1(sin)
	__mOP1(cos)
	__mOP1(tan)
	__mOP1(atan)
	__mOP1(acos)
	__mOP1(asin)
	__mOP1(exp)
	__mOP1(log)
	__mOP1(fabs)

	__mOP_EXP(norm2,mFabs(inp)*mFabs(inp))
	__mOP_EXP(norm,mFabs(inp))
	__mOP_EXP3(t,inp)

	virtual void print();
};

#endif
