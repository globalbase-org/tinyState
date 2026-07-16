

#ifndef ___mthLeastSquares_cpp_H___
#define ___mthLeastSquares_cpp_H___

#include	"mth2/c++/mthMatrix.h"

template<class __TYPE>
class mthLeastSquares : public mObject {
public:
	mthLeastSquares() {
	}
	mthLeastSquares(mthMatrix<__TYPE> inp,mthMatrix<__TYPE> v) {
		accMatrix = accumulationMatrix(inp);
		accVector = accumulationVector(inp,v);
	}
	mthMatrix<__TYPE> accumulationMatrix(mthMatrix<__TYPE> inp) const;
	mthMatrix<__TYPE> accumulationVector(
			mthMatrix<__TYPE> inp,
			mthMatrix<__TYPE> v) const;
	int add(mthMatrix<__TYPE> inp,mthMatrix<__TYPE> v);
	int sub(mthMatrix<__TYPE> inp,mthMatrix<__TYPE> v);
	mthMatrix<__TYPE> result() const;

	__TYPE estimate(mthMatrix<__TYPE> result);
private:
	mthMatrix<__TYPE>	accMatrix;
	mthMatrix<__TYPE>	accVector;
};


#endif
