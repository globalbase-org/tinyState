

#ifndef ___mthFFT_cpp_H___
#define ___mthFFT_cpp_H___

#include	"ts2/c++/stdObject.h"
#include	"mth2/c++/mComplex.h"
#include	"ts2/c++/sArray.h"

#define FFT_SIGN_FFT		(-1)
#define FFT_SIGN_IFFT		(1)

template <class __TYPE>
class mthFFT : public stdObject {
public:
  	mthFFT(int beki,int sign);
	~mthFFT();

	void exe(mComplex<__TYPE> * ret,
			mComplex<__TYPE> * list);
protected:
	int upsidedown(int a,int b);
	void _fastft(mComplex<__TYPE> * ret,
			mComplex<__TYPE> * list);

	int beki;
	int sign;
	sArray<mComplex<__TYPE> > w_list;
};

#endif

