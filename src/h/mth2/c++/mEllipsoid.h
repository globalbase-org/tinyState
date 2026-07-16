


#ifndef ___mEllipsoid_cpp_H___
#define ___mEllipsoid_cpp_H___


#include	"mth2/c++/mObject.h"

//typedef long double GEO_FLOATING;
typedef double GEO_FLOATING;

template<class __TYPE>
class mEllipsoid : public mObject {
public:

	class Earth {
	public:
		static mEllipsoid<__TYPE> BESSEL_1841;
		static mEllipsoid<__TYPE> CLARKE_1880;
		static mEllipsoid<__TYPE> SK42_1940;
		static mEllipsoid<__TYPE> EVEREST_1956;
		static mEllipsoid<__TYPE> AUSTRALIA_1965;
		static mEllipsoid<__TYPE> SUSA_1969;
		static mEllipsoid<__TYPE> IAG67_1967;
		static mEllipsoid<__TYPE> WGS72_1972;
		static mEllipsoid<__TYPE> IAU76_1976;
		static mEllipsoid<__TYPE> GRS80_1979;
		static mEllipsoid<__TYPE> WGS84_1986;
		static mEllipsoid<__TYPE> IERS_1989;
		static mEllipsoid<__TYPE> PZ90_1990;
		static mEllipsoid<__TYPE> IERS_2003;
		static mEllipsoid<__TYPE> GSK2011_2011;
	};

	mEllipsoid() {
		_a = _b = 0;
	}
	mEllipsoid(const char * method,__TYPE a1,__TYPE a2) {
		if ( strcmp(method,"ab") == 0 )
			ab(a1,a2);
		else if ( strcmp(method,"eb") == 0 )
			eb(a1,a2);
		else if ( strcmp(method,"ea") == 0 )
			ea(a1,a2);
		else if ( strcmp(method,"fa") == 0 )
			fa(a1,a2);
		else if ( strcmp(method,"fb") == 0 )
			fb(a1,a2);
	}

	int is_enabled() {
	  	return _a || _b;
	}

	__TYPE a() {
		return _a;
	}
	__TYPE b() {
		return _b;
	}
	__TYPE e() {
		return sqrt(1-_b*_b/(_a*_a));
	}
	__TYPE f() {
		return 1 - _b/_a;
	}
	void ab(__TYPE a,__TYPE b) {
		_a = a;
		_b = b;
	}
	void ea(__TYPE e,__TYPE a) {
		_a = a;
		_b = a*sqrt(1-e*e);
	}
	void eb(__TYPE e,__TYPE b) {
		_b = b;
		_a = b/sqrt(1-e*e);
	}
	void fa(__TYPE f,__TYPE a) {
		_a = a;
		_b = a*(1-f);
	}
	void fb(__TYPE f,__TYPE b) {
		_b = b;
		_a = b/(1-f);
	}

protected:
	__TYPE		_a,_b;
};



#endif

