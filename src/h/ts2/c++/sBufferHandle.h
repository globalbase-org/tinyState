

#ifndef ___sBufferHandle_cpp_H___
#define ___sBufferHandle_cpp_H___

#include	"ts2/c++/stdBuffer.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/tinyState.h"

class sBufferHandle : public sObject {
public:
	sBufferHandle(int arraySize);
	sBufferHandle(sBufferHandle& inp);
	sBufferHandle(sPtr<stdBuffer>,int arraySize);
	sBufferHandle();
	~sBufferHandle();
	int append(int *erp,int8_t * buf,int len);
	int append(int *erp,sBufferHandle& bf);
	int append(int *erp,sPtr<stdArray<int8_t> > ar);
	int shift(int * erp,int8_t * buf,int len);
	sBufferHandle shift(int * erp,int *len);
	int unshift(int * erp,int8_t * buf,int len);
	sPtr<stdString> shift(int * erp,int * hit,int8_t ** dlmList,int * lenList,int max);
	sPtr<stdString> shift(int * erp,int * hit,sPtr<stdString> * dlmList,int max);
	int unshift(int * erp,sPtr<stdString> str);
	int append(int * erp,sPtr<stdString> str);
	int length();
protected:
	void actionRemain();
	sPtr<stdQueue<tinyState> > invokeList();
	void invoke(sPtr<stdQueue<tinyState> > q);

	sPtr<stdBuffer>		b;
	int			mode;
#define stdBH_MODE_READ		1
#define stdBH_MODE_APPEND	2
#define stdBH_MODE_INDIV	3
#define stdBH_MODE_ERROR	4
	int			arraySize;
	unsigned		wait_flag:1;

	sPtr<tinyState>		caller;
	sThreadMutex		m;
	sBufferHandle *		hList;
	sBufferHandle *		hListNext;
	sBufferHandle *		hListRoot;
};



#endif
