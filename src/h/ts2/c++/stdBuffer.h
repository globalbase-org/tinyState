

#ifndef ___stdBuffer_cpp_H___
#define ___stdBuffer_cpp_H___


#include	<functional>
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sPtr.h"
#include	"ts2/c++/stdArray.h"


#define stdBF_ERR_OK		0
#define stdBF_ERR_REMAIN	(-1)
#define stdBF_ERR_OVER_EPOINT	(-2)
#define stdBF_ERR_EXTEND	(-3)
#define stdBF_ERR_TOO_LONG	(-4)
#define stdBF_ERR_INV		(-5)

class stdBuffer : public stdObject {
public:
  	stdBuffer();
	stdBuffer(
		sPtr<stdObject> container,
		int		writable,
		int8_t *	avairableTop,
		int		avairableSize,
		int		dataHead,
		int		dataSize
		);
	stdBuffer(sPtr<stdBuffer> inp);
	stdBuffer(sPtr<stdArray<int8_t> > inp,int writable);
	void initial(
		sPtr<stdObject> container,
		int		writable,
		int8_t *	avairableTop,
		int		avairableSize,
		int		dataHead,
		int		dataSize
		);
	
	sPtr<stdBuffer>		next;
	int is_writable() {
		if ( writable == 0 )
			return 0;
		if ( container == thNULL )
			return 0;
		if ( container->getref() == 1 )
			return 1;
		return 0;
	}
	int is_singlePointed(int len);
	sPtr<stdBuffer> dup(int len);
	int copyin(int * erp,int ptr,int8_t * buf,int len,int arraySize);
	int copyout(int * erp,int ptr,int8_t * buf,int len);
	sPtr<stdBuffer> shift(int * erp,sPtr<stdBuffer> * result,int * len);
	sPtr<stdBuffer> copyOnShift(int * len);
	void refresh(int arraySize);
	int optimize(int arraySize);
	sPtr<stdBuffer> last(int * ptr);
	void extend(int * erp,int arraySize);
	sPtr<stdBuffer> unshift(int * erp,int len,int arraySize);
	int cmp(int * erp,int pos,int8_t * buf,int *len);
	int delimiter(int * erp,int * hit,int8_t ** dlmList,int * lenList,int max);
	int concat(sPtr<stdBuffer> bf);
	int length();

protected:

	sPtr<stdObject>		container;
	unsigned		writable:1;
	int8_t *		avairableTop;
	int			avairableSize;
	int			dataHead;
	int			dataSize;
};

#endif

