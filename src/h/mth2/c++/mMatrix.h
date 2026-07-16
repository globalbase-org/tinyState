

#ifndef ___mMatrix_cpp_H___
#define ___mMatrix_cpp_H___


#include	"mth2/c++/mObject.h"
#include	"mth2/c++/mVector.h"

#define mObject_ELEMENTS	3

template <class __TYPE>
class mMatrix : public mObject {
public:
	mMatrix() {
		clear();
	};
	mMatrix(mVector<__TYPE> inp) {
		clear();
		e[0][0] = inp.x;
		e[1][1] = inp.y;
		e[2][2] = inp.z;
	}
	mMatrix(mVector<__TYPE> inp1,
		mVector<__TYPE> inp2,
		mVector<__TYPE> inp3) {

		e[0][0] = inp1.x;
		e[0][1] = inp1.y;
		e[0][2] = inp1.z;

		e[1][0] = inp2.x;
		e[1][1] = inp2.y;
		e[1][2] = inp2.z;

		e[2][0] = inp3.x;
		e[2][1] = inp3.y;
		e[2][2] = inp3.z;
	}

	mMatrix	operator+(mMatrix m) const {
	int i,j;
	mMatrix<__TYPE> ret;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ )
				ret.e[i][j] = e[i][j] + m.e[i][j];
		return ret;
	}
	mMatrix	operator-(mMatrix m) const {
	int i,j;
	mMatrix<__TYPE> ret;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ )
				ret.e[i][j] = e[i][j] - m.e[i][j];
		return ret;
	}
	mMatrix	operator*(mMatrix m) const {
	int i,j,k;
	mMatrix<__TYPE> ret;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ ) {
			__TYPE a;
				a = 0;
				for ( k = 0 ; k < mObject_ELEMENTS ; k ++ )
					a += e[k][j] * m.e[i][k];
				ret.e[i][j] = a;
			}
		return ret;
	}
	mVector<__TYPE>	operator*(mVector<__TYPE> v) const {
	mVector<__TYPE> ret;
		ret.x = v.x*e[0][0] + v.y*e[1][0] + v.z*e[2][0];
		ret.y = v.x*e[0][1] + v.y*e[1][1] + v.z*e[2][1];
		ret.z = v.x*e[0][2] + v.y*e[1][2] + v.z*e[2][2];
		return ret;
	}

	mMatrix	operator*(__TYPE c) const {
	int i,j;
	mMatrix<__TYPE> ret;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ ) {
				ret.e[i][j] = e[i][j]*c;
			}
		return ret;
	}
	int operator==(mMatrix m) const {
	int i,j;
	mMatrix<__TYPE> ret;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ )
				if ( e[i][j] != m.e[i][j] )
					return 0;
		return 1;
	}
	int operator!=(mMatrix m) const {
	int i,j;
	mMatrix<__TYPE> ret;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ )
				if ( e[i][j] != m.e[i][j] )
					return 1;
		return 0;
	}
	mMatrix t() const {
	mMatrix<__TYPE> ret;
	int i,j;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ )
				ret[i][j] = e[j][i];
		return ret;
	}
	__TYPE det() const {
	mMatrix<__TYPE> tmp;
	__TYPE ret = 0;
	int i,j,k;
	int s;
		tmp = *this;
		s = 1;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ ) {
			if ( tmp.e[i][i] == 0 ) {
				for ( j = i+1 ; j < mObject_ELEMENTS ; j ++ )
					if ( tmp.e[j][i] )
						break;
				if ( j == mObject_ELEMENTS )
					return 0;
			__TYPE swap;
				for ( k = 0 ; k < mObject_ELEMENTS ; k ++ ) {
					swap = tmp.e[j][k];
					tmp.e[j][k] = tmp.e[i][k];
					tmp.e[i][k] = swap;
				}
				s *= -1;
			}
			for ( j = i+1 ; j < mObject_ELEMENTS ; j ++ ) {
			__TYPE r;
				r = tmp.e[j][i]/tmp.e[i][i];
				for ( k = 0 ; k < mObject_ELEMENTS ; k ++ )
					tmp.e[j][k] = tmp.e[j][k] - r*tmp.e[i][k];
			}
		}
		ret = 1;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			ret *= tmp.e[i][i];
		return ret*s;
	}
	__TYPE elm(int i,int j) const {
		return e[i][j];
	}
	mMatrix E() const {
	mMatrix ret;
	int i;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			ret.e[i][i] = 1;
		return ret;
	}

	void print() const {
	int i,j;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ ) {
			::printf("[");
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ ) {
				::printf("%Lf ",(long double)e[j][i]);
			}
			::printf("]\n");
		}
	}

protected:
	void clear() {
	int i,j;
		for ( i = 0 ; i < mObject_ELEMENTS ; i ++ )
			for ( j = 0 ; j < mObject_ELEMENTS ; j ++ )
				e[i][j] = 0;
	}
	__TYPE	e[3][3];
};

#endif
