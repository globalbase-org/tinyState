

#include	"ts2/c++/sBufferHandle.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/tsThread.h"

sBufferHandle::sBufferHandle()
{
	mode = stdBH_MODE_ERROR;
	this->hList = 0;
	this->hListNext = 0;
	this->hListRoot = this;
	arraySize = 0;
}

sBufferHandle::sBufferHandle(int arraySize)
{
  	this->mode = stdBH_MODE_APPEND;
	this->arraySize = arraySize;
	b = thNEW(stdBuffer,());
	this->hList = 0;
	this->hListNext = 0;
	this->hListRoot = this;
}


sBufferHandle::sBufferHandle(sBufferHandle& inp)
{
sThreadMutexHandle __hdr(inp.m);
	this->mode = stdBH_MODE_APPEND;
	this->arraySize = inp.arraySize;
	b = inp.b;

	this->hList = 0;
	this->hListRoot = inp.hListRoot;
	this->hListNext = this->hListRoot->hListNext;
	this->hListRoot->hListNext = this;
}

sBufferHandle::sBufferHandle(sPtr<stdBuffer> bf,int arraySize)
{
  	this->mode = stdBH_MODE_INDIV;
	this->arraySize = arraySize;
	b = bf;

	this->hList = 0;
	this->hListRoot = this;
	this->hListNext = 0;
}

sBufferHandle::~sBufferHandle()
{
	switch ( mode ) {
	case stdBH_MODE_READ:
		{
		sBufferHandle ** p;
		sThreadMutexHandle __hdr(m);
		
			for ( p = &this->hListRoot->hList;
				*p == this ; p = &(*p)->hListNext );
			*p = hListNext;
		}
		break;
	case stdBH_MODE_APPEND:
		{
		sBufferHandle * p, * q;
		sThreadMutexHandle __hdr(hListRoot->m);
			for ( p = hList ; p ; ) {
				q = p->hListNext;
				p->mode = stdBH_MODE_INDIV;
				p->hList = p->hListNext = 0;
				p->hListRoot = p;
			}
		}
		break;
	case stdBH_MODE_INDIV:
	case stdBH_MODE_ERROR:
		break;
	default:
		sObject::panic("invalid mode");
	}
}

sPtr<stdQueue<tinyState> > 
sBufferHandle::invokeList()
{
sBufferHandle * p;
sPtr<stdQueue<tinyState> > q;
	q = thNEW(stdQueue<tinyState>,());
	for ( p = hList ; p ; p = p->hListNext ) {
		if ( p->wait_flag == 0 )
			continue;
		p->wait_flag = 0;
		if ( caller.is_notNull() )
			q->ins(MAX_INTEGER64,caller);
		caller = thNULL;
	}
	return q;
}

void
sBufferHandle::invoke(sPtr<stdQueue<tinyState> > q)
{
sPtr<tinyState> ts;
	for ( ; (ts = q->del()).is_notNull() ; )
		ts->eventHandler(thNEW(stdEvent,(TSE_UPDATED,
			sCallSection::key->caller(),
			thNULL)));
}

int
sBufferHandle::append(int * erp,int8_t * buf,int len)
{
int ret;
sPtr<stdQueue<tinyState> > q;
	if ( mode != stdBH_MODE_APPEND ) {
		*erp = stdBF_ERR_INV;
		return -1;
	}
	{
	int ptr;
	sPtr<stdBuffer> bb;
	sThreadMutexHandle __hdr(hListRoot->m);
		bb = b->last(&ptr);
		ret = bb->copyin(erp,ptr,buf,len,arraySize);
		if ( *erp )
			return -1;
		if ( mode != stdBH_MODE_APPEND )
			return ret;
		b = bb->last(&ptr);
		q = invokeList();
	}
	invoke(q);
	*erp = stdBF_ERR_OK;
	return ret;
}

int
sBufferHandle::append(int * erp,sBufferHandle& bf)
{
int ret;
sPtr<stdQueue<tinyState> > q;
	if ( bf.mode != stdBH_MODE_INDIV ) {
		*erp = stdBF_ERR_INV;
		return -1;
	}
	ret = bf.length();
	{
	sThreadMutexHandle __hdr(hListRoot->m);
	sPtr<stdBuffer> bb;
	int ptr;
		bb = b->last(&ptr);
		bb->next = bf.b;
		if ( mode != stdBH_MODE_APPEND )
			return ret;
		b = bb->last(&ptr);
		q = invokeList();
	}
	invoke(q);
	*erp = stdBF_ERR_OK;
	return ret;
}

int
sBufferHandle::append(int * erp,sPtr<stdArray<int8_t> > ar)
{
sBufferHandle bf(thNEW(stdBuffer,(ar,1)),ar->length());
	return append(erp,bf);
}

void
sBufferHandle::actionRemain()
{
	wait_flag = 1;
	caller = sCallSection::key->caller();
	throw sException([this](sPtr<tinyState> _caller){
		sThreadMutexHandle __hdr(hListRoot->m);
			if ( wait_flag == 0 )
				return 1;
			return 0;
		});
}

int
sBufferHandle::shift(int * erp,int8_t * buf,int len)
{
sThreadMutexHandle __hdr(hListRoot->m);
int ret;
sPtr<stdBuffer> result;
	if ( !b->is_singlePointed(len) )
		b = b->dup(len);
	ret = b->copyout(erp,0,buf,len);
	switch ( *erp ) {
	case stdBF_ERR_REMAIN:
		actionRemain();
		return -1;
	case stdBF_ERR_OK:
		b = b->shift(erp,0,&len);
		return ret;
	default:
		return -1;
	}
}

sBufferHandle
sBufferHandle::shift(int * erp,int *len)
{
int _len = *len;
sThreadMutexHandle __hdr(hListRoot->m);
sPtr<stdBuffer> result,bb;
	bb = b->shift(erp,&result,&_len);
	switch ( *erp ) {
	case stdBF_ERR_REMAIN:
		actionRemain();
		return sBufferHandle();
	case stdBF_ERR_OK:
		bb = b;
		return sBufferHandle(result,arraySize);
	default:
		return sBufferHandle();
	}
}


sPtr<stdString>
sBufferHandle::shift(int * erp,int * hit,int8_t ** dlmList,int * lenList,int max)
{
int clen;
	{
	sThreadMutexHandle __hdr(hListRoot->m);
		clen = b->delimiter(erp,hit,dlmList,lenList,max);
		switch ( *erp ) {
		case stdBF_ERR_OK:
			break;
		case stdBF_ERR_REMAIN:
			actionRemain();
			return thNULL;
		default:
			return thNULL;
		}
	}
sPtr<stdString> ret;
	ret = thNEW(stdString,(clen));
	shift(erp,(int8_t*)ret->get_str(),clen);
	return ret;
}

sPtr<stdString>
sBufferHandle::shift(int * erp,int * hit,sPtr<stdString> * dlmList,int max)
{
sArray<int8_t*> _dlmList;
sArray<int> _lenList;
sArray<int> _shaffle;
int i,j;
int ll = 0;
int len;
	for ( i = 0 ; dlmList[i].is_notNull() ; i ++ ) {
		if ( ll < dlmList[i]->length() )
			ll = dlmList[i]->length();
	}
	len = i;
	_dlmList.length(len+1);
	_lenList.length(len+1);
	_shaffle.length(len);
	j = 0;
	for ( ; ll >= 0 ; ll -- )
		for ( i = 0 ; dlmList[i].is_notNull() ; i ++ ) {
			if ( dlmList[i]->length() != ll )
				continue;
			_dlmList[j] = (int8_t*)dlmList[i]->get_str();
			if ( ll == 0 )
				_lenList[j] = 1;
			else	_lenList[j] = ll;
			_shaffle[j] = i;
			j ++;
		}
sPtr<stdString> ret;
	ret = shift(erp,hit,&_dlmList[0],&_lenList[0],max);
	if ( *erp )
		return ret;
	*hit = _shaffle[*hit];
	return ret;
}


int
sBufferHandle::unshift(int * erp,int8_t* buf,int len)
{
sThreadMutexHandle __hdr(hListRoot->m);
	b = b->unshift(erp,len,arraySize);
	if ( *erp )
		return -1;
	return b->copyin(erp,0,buf,len,arraySize);
}

int
sBufferHandle::unshift(int * erp,sPtr<stdString> str)
{
	if ( str == thNULL ) {
		*erp = stdBF_ERR_OK;
		return 0;
	}
	return unshift(erp,(int8_t*)str->get_str(),str->length());
}

int
sBufferHandle::append(int * erp,sPtr<stdString> str)
{
	if ( str == thNULL ) {
		*erp = stdBF_ERR_OK;
		return 0;
	}
	return append(erp,(int8_t*)str->get_str(),str->length());
}


int
sBufferHandle::length()
{
	return b->length();
}
