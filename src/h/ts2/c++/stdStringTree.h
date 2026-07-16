

#ifndef ___stdStringTree_cpp_H___
#define ___stdStringTree_cpp_H___

#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/stdString.h"

class stdStringTreeBase : public stdObject {
public:
	stdStringTreeBase();
	~stdStringTreeBase();

	void dump(const char * msg,sPtr<stdString>  path);


protected:
	stdStringTreeBase(
		sPtr<stdString>  str,
		sPtr<stdObject>  obj,
		int partial_flag,
		sPtr<stdStringTreeBase>  list);
	void match(
		int * clen_p,
		int * xlen_p,
		sPtr<stdStringTreeBase>  elp,
		const char * ix,
		int len);
	void reset(
		int len,
		sPtr<stdObject>  obj,
		int partial_flag,
		sPtr<stdStringTreeBase>  list);

	void count(int * acc);
	void ins(const char * key,sPtr<stdObject>  obj,int partial_flag);
	void ins(sPtr<stdString>  key,sPtr<stdObject>  obj,int partial_flag);
	void ins(const char * key,int len,sPtr<stdObject>  obj,int partial_flag);

	sPtr<stdObject>  del(const char * key);
	sPtr<stdObject>  del(sPtr<stdString>  key);
	sPtr<stdObject>  del(sPtr<stdObject>  obj);
	sPtr<stdObject>  del(const char * key,int len);

	sPtr<stdObject>  search(const char * key);
	sPtr<stdObject>  search(sPtr<stdString>  key);
	sPtr<stdString>  search(sPtr<stdObject>  obj);
	sPtr<stdObject>  search(const char * key,int len);
	sPtr<stdObject>  search(sPtr<stdObject>  msg,
			int (*cmp)(sPtr<stdObject>  d1,
				sPtr<stdObject>  d2));

	sPtr<stdObject>  search(int * match_len,const char * key);
	sPtr<stdObject>  search(int * match_len,sPtr<stdString>  key);
	sPtr<stdObject>  search(const char * key,int len,sPtr<stdObject>  olast,const char ** m_ptr);

sPtr<stdStringTreeBase>  	next;
	sPtr<stdString>  		str;
	sPtr<stdObject>  		obj;
	sPtr<stdStringTreeBase>  	list;

	unsigned		partial_flag:1;
};

template<class __TYPE>
class stdStringTree : public stdStringTreeBase {
public:
	stdStringTree()
	{
	}
	~stdStringTree() 
	{
	}

	int count() {
	int ret;
		ret = 0;	
		stdStringTreeBase::count(&ret);
		return ret;
	}
	void ins(const char * key,sPtr<__TYPE> obj,int p_flag) {
		stdStringTreeBase::ins(key,obj,p_flag);
	}
	void ins(sPtr<stdString>  key,sPtr<__TYPE> obj,int p_flag) {
		stdStringTreeBase::ins(key,obj,p_flag);
	}
	sPtr<__TYPE> del(const char * key) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::del(key));
	}
	sPtr<__TYPE> del(sPtr<stdString>  key) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::del(key));
	}
	sPtr<__TYPE> del(sPtr<__TYPE> obj) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::del(obj));
	}
	sPtr<stdString>  search(sPtr<__TYPE> obj) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::search(obj));
	}
	sPtr<__TYPE> search(const char * key) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::search(key));
	}
	sPtr<__TYPE> search(sPtr<stdString>  key) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::search(key));
	}
	sPtr<__TYPE> search(int *match_len,const char * key) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::search(match_len,key));
	}
	sPtr<__TYPE> search(int *match_len,sPtr<stdString>  key) {
		return sPtr<__TYPE>::d_cast(stdStringTreeBase::search(match_len,key));
	}
	sPtr<__TYPE> search(sPtr<stdObject>  msg,int (*cmp)(sPtr<__TYPE>,sPtr<stdObject> )) {
		return sPtr<__TYPE>::d_cast(
			stdStringTreeBase::search(msg,
				(int (*)(sPtr<stdObject> ,sPtr<stdObject> ))cmp));
	}
	void dump(const char * msg) {
		::printf("%s BEGIN\n",msg);
		stdStringTreeBase::dump(msg,thNULL);
		::printf("%s END\n",msg);
	}
};


#endif

