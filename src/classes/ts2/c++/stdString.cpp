

#include	<string.h>
#include	<stdio.h>
#include	"ts2/c++/stdString.h"


stdString::stdString(const char * str)
	:
	stdArray<char>(0)
{
	if ( str == 0 )
		return;
	this->length(strlen(str)+1);
	strcpy(&this->ary[0],str);

}

stdString::stdString(sPtr<stdString>  s)
	:
	stdArray<char>(s.is_notNull()? s : thNEW( stdString,(0)))
{
}

stdString::stdString(int len)
	:
	stdArray<char>(len+1)
{
}

stdString::stdString(const char * fmt,int data)
	:
	stdArray<char>(0)
{
char buffer[50];
int len;
	if ( strlen(fmt) > 50 )
		stdObject::panic("invalid fmt");
	snprintf(buffer,sizeof(buffer),fmt,data);
	length(len=strlen(buffer));
	memcpy(&this->ary[0],buffer,len+1);

}

stdString::stdString(const char * fmt,INTEGER64 data)
	:
	stdArray<char>(0)
{
char buffer[50];
int len;
	if ( strlen(fmt) > 50 )
		stdObject::panic("invalid fmt");
	snprintf(buffer,sizeof(buffer),fmt,data);
	length(len=strlen(buffer));
	memcpy(&this->ary[0],buffer,len+1);

}

stdString::stdString(const char * fmt,double data)
	:
	stdArray<char>(0)
{
char buffer[100];
int len;
	if ( strlen(fmt) > 100 )
		stdObject::panic("invalid fmt");
	snprintf(buffer,sizeof(buffer),fmt,data);
	length(len=strlen(buffer));
	memcpy(&this->ary[0],buffer,len+1);

}

stdString::stdString(const char * fmt,void * data)
	:
	stdArray<char>(0)
{
char buffer[50];
int len;
	if ( strlen(fmt) > 50 )
		stdObject::panic("invalid fmt");
	snprintf(buffer,sizeof(buffer),fmt,data);
	length(len=strlen(buffer));
	memcpy(&this->ary[0],buffer,len+1);

}

stdString::stdString(char ch)
	:
	stdArray<char>(2)
{
	this->ary[0] = ch;
	this->ary[1] = 0;

}

stdString::stdString()
	:
	stdArray<char>(1)
{

}

stdString::stdString(const char * str,int sp,int ep)
	:
	stdArray<char>(1)
{
int len;
	for ( len = 0 ; str[len] && len < ep ; len ++ );
	if ( len >= ep ) {
		if ( ep <= sp ) {
			length(0);
			return;
		}
		length(ep-sp);
		memcpy(&ary[0],&str[sp],ep-sp);
		ary[ep-sp] = 0;
	}
	else {
		if ( len <= sp ) {
			length(0);
			return;
		}
		length(len-sp);
		memcpy(&ary[0],&str[sp],len-sp);
		ary[len-sp] = 0;
	}
}


stdString::stdString(sPtr<stdString>  str,int sp,int ep)
	:
	stdArray<char>(1)
{
sPtr<stdString>  pt;
	pt = thNEW( stdString,(str->get_str(),sp,ep));
	length(pt->length());
	strcpy(&ary[0],pt->get_str());
}

stdString::stdString(sPtr<stdString>  str,int sp)
	:
	stdArray<char>(1)
{
sPtr<stdString>  pt;
	pt = thNEW( stdString,(str->get_str(),sp,0x7fffffff));
	length(pt->length());
	strcpy(&ary[0],pt->get_str());
}

int
stdString::length()
{
	return strlen(&ary[0]);
}

void
stdString::length(int len)
{
	if ( len < 0 ) {
		this->stdArray<char>::length(-1);
		return;
	}
	this->stdArray<char>::length(len+1);
	this->ary[len] = 0;
}

sPtr<stdString> 
stdString::add(sPtr<stdString>  str)
{
int tlen;
sPtr<stdString>  ret;
	ret = thNEW( stdString,(thThis));
	if ( str == thNULL )
		return ret;
	tlen = this->length();
	ret->stdArray<char>::length(tlen + str->length() + 1);
	strcpy(&ret->ary[tlen],&str->ary[0]);
	return ret;
}

void
stdString::addIn(sPtr<stdString>  str)
{
int tlen;
sPtr<stdString>  ret;
	tlen = this->length();
	this->stdArray<char>::length(tlen + str->length() + 1);
	strcpy(&this->ary[tlen],&str->ary[0]);
}

int
stdString::cmp(sPtr<stdString>  str)
{
	if ( str == thNULL ) {
		if ( this->ary[0] == 0 )
			return 0;
		return 1;
	}
	return strcmp(&this->ary[0],&str->ary[0]);
}

int
stdString::cmp(const char * str)
{
	if ( str == 0 ) {
		if ( this->ary[0] == 0 )
			return 0;
		return -1;
	}
	return strcmp(&this->ary[0],str);
}

sPtr<stdString> 
stdString::to_lower()
{
sPtr<stdString>  ret;
char * ptr;
	ret = thNEW( stdString,(thThis));
	ptr = &ret->ary[0];
	for ( ; *ptr ; ptr ++ )
		if ( 'A' <= *ptr && *ptr <= 'Z' )
			*ptr += 'a' - 'A';
	return ret;
}

sPtr<stdString> 
stdString::to_upper()
{
sPtr<stdString>  ret;
char * ptr;
	ret = thNEW( stdString,(thThis));
	ptr = &ret->ary[0];
	for ( ; *ptr ; ptr ++  )
		if ( 'a' <= *ptr && *ptr <= 'z' )
			*ptr += 'A' - 'a';
	return ret;
}

void
stdString::copyout(char * dest,int size)
{
	if ( length() >= size )
		memcpy(dest,&this->ary[0],size);
	else
		strcpy(dest,&this->ary[0]);
}

void
stdString::copyin(char * dest,int size)
{
int len;
	for ( len = 0 ; len < size && dest[len] ; len ++ ) {}
	if ( len < size ) {
		length(len);
		strcpy(&this->ary[0],dest);		
	}
	else {
		length(size);
		memcpy(&this->ary[0],dest,size);
		this->ary[size] = 0;
	}
}

int
stdString::partial_cmp(const char * str)
{
	if ( length() > strlen(str) )
		return -1;
	if ( strcmp(&this->ary[0],str) == 0 )
		return 0;
	if ( memcmp(&this->ary[0],str,length()) == 0 )
		return 1;
	return -1;
}

int
stdString::partial_cmp(sPtr<stdString>  str)
{
	return partial_cmp(str->get_str());
}

sPtr<stdString> 
stdString::percent_encode()
{
sPtr<stdArray<char> > buffer;
char * p1, * p2; 
unsigned char ch;
char digit[] = "0123456789ABCDEF";
	buffer = thNEW( stdArray<char>,(length()*3+1));
	p1 = &this->ary[0];
	p2 = &buffer->ary[0];
	for ( ; *p1 ; p1 ++ ) {
		ch = (unsigned char)(*p1);
		if ( ('0' <= ch && ch <= '9') ||
				('a' <= ch && ch <= 'z') ||
				('A' <= ch && ch <= 'Z') ||
				ch == '-' ||
				ch == '.' ||
				ch == '_' ||
				ch == '~' ) {
			*p2 ++ = (char)ch;
			continue;
		}
		*p2 ++ = '%';
		*p2 ++ = digit[(ch>>4)&0xf];
		*p2 ++ = digit[ch&0xf];
	}
	*p2 = 0;
	return thNEW( stdString,(&buffer->ary[0]));
}

sPtr<stdString> 
stdString::percent_decode()
{
sPtr<stdArray<char> > buffer;
char * p1, * p2; 
char ch;
unsigned char result;

	buffer = thNEW( stdArray<char>,(length()+1));
	p1 = &this->ary[0];
	p2 = &buffer->ary[0];
	for ( ; *p1 ; ) {
		switch ( *p1 ) {
		case '%':
			p1 ++;
			break;
		default:
			*p2 ++ = *p1 ++;
			continue;
		}
		ch = *p1 ++;
		result = 0;
		if ( '0' <= ch && ch <= '9' )
			result = ch-'0';
		else if ( 'a' <= ch && ch <= 'f' )
			result = ch-'a'+10;
		else if ( 'A' <= ch && ch <= 'F' )
			result = ch-'A'+10;
		else {
			*p2 ++ = ch;
			continue;
		}
		ch = *p1 ++;
		result = (result << 4)&0xf0;
		if ( '0' <= ch && ch <= '9' )
			result += ch-'0';
		else if ( 'a' <= ch && ch <= 'f' )
			result += ch-'a'+10;
		else if ( 'A' <= ch && ch <= 'F' )
			result += ch-'A'+10;
		else {
			*p2 ++ = ch;
			continue;
		}
		*p2 ++ = (char)result;
	}
	*p2 = 0;
	return thNEW( stdString,(&buffer->ary[0]));
}


sPtr<stdString> 
stdString::rx(
	const char * method,
	sPtr<stdRx>  re,
	sPtr<stdString>  rstr)
{
int status;
sPtr<stdString>  str;
int i;
sPtr<stdRx>  rx;

	rx_work = thNULL;
	switch ( method[0] ) {
	case 0:
	zero:
		if ( regexec(&re->re,get_str(),0,NULL,0) == 0 )
			return thNEW( stdString,("true"));
		return thNULL;
	case 'm':
		switch ( method[1] ) {
		case 0:
			goto zero;
		case 'v':
			rx_work = thNEW( rxWork,());
			rx_work->match = 
				(thNEW( stdArray<regmatch_t>,(re->numpunc)));
			status = regexec(&re->re,get_str(),
				re->numpunc,
				&rx_work->match->ary[0],
				0);
			if ( status ) {
				rx_work = thNULL;
				return thNULL;
			}
			rx_work->values = (thNEW( stdArray<sPtr<stdString> >,(re->numpunc+1)));
			for ( i = 0 ; i < re->numpunc ; i ++ ) {
				rx_work->values->ary[i] = 
					thNEW( stdString,(get_str(),
						rx_work->match->ary[i].rm_so,
						rx_work->match->ary[i].rm_eo));
			}
			rx_work->values->ary[re->numpunc] = 
				thNEW( stdString,(get_str(),
					rx_work->match->ary[0].rm_eo,0x7fffffff));
			return thNEW( stdString,("true"));
		}
		stdObject::panic("rx invalid(1)");
	case 's':
		switch ( method[1] ) {
		case 0:
			str = this->rx("sx",re,rstr);
			length(str->length());
			strcpy(&ary[0],&str->ary[0]);
			if ( rx_work.is_notNull() )
				return thThis;
			return thNULL;
		case 'x':
			rx = thNEW( stdRx,("\\$([0-9]+)|\\$\\{([0-9]+)\\}"));
			if ( !rstr->rx("m",rx).is_notNull() ) {
				if ( !this->rx("mv",re).is_notNull() )
					return thNEW( stdString,(this));
				return (thNEW( stdString,(thThis,
						0,
						rx_work->match->ary[0].rm_so)))
					->add(rstr)
					->add(thNEW( stdString,(thThis,
						rx_work->match->ary[0].rm_eo)));
			}
			if ( !this->rx("mv",re).is_notNull() )
				return thNEW( stdString,(this));
			rx = thNEW( stdRx,("([^\\$]*)(\\$([0-9]+)|\\$\\{([0-9]+)\\})"));
			{
			sPtr<stdString>  rep;
				rep = thNEW( stdString,());
				for ( ; rstr->rx("mv",rx).is_notNull() ; ) {
					if ( STR_NULL(rstr->rxVar(3))->cmp("") )
						rep = rep->add(rstr->rxVar(1))
							->add(this->rxVar(rstr->rxVar(3)->get_int()));
					else if ( STR_NULL(rstr->rxVar(4))->cmp("") )
						rep = rep->add(rstr->rxVar(1))
							->add(this->rxVar(rstr->rxVar(4)->get_int()));
					else
						rep = rep->add(rstr->rxVar(1))
							->add(this->rxVar(1));
					rstr = rstr->rxNext();
				}
				rep = rep->add(rstr);
				return this->rx("sx",re,rep);
			}
		case 'g':
			if ( method[2] == 'x' ) {
			sPtr<stdString>  inp;
			sPtr<stdString>  res;
			sPtr<stdString>  m;
			int pc_flag = 0;
				inp = thNEW( stdString,(thThis));
				res = thNEW( stdString,(""));
				for ( ; inp->rx("mv",re).is_notNull() ; ) {
					if ( rx_work == thNULL )
						rx_work = thNEW( rxWork,());
					res = res->add(inp->rxPrev())
						->add(inp->rxVar(0)->rx("sx",re,rstr));
					inp = inp->rxNext();
					if ( inp->cmp("") == 0 )
						break;
					if ( pc_flag == 0 ) {
					sPtr<stdString>  p = re->pattern();
					char * pp;
					int count = 0;
						pc_flag = 1;
						for ( pp = (char*)p->get_str() ; *pp ; pp ++ ) {
							if ( *pp == '^' )
								count ++;
						}
						if ( count ) {
							p = thNEW( stdString,(p));
							p->length(p->length()+count);
							pp = (char*)p->get_str();
							if ( *pp == '^' ) {
								memmove(pp+1,pp,strlen(pp));
								*pp = 'a';
								pp += 2;
							}
							for ( ; *pp ; pp ++ ) {
								if ( *pp != '^' )
									continue;
								if ( *(pp-1) != '|' && *(pp-1) != '(' )
									continue;
								memmove(pp+1,pp,strlen(pp));
								*pp = 'a';
								pp ++;
							}
							re = thNEW( stdRx,(p));
						}
					}
				}
				res = res->add(inp);
				return res;
			}
			sPtr<stdString>  str = this->rx("sgx",re,rstr);
			length(str->length());
			strcpy(&ary[0],&str->ary[0]);
			if ( rx_work.is_notNull() )
				return thThis;
			return thNULL;
		}
		stdObject::panic("rx invalid(2)");
	default:
		stdObject::panic("rx invalid(3)");
	}
	return thNULL;
}

sPtr<stdString> 
stdString::rx(
	const char * method,
	sPtr<stdRx>  re,
	const char * rstr)
{

	return rx(method,
		re,
		thNEW( stdString,(rstr)));
}

sPtr<stdString> 
stdString::rxVar(int no)
{
	if ( no < 0 )
		return thNULL;
	if ( rx_work == thNULL )
		return thNULL;
	if ( no >= rx_work->values->length()-1 )
		return thNULL;
	return rx_work->values->ary[no];
}

sPtr<stdString> 
stdString::rxNext()
{
	if ( rx_work == thNULL )
		return thNULL;
	return rx_work->values->ary[rx_work->values->length()-1];
}


sPtr<stdString> 
stdString::rxPrev()
{
	if ( rx_work == thNULL )
		return thNULL;
	return thNEW( stdString,(thThis,0,rx_work->match->ary[0].rm_so));
}

