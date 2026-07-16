

#include	"ts2/c++/stdStringTree.h"


stdStringTreeBase::stdStringTreeBase()
{
}

stdStringTreeBase::stdStringTreeBase(
	sPtr<stdString>  str,
	sPtr<stdObject>  obj,
	int partial_flag,
	sPtr<stdStringTreeBase>  list)
{
	this->partial_flag = partial_flag;
	this->str = str;
	this->obj = obj;
	this->list = list;
}

stdStringTreeBase::~stdStringTreeBase()
{
	obj = thNULL;
	list = thNULL;
	next = thNULL;
	str = thNULL;
}


void
stdStringTreeBase::reset(
	int len,
	sPtr<stdObject>  obj,
	int partial_flag,
	sPtr<stdStringTreeBase>  list)
{
	str->length(len);
	this->list = list;
	this->obj = obj;
	this->partial_flag = partial_flag;
}

void
stdStringTreeBase::count(int * accp)
{
sPtr<stdStringTreeBase>  elp;
	if ( obj.is_notNull() )
		(*accp) ++;
	for ( elp = list ; elp.is_notNull() ; elp = elp->next )
		elp->count(accp);
}

void
stdStringTreeBase::match(int * clen_p,int * xlen_p,sPtr<stdStringTreeBase>  elp,const char * ix,int len)
{
int xlen = elp->str->length();
int clen;
const char * p1, * p2;
	clen = 0;
	p1 = ix;
	p2 = elp->str->get_str();
	for ( ; clen < xlen && clen < len ; clen ++ , p1 ++ , p2 ++ ) {
		if ( *p1 != *p2 )
			break;
	}
	*clen_p = clen;
	*xlen_p = xlen;
	return;
}


void
stdStringTreeBase::ins(const char * ix,int len,sPtr<stdObject>  obj,int partial_flag)
{
sPtr<stdStringTreeBase>  elp;

	if ( ix[0] == 0 || len == 0 ) {
		this->obj = obj;
		this->partial_flag = partial_flag;
		return;
	}
	for ( elp = list ; elp.is_notNull() ; elp = elp->next ) {
	int clen,xlen;
		match(&clen,&xlen,elp,ix,len);
		if ( clen == 0 )
			continue;
		if ( clen == xlen ) {
			elp->ins(&ix[clen],len-clen,obj,partial_flag);
			return;
		}
		if ( clen == len ) {
		sPtr<stdStringTreeBase>  t;
			t = thNEW( stdStringTreeBase,(
				thNEW( stdString,(elp->str->get_str(),clen,xlen)),
				elp->obj,
				elp->partial_flag,
				elp->list));
			elp->reset(clen,obj,partial_flag,t);
			return;
		}
		else {
		sPtr<stdStringTreeBase>  t1, t2;
			t1 = thNEW( stdStringTreeBase,(
				thNEW( stdString,(elp->str->get_str(),clen,xlen)),
				elp->obj,
				elp->partial_flag,
				elp->list));
			elp->reset(clen,thNULL,0,t1);

			t2 = thNEW( stdStringTreeBase,(
				thNEW( stdString,(ix,clen,len)),
				obj,
				partial_flag,
				thNULL));
			t1->next = t2;
			return;
		}
	}
sPtr<stdStringTreeBase>  t1;
	t1 = thNEW( stdStringTreeBase,(
		thNEW( stdString,(ix,0,len)),
		obj,
		partial_flag,
		thNULL));
	t1->next = list;
	list = t1;
}

void
stdStringTreeBase::ins(const char * str,sPtr<stdObject>  obj,int partial_flag)
{
	ins(str,strlen(str),obj,partial_flag);
}

void
stdStringTreeBase::ins(sPtr<stdString>  key,sPtr<stdObject>  obj,int partial_flag)
{
	ins(key->get_str(),key->length(),obj,partial_flag);
}

sPtr<stdObject>  
stdStringTreeBase::search(const char * key,int len)
{
sPtr<stdStringTreeBase>  k;
	if ( len == 0 || key[0] == 0 )
		return this->obj;
	for ( k = list ; k.is_notNull() ; k = k->next ) {
	int clen,xlen;
		match(&clen,&xlen,k,key,len);
		if ( clen == 0 )
			continue;
		if ( clen == xlen )
			return k->search(&key[clen],len-clen);
		return thNULL;
	}
	return thNULL;
}

sPtr<stdObject>  
stdStringTreeBase::search(const char * key,int len,sPtr<stdObject>  olast,const char ** m_ptr)
{
sPtr<stdStringTreeBase>  k;

	if ( this->obj.is_notNull() ) {
		if ( len == 0 || key[0] == 0 ) {
			*m_ptr = key;
			return this->obj;
		}
		if ( this->partial_flag ) {
			olast = this->obj;
			*m_ptr = key;
		}
	}
	if ( len == 0 || key[0] == 0 )
		return olast;
	for ( k = list ; k.is_notNull() ; k = k->next ) {
	int clen,xlen;
		match(&clen,&xlen,k,key,len);
		if ( clen == 0 )
			continue;
		if ( clen == xlen )
			return k->search(&key[clen],len-clen,olast,m_ptr);
		return olast;
	}
	return olast;
}

sPtr<stdString> 
stdStringTreeBase::search(sPtr<stdObject>  obj)
{
sPtr<stdStringTreeBase>  elp;

	if ( this->obj == obj ) {
		return str;
	}
	for ( elp = list ; elp.is_notNull() ; elp = elp->next ) {
	sPtr<stdString>  ret;
		ret = elp->search(obj);
		if ( ret.is_notNull() ) {
			if ( str.is_notNull() )
				return str->add(ret);
			return ret;
		}
	}
	return thNULL;
}

sPtr<stdObject>  
stdStringTreeBase::search(
	sPtr<stdObject>  msg,
	int (*cmp)(
		sPtr<stdObject>  d1,
		sPtr<stdObject>  d2))
{
sPtr<stdStringTreeBase>  elp;
	if ( obj.is_notNull() && (*cmp)(obj,msg) )
		return obj;
	for ( elp = list ; elp.is_notNull() ; elp = elp->next ) {
	sPtr<stdObject>  ret;
		ret = elp->search(msg,cmp);
		if ( ret.is_notNull() )
			return ret;
	}
	return thNULL;
}


sPtr<stdObject> 
stdStringTreeBase::search(const char * key)
{
	return search(key,strlen(key));
}

sPtr<stdObject> 
stdStringTreeBase::search(sPtr<stdString>  key)
{
	return search(key->get_str(),key->length());
}

sPtr<stdObject> 
stdStringTreeBase::search(int * match_len,const char * key)
{
const char * m_ptr;
sPtr<stdObject>  ret;
	m_ptr = 0;
	ret = search(key,strlen(key),thNULL,&m_ptr);
	if ( ret == thNULL ) {
		*match_len = 0;
		return thNULL;
	}
	*match_len = m_ptr - key;
	return ret;
}

sPtr<stdObject> 
stdStringTreeBase::search(int *match_len,sPtr<stdString>  key)
{
const char * m_ptr, * s_ptr;
sPtr<stdObject> ret;
	m_ptr = 0;
	s_ptr = key->get_str();
	ret = search(s_ptr,key->length(),thNULL,&m_ptr);
	if ( ret == thNULL ) {
		*match_len = 0;
		return thNULL;
	}
	*match_len = m_ptr - s_ptr;
	return ret;
}

sPtr<stdObject> 
stdStringTreeBase::del(sPtr<stdObject>  obj)
{
sPtr<stdString>  path;

	path = search(obj);
	if ( path == thNULL )
		return thNULL;
	return del(path);
}

sPtr<stdObject> 
stdStringTreeBase::del(const char * key,int len)
{
sPtr<stdStringTreeBase>  elp, * elpp;
sPtr<stdObject>  ret;


	if ( len == 0 || key[0] == 0 ) {
		ret = obj;
		obj = thNULL;
		partial_flag = 0;
		return ret;
	}
	for ( elpp = &list ; (*elpp).is_notNull() ; elpp = &elp->next ) {
	int clen,xlen;
		elp = *elpp;
		match(&clen,&xlen,elp,key,len);
 		if ( clen == 0 ) {
			continue;
		}
		if ( clen == xlen ) {
			ret = elp->del(&key[clen],len-clen);
			if ( elp->obj.is_notNull() )
				return ret;
			if ( elp->list == thNULL ) {
				*elpp = elp->next;
			}
			else if ( elp->list->next == thNULL ) {
				elp->str = 
					elp->str->add(elp->list->str);
				elp->obj = elp->list->obj;
				elp->partial_flag = elp->list->partial_flag;
				elp->list = elp->list->list;
			}
			return ret;
		}
		return thNULL;
	}
	return thNULL;
}


sPtr<stdObject>  
stdStringTreeBase::del(const char * key)
{
	return del(key,strlen(key));
}

sPtr<stdObject> 
stdStringTreeBase::del(sPtr<stdString>  key)
{
	return del(key->get_str(),key->length());
}



void
stdStringTreeBase::dump(const char * msg,sPtr<stdString>  path)
{
sPtr<stdStringTreeBase>  elp;
	if ( path.is_notNull() ) {
		::printf("%s %s*%s %p %i\n",msg,path->get_str(),STR_NULL(str)->get_str(),obj.__get(),
				partial_flag);
		path = path->add("*")->add(str);
	}
	else {
		::printf("%s %s %p\n",msg,STR_NULL(str)->get_str(),obj.__get());
		path = thNEW( stdString,(""));
	}
	for ( elp = list ; elp.is_notNull() ; elp = elp->next )
		elp->dump(msg,path);
}
