

#include	"ts2/c++/sEndian.h"


sEndian
sEndian::cpu;

void
sEndian::swap(int8_t * buf,int len)
{
int8_t * p1, * p2;
	p1 = buf;
	p2 = &buf[len-1];
	for ( ; p1 < p2 ; p1 ++ , p2 -- ) {
	int8_t tmp = *p1;
		*p1 = *p2;
		*p2 = tmp;
	}
}

INTEGER64
sEndian::conv(int8_t * buf,int len,int endian)
{
uint8_t _buf[8];
	if ( len >= 8 )
		sObject::panic("length over");
	memcpy(_buf,buf,len);
	if ( endian == ENDIAN_BIG )
		swap((int8_t*)_buf,len);
INTEGER64 ret = 0;
int i;
	for ( i = 0 ; i < len ;i ++ )
		ret |= ((INTEGER64)_buf[i])<<(i*8);
	return ret;
}

void 
sEndian::conv(int8_t * buf,INTEGER64 inp,int len,int endian)
{
	if ( len >= 8 )
		sObject::panic("length over");
int i;
	if ( endian == ENDIAN_BIG ) {
		for ( i = len-1 ; i >= 0 ; i -- )
			*buf++ = (inp>>(8*i))&0xff;
	}
	else {
		for ( i = 0 ; i < len ; i ++ )
			*buf++ = (inp>>(8*i))&0xff;
	}
}
