

#include	"ts2/c++/stdBuffer.h"
#include	"ts2/c++/sWptr.h"

stdBuffer::stdBuffer()
{
	initial(thNULL,1,0,0,0,0);
}


stdBuffer::stdBuffer(
		sPtr<stdObject> _container,
		int		_writable,
		int8_t *	_avairableTop,
		int		_avairableSize,
		int		_dataHead,
		int		_dataSize
		)
{
	initial(
		_container,
		_writable,
		_avairableTop,
		_avairableSize,
		_dataHead,
		_dataSize
		);
}

void
stdBuffer::initial(
		sPtr<stdObject> _container,
		int		_writable,
		int8_t *	_avairableTop,
		int		_avairableSize,
		int		_dataHead,
		int		_dataSize
		)
{
	container = _container;
	writable = _writable;
	avairableTop = _avairableTop;
	avairableSize = _avairableSize;
	dataHead = _dataHead;
	dataSize = _dataSize;
	if ( avairableSize < 0 )
		stdObject::panic("stdBuffer::avairableSize");
	if ( avairableSize == 0 )
		return;
	if ( dataHead < 0 || dataHead >= avairableSize )
		stdObject::panic("stdBuffer::dataHead");
	if ( dataSize < 0 || dataSize >= avairableSize - dataHead )
		stdObject::panic("stdBuffer::dataSize");
}

stdBuffer::stdBuffer(sPtr<stdBuffer> inp)
{
	initial(
		inp->container,
		inp->writable,
		inp->avairableTop,
		inp->avairableSize,
		inp->dataHead,
		inp->dataSize
		);
	next = inp->next;
}


stdBuffer::stdBuffer(sPtr<stdArray<int8_t> > inp,int _writable)
{
	initial(
		inp,
		_writable,
		&inp->ary[0],
		inp->length(),
		0,0);
}

int
stdBuffer::copyout(int * erp,int ptr,int8_t * buf,int len)
{
int ret;
sPtr<stdBuffer> pb;
	pb = thThis;
	ptr = pb->dataHead + ptr;
	ret = 0;
	for ( ; len > 0 ; ) {
	int s;
		if ( pb->avairableTop == 0 ) {
			*erp = stdBF_ERR_REMAIN;
			return ret;
		}
		if ( ptr >= pb->dataHead + pb->dataSize ) {
			ptr = pb->dataHead;
			pb = pb->next;
			continue;
		}
		if ( ptr + len > pb->dataHead + pb->dataSize )
			s = pb->dataHead + pb->dataSize - ptr;
		else	s = len;
		memcpy(buf,&pb->avairableTop[ptr],s);
		len -= s;
		buf += s;
		ptr += s;
		ret += s;
	}
	*erp = stdBF_ERR_OK;
	return ret;
}


int
stdBuffer::copyin(int * erp,int ptr,int8_t * buf,int len,int arraySize)
{
int ret,s;
sPtr<stdBuffer> pb;
	pb = thThis;
	ptr = pb->dataHead + ptr;
	for ( ; len > 0 ; ) {
		if ( pb->avairableTop == 0 ) {
			extend(erp,arraySize);
			if ( *erp )
				return -1;
		}
		if ( ptr > pb->dataHead + pb->dataSize ) {
			*erp = stdBF_ERR_OVER_EPOINT;
			return -1;
		}
		if ( ptr == pb->dataHead + pb->dataSize ) {
			if ( pb->next != thNULL ) {
				pb = pb->next;
				ptr = pb->dataHead;
				continue;
			}
			if ( pb->dataHead + pb->dataSize 
					== pb->avairableSize ) {
				next = thNEW(stdBuffer,
					(thNEW(stdArray<int8_t>,
					(arraySize)),1));
				pb = pb->next;
				ptr = pb->dataHead;
				continue;
			}
			if ( ptr + len > pb->avairableSize )
				pb->dataSize = pb->avairableSize - pb->dataHead;
			else	pb->dataSize = ptr + len - pb->dataHead;
		}
		refresh(arraySize);
		if ( ptr + len > pb->dataHead + pb->dataSize )
			s = pb->dataHead + pb->dataSize - ptr;
		else	s = len;
		memcpy(&pb->avairableTop[ptr],buf,s);
		len -= s;
		buf += s;
		ptr += s;
		ret += s;
	}
	*erp = stdBF_ERR_OK;
	return ret;
}


int
stdBuffer::concat(sPtr<stdBuffer> bf)
{
sPtr<stdBuffer> pb;
	pb = thThis;
	for ( ; pb->next != thNULL ; pb = pb->next ) {
		if ( pb->avairableTop == 0 ) {
			initial(
				bf->container,
				bf->writable,
				bf->avairableTop,
				bf->avairableSize,
				bf->dataHead,
				bf->dataSize);
			pb->next = bf->next;
		}
	}
	pb->next = bf;
	return 0;
}

void
stdBuffer::refresh(int arraySize)
{
	if ( is_writable() )
		return;
	if ( dataSize <= arraySize ) {
	sPtr<stdArray<int8_t> > ary;
		ary = thNEW(stdArray<int8_t>,(arraySize));
		memcpy(&ary->ary[0],avairableTop+dataHead,dataSize);
		initial(
			ary,1,
			&ary->ary[0],
			ary->length(),
			0,
			dataSize);
	}
	else {
	sPtr<stdBuffer> n;
		n = thNEW(stdBuffer,(
				container,
				writable,
				avairableTop,
				avairableSize,
				dataHead + arraySize,
				dataSize - arraySize));
		n->next = next;
		next = n;
	sPtr<stdArray<int8_t> > ary;
		ary = thNEW(stdArray<int8_t>,(arraySize));
		memcpy(&ary->ary[0],avairableTop+dataHead,arraySize);
		initial(
			ary,1,
			&ary->ary[0],
			ary->length(),
			0,
			arraySize);
	}
}


sPtr<stdBuffer>
stdBuffer::shift(int * erp,sPtr<stdBuffer> * result,int * len)
{
sPtr<stdBuffer> ret;
sPtr<stdBuffer> * rp;
sPtr<stdBuffer> dummy;
	ret = thThis;
	if ( result ) {
		*result = thNULL;
		rp = result;
	}
	else {
		rp = &dummy;
	}
	for ( ; *len > 0 ; ) {
		if ( ret->avairableTop == 0 ) {
			*rp = thNULL;
			*erp = stdBF_ERR_REMAIN;
			return ret;
		}
		if ( ret == thNULL ) {
			*rp = thNULL;
			*erp = stdBF_ERR_OK;
			return ret;
		}
		if ( ret->dataSize <= *len ) {
			*len -= ret->dataSize;
			*rp = ret;
			ret = ret->next;
			rp = &(*rp)->next;
		}
		else {
			if ( result ) {
				*rp = thNEW(stdBuffer,
					(ret->container,
					ret->writable,
					ret->avairableTop,
					ret->avairableSize,
					ret->dataHead,
					*len));
				(*rp)->next = thNULL;
			}
			ret->dataSize -= *len;
			ret->dataHead += *len;
			*erp = stdBF_ERR_OK;
			return ret;
		}
	}
	*rp = thNULL;
	*erp = stdBF_ERR_OK;
	return ret;
}


sPtr<stdBuffer>
stdBuffer::unshift(int * erp,int len,int arraySize) 
{
int zeroFlag = 0;
sPtr<stdBuffer> ret;
	if ( len && this->getref() != 1 )
		zeroFlag = 1;
	for ( ret = thThis ; len ; ) {
	int s;
		if ( zeroFlag ) {
			if ( len > ret->dataHead )
				s = ret->dataHead;
			else	s = len;
			ret->dataHead -= s;
			ret->dataSize += s;
			len -= s;
			if ( len == 0 )
				break;
		}
	sPtr<stdBuffer> rr;
		rr = thNEW(stdBuffer,
				(thNEW(stdArray<int8_t>,(arraySize)),writable));
		rr->dataHead = rr->avairableSize;
		rr->dataSize = 0;
		rr->next = ret;
		ret = rr;
	}
	*erp = stdBF_ERR_OK;
	return ret;
}

int
stdBuffer::optimize(int arraySize)
{
	refresh(arraySize);
	if ( dataHead ) {
		memmove(avairableTop,avairableTop+dataHead,dataSize);
		dataHead = 0;
	}
	if ( next == thNULL )
		return 0;
int ret;
int err;
	ret = next->copyout(&err,0,avairableTop+dataSize,avairableSize - dataSize);
	dataSize += ret;
sPtr<stdBuffer> result;
	next = next->shift(&err,&result,&ret);
	return 0;
}

sPtr<stdBuffer>
stdBuffer::last(int * ptr)
{
sPtr<stdBuffer> ret;
	ret = thThis;
	for ( ; ret->next.is_notNull() ; ret = ret->next );
	*ptr = ret->dataSize;
	return ret;
}

void
stdBuffer::extend(int * erp,int arraySize)
{
	if ( avairableTop || next != thNULL) {
		*erp = stdBF_ERR_EXTEND;
		return;
	}
sPtr<stdArray<int8_t> > ary;
	ary = thNEW(stdArray<int8_t>,(arraySize));
	initial(ary,1,
		&ary->ary[0],
		ary->length(),
		0,0);
	next = thNEW(stdBuffer,());
	*erp = stdBF_ERR_OK;
}


int
stdBuffer::is_singlePointed(int len)
{
sWptr<stdBuffer> pb;
int length = 0;
int ret = 0;
	for ( pb = thThis ; length < len && pb != thNULL ; pb = pb->next ) {
		if ( pb->getref() != 1 )
			ret = length+pb->dataSize;
		length += pb->dataSize;
		pb = pb->next;
	}
	return ret;
}

sPtr<stdBuffer>
stdBuffer::dup(int len)
{
sPtr<stdBuffer> ret, * rp;
sPtr<stdBuffer> pb;
int length = 0;
	rp = &ret;
	for ( pb = thThis ; length < len && pb != thNULL ; pb = pb->next ) {
		*rp = thNEW(stdBuffer,(pb));
		rp = &(*rp)->next;
	}
	*rp = pb;
	return ret;
}


int
stdBuffer::cmp(int * erp,int pos,int8_t * buf,int * len)
{
	if ( pos >= dataSize ) {
		if ( next == thNULL ) {
			*erp = stdBF_ERR_OVER_EPOINT;
			return 0;
		}
		return next->cmp(erp,pos-dataSize,buf,len);
	}
sPtr<stdBuffer> pb;
	pb = thThis;
	for ( ; ; ) {
		if ( *len == 0 ) {
			*erp = stdBF_ERR_OK;
			return 0;
		}
		if ( pb == thNULL ) {
			*len = 0;
			*erp = stdBF_ERR_OK;
			return 1;
		}
		if ( pb->avairableTop == 0 ) {
			*erp = stdBF_ERR_REMAIN;
			return 0;
		}
	int s = *len;
	int ret;
		if ( pb->dataSize-pos < *len )
			s = dataSize-pos;
		ret = memcmp(pb->avairableTop+pb->dataHead+pos,buf,s);
		if ( ret ) {
			*len = 0;
			*erp = stdBF_ERR_OK;
			return ret;
		}
		*len -= s;
		buf += s;
		pos = 0;
		pb = pb->next;
	}
}


int 
stdBuffer::delimiter(int * erp,int * hit,int8_t ** dlmList,int * lenList,int max)
{
int ret = 0;
int pos = 0;
sPtr<stdBuffer> pb;
	pb = thThis;
	for ( ; pb.is_notNull() && ret < max ; ) {
		if ( pb->avairableTop == 0 ) {
			*erp = stdBF_ERR_REMAIN;
			return -1;
		}
		if ( pos >= pb->dataSize ) {
			pos = 0;
			pb = pb->next;
			continue;
		}
	int i;
		for ( i = 0 ; dlmList[i] ; i ++ ) {
		int len;
			len = lenList[i];
			if ( pb->cmp(erp,pos,dlmList[i],&len) == 0 ) {
				if ( *erp )
					return -1;
				*hit = i;
				*erp = stdBF_ERR_OK;
				return ret;
			}
		}
		pos ++;
		ret ++;
	}
	*hit = -1;
	if ( ret == max )
		*erp = stdBF_ERR_TOO_LONG;
	else	*erp = stdBF_ERR_OK;
	return ret;
}


int
stdBuffer::length()
{
int ret;
	ret = 0;
sPtr<stdBuffer> pb = thThis;
	for ( ; pb.is_notNull() ; pb = pb->next )
		ret += pb->dataSize;
	return ret;
}
