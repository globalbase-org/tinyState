

#include	"mth2/c++/mthMatrix.h"


template<class __TYPE>
int mthMatrix<__TYPE>::test;

template<class __TYPE>
__TYPE
mthMatrix<__TYPE>::det()  const
{
	if ( xlength != ylength )
		return 0;
	if ( xlength == 1 )
		return data[0];
int x;
int xx,yy;
double ret = 0;
int sign = 1;
	for ( x = 0 ; x < xlength ; x ++ ) {
	mthMatrix<__TYPE> m1;
		m1 = mthMatrix<__TYPE>(xlength-1,xlength-1);
		for ( xx = 0 ; xx < xlength-1 ; xx ++ )
			for ( yy = 0 ; yy < xlength-1 ; yy ++ ) {
				if ( xx < x )
					m1.set(xx,yy,  this->elm(xx,yy+1));
				else	m1.set(xx,yy,  this->elm(xx+1,yy+1));
			}
		ret += sign*this->elm(x,0)*m1.det();
		sign *= -1;
	}
	return ret;
}

/*
template<class __TYPE>
__TYPE
mthMatrix<__TYPE>::det() const
{
mthMatrix<__TYPE> m1;
__TYPE ret,tmp;
int x,y,y1,y2;
int s;
	if ( xlength != ylength )
		return 0;
	if ( xlength == 1 )
		return data[0];
	m1 = mthMatrix<__TYPE>(*this);
	s = 1;
	for ( y = 1 ; y < m1.ylength ; y ++ ) {
	__TYPE rate,pibot;
		pibot = m1.elm(y-1,y-1);
		if ( pibot == 0 ) {
			for ( y1 = y ; y1 < m1.ylength ; y1 ++ )
				if ( m1.elm(y-1,y1) )
					break;
			if ( y1 == m1.ylength )
				return 0;
			for ( x = 0 ; x < m1.xlength ; x ++ ) {
				tmp = m1.elm(x,y-1);
				m1.set(x,y-1,	m1.elm(x,y1));
				m1.set(x,y1, 	tmp);
			}
			pibot = m1.elm(y-1,y-1);
			s *= -1;
		}
		for ( y1 = y ; y1 < m1.ylength ; y1 ++ ) {		
			rate = m1.elm(y-1,y1)/pibot;
			for ( x = 0 ; x < m1.xlength ; x ++ )
				m1.set(x,y1,	m1.elm(x,y1) - rate*m1.elm(x,y-1));
		}
	}
	ret = 1;
	for ( x = 0 ; x < m1.ylength ; x ++ ) 
		ret *= m1.elm(x,x);
	return ret*s;
}
*/

template<class __TYPE>
mthMatrix<__TYPE>
mthMatrix<__TYPE>::equation(mthMatrix<__TYPE> v) const
{
mthMatrix<__TYPE> ret,m1;
int x,y;
int i;
__TYPE d;
	if ( xlength != ylength || xlength == 0 )
		return mthMatrix<__TYPE>(-1);
	d = det();
	if ( d == 0 )
		return mthMatrix<__TYPE>(-1);
	ret = mthMatrix<__TYPE>(v.xlength,v.ylength);
	for ( x = 0 ; x < ret.xlength ; x ++ )
		for ( y = 0 ; y < ret.ylength ; y ++ ) {
			m1 = mthMatrix<__TYPE>(*this);
			for ( i = 0 ; i < ret.ylength ; i ++ )
				m1.set(y,i,	v.elm(x,i));
			ret.set(x,y,	m1.det()/d);
		}
	return ret;
}


template<class __TYPE>
void
mthMatrix<__TYPE>::print()
{
int x,y;
 	::printf("<%i:%i:err:%i>\n",xlength,ylength,err);
	for ( y = 0 ; y < ylength ; y ++ ) {
		printf("[");
		for ( x = 0 ; x < xlength ; x ++ )
			printf("%Lf\t",(long double)elm(x,y));
		printf("]\n");
	}
}

template class mthMatrix<float>;
template class mthMatrix<double>;
template class mthMatrix<long double>;
