

#include	"mth2/c++/mthFFT.h"


template<class __TYPE>
mthFFT<__TYPE>::mthFFT(int beki,int sign)
{
	this->beki = beki;
	this->sign = sign;
int i;
int size = 1<<beki;
	w_list.length(size);
	for ( i = 0 ; i < size ; i ++ ) {
		w_list[i].r = mCos(2*M_PI*i/size);
		w_list[i].i = sign*mSin(2*M_PI*i/size);
	}
}

template<class __TYPE>
mthFFT<__TYPE>::~mthFFT()
{
}



template <class __TYPE>
int 
mthFFT<__TYPE>::upsidedown(int a,int b)
{
int i;
int ret;
	ret = 0;
	for ( i = 0 ; i < b ; i ++ ) {
		ret <<= 1;
		if ( a & 1 )
			ret |= 1;
		a >>= 1;
	}
	return ret;
}

template <class __TYPE>
void
mthFFT<__TYPE>::_fastft(
		mComplex<__TYPE> * ret,
		mComplex<__TYPE> * list)
{
int i,imax,j,k,kstep;
int size;
mComplex<__TYPE> a,b,c;
	size = 1<<beki;
	for ( i = 0 ; i < size ; i ++ )
		ret[upsidedown(i,beki)] = list[i];
	imax = 1<<(beki-1);
	kstep = imax;
	for ( i = 1 ; i < size ; i <<= 1 ) {
		for ( j = 0 ; j < size ; j += 2*i ) {
			for ( k = 0 ; k < i ; k ++ ) {
				a = ret[j+k+i] * w_list[k*kstep];
				b = ret[j+k] + a;
				c = ret[j+k] - a;
				ret[j+k] = b;
				ret[j+k+i] = c;
			}
		}
		kstep = kstep >> 1;
	}
}


template <class __TYPE>
void
mthFFT<__TYPE>::exe(mComplex<__TYPE> * ret,mComplex<__TYPE> * list)
{
int size;
__TYPE a;
int i;
	_fastft(ret,list);
	if ( sign > 0 )
		return;
	size = 1<<beki;
	a = 1/(double)size;
	for ( i = 0 ; i < size ; i ++ ) {
		ret[i].r *= a;
		ret[i].i *= a;
	}
}


template class mthFFT<float>;
template class mthFFT<double>;
template class mthFFT<long double>;


