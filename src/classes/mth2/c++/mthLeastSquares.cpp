

#include	"mth2/c++/mthLeastSquares.h"

template<class __TYPE>
mthMatrix<__TYPE> 
mthLeastSquares<__TYPE>::accumulationMatrix(mthMatrix<__TYPE> inp) const {
mthMatrix<__TYPE> ret;
int x,y;
	ret = mthMatrix<__TYPE>(inp.get_ylength(),inp.get_ylength());
	for ( x = 0 ; x < ret.get_xlength() ; x ++ )
		for ( y = 0 ; y < ret.get_ylength() ; y ++ ) {
		int i;
		__TYPE acc = 0;
			for ( i = 0 ; i < inp.get_xlength() ; i ++ )
				acc += inp.elm(i,y)*inp.elm(i,x);
			ret.set(x,y,acc);	
		}
	return ret;
}


template<class __TYPE>
mthMatrix<__TYPE> 
mthLeastSquares<__TYPE>::accumulationVector(
		mthMatrix<__TYPE> inp,
		mthMatrix<__TYPE> v) const {
mthMatrix<__TYPE> ret;
int x,y;
	ret = mthMatrix<__TYPE>(1,inp.get_ylength());
	for ( y = 0 ; y < ret.get_ylength() ; y ++ ) {
	int i;
	__TYPE acc = 0;
		for ( i = 0 ; i < inp.get_xlength() ; i ++ )
			acc += v.elm(i,0)*inp.elm(i,y);
		ret.set(0,y,acc);
	}
	return ret;
}

template<class __TYPE>
int 
mthLeastSquares<__TYPE>::add(mthMatrix<__TYPE> inp,mthMatrix<__TYPE> v) {
	if ( accMatrix.get_xlength() == 0 ) {
		accMatrix = accumulationMatrix(inp);
		accVector = accumulationVector(inp,v);
		return 0;
	}
	accMatrix = accMatrix + accumulationMatrix(inp);
	accVector = accVector + accumulationVector(inp,v);
	if ( accMatrix.get_err() < 0 )
		return accMatrix.get_err();
	return accVector.get_err();
}

template<class __TYPE>
int 
mthLeastSquares<__TYPE>::sub(mthMatrix<__TYPE> inp,mthMatrix<__TYPE> v) {
	accMatrix = accMatrix - accumulationMatrix(inp);
	accVector = accVector - accumulationVector(inp,v);
	if ( accMatrix.get_err() < 0 )
		return accMatrix.get_err();
	return accVector.get_err();
}

template<class __TYPE>
mthMatrix<__TYPE> 
mthLeastSquares<__TYPE>::result() const {
	return accMatrix.equation(accVector);
}

template<class __TYPE>
__TYPE
mthLeastSquares<__TYPE>::estimate(mthMatrix<__TYPE> res)
{
mthMatrix<__TYPE> v;
	v = res.t() * accMatrix - accVector;
	return (v*v.t()).elm(0,0);
}


template class mthLeastSquares<float>;
template class mthLeastSquares<double>;
template class mthLeastSquares<long double>;
