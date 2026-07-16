

#ifndef ___mthMatrix_cpp_H___
#define ___mthMatrix_cpp_H___

#include	"mth2/c++/mObject.h"
#include	"ts2/c++/stdArray.h"
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sPtr.h"
#include	"mth2/c++/mMatrix.h"
#include	"mth2/c++/mVector.h"

template<class __TYPE>
class mthMatrix : public mObject {
public:
	static int test;
	mthMatrix()
  		:
		data(0)
	{
		err = -1;
	}
	mthMatrix(int err) 
  		:
		data(0)
	{
		xlength = ylength = 0;
		this->err = err;
	}
	mthMatrix(int xlength,int ylength) 
  		:
		data(0)
	{
		this->xlength = xlength;
		this->ylength = ylength;
		data.length(xlength*ylength);
		err = 0;
	}
	mthMatrix(mthMatrix const & inp) 
  		:
		data(0)
	{
	int i;
		xlength = inp.xlength;
		ylength = inp.ylength;
		data.length(xlength*ylength);
		for ( i = 0 ; i < xlength * ylength ; i ++ )
			data[i] = inp.data[i];
		err = inp.err;
	}
	mthMatrix(mMatrix<__TYPE> const inp) 
  		:
		data(0)
	{
	int i,j;
		xlength = ylength = 3;
		data.length(xlength*ylength);
		for ( j = 0 ; j < 3 ; j ++ )
			for ( i = 0 ; i < 3 ; i ++ )
				set(i,j,inp.elm(i,j));
		err = 0;
	}
	mthMatrix(mVector<__TYPE> const inp) 
  		:
		data(0)
	{
	int i,j;
		xlength = 1;
		ylength = 3;
		data.length(xlength*ylength);
		for ( j = 0 ; j < 3 ; j ++ )
			set(0,j,inp.e[j]);
		err = 0;
	}
	~mthMatrix() {
	}
	int get_err() {
		return err;
	}
	int get_xlength() {
		return xlength;
	}
	int get_ylength() {
		return ylength;
	}
	__TYPE elm(int x,int y) const {
		if ( x < 0 || x >= xlength )
			return 0;
		if ( y < 0 || y >= ylength )
			return 0;
		return data[x+y*xlength];
	}
	int set(int x,int y,__TYPE inp) {
		if ( x < 0 || x >= xlength )
			return -1;
		if ( y < 0 || y >= ylength )
			return -1;
		data[x+y*xlength] = inp;
		return 0;
	}
	mthMatrix operator=(mthMatrix const & inp) {
	int i;
		xlength = inp.xlength;
		ylength = inp.ylength;
		data.length(xlength*ylength);
		for ( i = 0 ; i < xlength * ylength ; i ++ )
			data[i] = inp.data[i];
		err = inp.err;
		return *this;
	}

	mthMatrix operator+ (mthMatrix inp) {
	mthMatrix ret;
	int len,i;
		if ( xlength != inp.xlength ||
				ylength != inp.ylength )
			return mthMatrix(-1);
		ret = mthMatrix<__TYPE>(xlength,ylength);
		len = xlength * ylength;
		for ( i = 0 ; i < len ; i ++ )
			ret.data[i] = data[i] + inp.data[i];
		return ret;
	}
	mthMatrix operator- (mthMatrix inp) {
	mthMatrix ret;
	int len,i;
		if ( xlength != inp.xlength ||
				ylength != inp.ylength )
			return mthMatrix(-1);
		ret = mthMatrix<__TYPE>(xlength,ylength);
		len = xlength * ylength;
		for ( i = 0 ; i < len ; i ++ )
			ret.data[i] = data[i] - inp.data[i];
		return ret;
	}
	mthMatrix operator* (mthMatrix inp) {
	mthMatrix ret;
	int i,j,x,y;
		if ( xlength != inp.ylength )
			return mthMatrix(-1);
		ret = mthMatrix<__TYPE>(inp.xlength,ylength);
		j = 0;
		for ( y = 0 ; y < ret.ylength ; y ++ )
			for ( x = 0 ; x < ret.xlength ; x ++ ) {
			__TYPE acc = 0;
				for ( i = 0 ; i < ylength ; i ++ )
					acc += elm(i,y) * inp.elm(x,i);
				ret.data[j++] = acc;
			}
		return ret;
	}
	mthMatrix operator*(__TYPE c) {
	mthMatrix ret;
	int len,i;
		ret = mthMatrix<__TYPE>(xlength,ylength);
		len = xlength * ylength;
		for ( i = 0 ; i < len ; i ++ )
			ret.data[i] = data[i] * c;
		return ret;
	}
	mthMatrix operator/(__TYPE c)  {
	mthMatrix ret;
	int len,i;
		ret = mthMatrix<__TYPE>(xlength,ylength);
		len = xlength * ylength;
		for ( i = 0 ; i < len ; i ++ )
			ret.data[i] = data[i] / c;
		return ret;
	}
	mthMatrix t()  {
	mthMatrix ret;
	int x,y,i;
		ret = mthMatrix<__TYPE>(ylength,xlength);
		for ( y = 0 ; y < ret.ylength ; y ++ )
			for ( x = 0 ; x < ret.xlength ; x ++ )
				ret.data[i++] = elm(y,x);
		return ret;
	}
	__TYPE det() const;
	mthMatrix equation(mthMatrix v) const;

	mthMatrix E() {
	int x;
	mthMatrix<__TYPE> ret;
		if ( xlength != ylength )
			return mthMatrix<__TYPE>(-1);
		ret = mthMatrix<__TYPE>(xlength,xlength);
		for ( x = 0 ; x < xlength ; x ++ )
			ret.set(x,x,1);
		return ret;
	}
	mthMatrix inv() {
		return equation(E());
	}
	void print();
private:
	int xlength,ylength;
	int err;
	sArray<__TYPE> data;
};

#endif
