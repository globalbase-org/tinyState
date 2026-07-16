

#ifndef ___stdBalancedTree_cpp_H___
#define ___stdBalancedTree_cpp_H___

#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/stdQueue.h"

class sBalancedTreeCondition {
public:
	sBalancedTreeCondition();
	sBalancedTreeCondition(int inf);
	~sBalancedTreeCondition();
	virtual int cmp(sBalancedTreeCondition & d);
	int is_infty();

	static sBalancedTreeCondition plusInfty;
	static sBalancedTreeCondition minusInfty;
protected:
	unsigned	plusInf:1;
	unsigned	minusInf:1;
};


class stdBalancedTreeCallback : public stdObject {
public:
	virtual int callback(
		sBalancedTreeCondition & c1,
		sPtr<stdBalancedTreeCallback> cb,
		sBalancedTreeCondition & c2,
		sPtr<stdObject>  obj);
};

class stdBalancedTreeObject_ : public stdObject {
public:
	stdBalancedTreeObject_(sBalancedTreeCondition &,sPtr<stdObject>  );
	~stdBalancedTreeObject_();
	sPtr<stdBalancedTreeObject_> 	next;
	sPtr<stdObject> 			obj;
	sBalancedTreeCondition &	cond;
};


class stdBalancedTree_;
class stdBalancedTreeElement_ : public stdObject {
	friend class stdBalancedTree_;
public:
	stdBalancedTreeElement_(sPtr<stdBalancedTreeObject_> obj);
	~stdBalancedTreeElement_();

protected:
	int levelSmall() {
		if ( small == thNULL )
			return 0;
		return small->level;
	}
	int levelLarge() {
		if ( large == thNULL )
			return 0;
		return large->level;
	}
	void leveled() {
	int s = levelSmall();
	int l = levelLarge();
		if ( s < l )
			level = l + 1;
		else	level = s + 1;
	}
	sPtr<stdBalancedTreeObject_> ins(sPtr<stdBalancedTreeObject_> obj,int enabledEqu);
	sPtr<stdBalancedTreeObject_> del(sPtr<stdBalancedTreeObject_> d,sPtr<stdBalancedTreeElement_>* posThis);
	virtual void walk(sPtr<stdBalancedTree_> parent,
		sBalancedTreeCondition & c1,sPtr<stdBalancedTreeCallback> cb);
	void balanced();

	sPtr<stdBalancedTreeElement_> 		small;
	sPtr<stdBalancedTreeElement_> 		large;
	sPtr<stdBalancedTreeObject_>		obj;

	int					level;
};

class stdBalancedTree_ : public stdObject {
	friend class stdBalancedTreeElement_;
public:
	stdBalancedTree_(int enableEqu);
	~stdBalancedTree_();

	virtual sPtr<stdBalancedTreeObject_> search(sBalancedTreeCondition & inp);
	virtual sPtr<stdBalancedTreeObject_> ins(sPtr<stdBalancedTreeObject_> obj);
	virtual sPtr<stdBalancedTreeObject_> del(sPtr<stdBalancedTreeObject_> d);
	virtual void walk(sBalancedTreeCondition & c1,sPtr<stdBalancedTreeCallback> cb);
	int count();

protected:
	virtual int callback(
			sBalancedTreeCondition & c1,
			sPtr<stdBalancedTreeCallback> cb,
			sBalancedTreeCondition & c2,
			sPtr<stdObject>  obj);

	unsigned	enabledEqu:1;
	sPtr<stdBalancedTreeElement_> 		top;
	int 					_count;
};

template<class __TYPE2,class __TYPE1>
class stdBalancedTreeObject : public stdBalancedTreeObject_ {
public:
	stdBalancedTreeObject(sPtr<__TYPE1> obj)
		: stdBalancedTreeObject_(cond,obj)
	{
	}
	__TYPE2		cond;
};

template<class __TYPE1>
class stdBalancedTreeStdQueueCallback : public stdBalancedTreeCallback {
public:
	stdQueue<__TYPE1> * q;
	virtual int callback(
			sBalancedTreeCondition & c1,
			sPtr<stdBalancedTreeCallback>  cb,
			sBalancedTreeCondition & c2,
			sPtr<stdObject>  obj) {
		q->ins(MAX_INTEGER64,dynamic_cast<__TYPE1*>(obj));
		return 0;
	}
};

template<class __TYPE2,class __TYPE1,bool __EQU=false>
class stdBalancedTree;


template<class __TYPE2,class __TYPE1>
class stdBalancedTree<__TYPE2,__TYPE1,false> : public stdBalancedTree_ {
	friend class stdBalancedTreeElement_;
public:
	stdBalancedTree()
		:stdBalancedTree_(0)
	{
	}

	virtual sPtr<__TYPE1> search(__TYPE2 inp) {
	sPtr<stdBalancedTreeObject_> ret = stdBalancedTree_::search(inp);
		if ( ret == thNULL )
			return thNULL;
		return sPtr<__TYPE1>::dcast(ret->obj);
	}
	virtual sPtr<__TYPE1> ins(__TYPE2 cond,sPtr<__TYPE1> obj) {
	sPtr<stdBalancedTreeObject_> ret;
		ret = stdBalancedTree_::ins(thNEW( stdBalancedTreeObject<__TYPE2 COMMA __TYPE1>,(obj)));
		if ( ret == thNULL )
			return thNULL;
		return dynamic_cast<__TYPE1*>(ret->obj);
	}
	virtual sPtr<__TYPE1> del(__TYPE2 cond,sPtr<__TYPE1> obj=0) {
	sPtr<stdBalancedTreeObject_> ret;
		ret = stdBalancedTree_::del(thNEW( stdBalancedTreeObject<__TYPE2 COMMA __TYPE1>,(obj)));
		if ( ret == thNULL )
			return thNULL;
		return sPtr<__TYPE1>::d_cast(ret->obj);
	}
	virtual void walk(__TYPE2 cond,sPtr<stdBalancedTreeCallback > cb) {
		stdBalancedTree_::walk(cond,cb);
	}
	virtual sPtr<stdQueue<__TYPE1> > get_stdQueue() {
	sPtr<stdBalancedTreeStdQueueCallback<__TYPE1> > cb;
		cb = thNEW( stdBalancedTreeStdQueueCallback<__TYPE1>,());
		cb->q = thNEW( stdQueue<__TYPE1>,());
		stdBalancedTree_::walk(sBalancedTreeCondition::minusInfty,cb);
		return cb->q;
	}
protected:
	virtual int callback(
			sBalancedTreeCondition & c1,
			sPtr<stdBalancedTreeCallback> cb,
			sBalancedTreeCondition & c2,
			sPtr<stdObject>  obj) {
	__TYPE2 & cc1((__TYPE2 &)c1);
		return cb->callback((__TYPE2 &)c1,cb,(__TYPE2 &)c2,
				dynamic_cast<__TYPE1*>(obj));
	}
};

template<class __TYPE2,class __TYPE1>
class stdBalancedTree<__TYPE2,__TYPE1,true> : public stdBalancedTree_ {
	friend class stdBalancedTreeElement_;
public:
	stdBalancedTree()
		:stdBalancedTree_(1)
	{
	}

	virtual sPtr<stdQueue<__TYPE1> > search(__TYPE2 inp) {
	sPtr<stdBalancedTreeObject_ > ret = stdBalancedTree_::search(inp);
		if ( ret == thNULL )
			return thNULL;
	sPtr<stdQueue<__TYPE1> > q;
		q = thNEW( stdQueue<__TYPE1>,());
		for ( ; ret.is_notNull() ; ret = ret->next ) {
			q->ins(MAX_INTEGER64,sPtr<__TYPE1>::d_cast(ret->obj));
		}
		return q;
	}
	virtual sPtr<__TYPE1> ins(__TYPE2 cond,sPtr<__TYPE1> obj) {
	sPtr<stdBalancedTreeObject_> ret;
		ret = stdBalancedTree_::ins(thNEW( stdBalancedTreeObject<__TYPE2 COMMA __TYPE1>,(obj)));
		if ( ret == thNULL )
			return thNULL;
		return sPtr<__TYPE1>::d_cast(ret->obj);
	}
	virtual sPtr<stdQueue<__TYPE1> > del(__TYPE2 cond,sPtr<__TYPE1> obj=thNULL) {
	sPtr<stdBalancedTreeObject_> ret;
		ret = stdBalancedTree_::del(thNEW( stdBalancedTreeObject<__TYPE2 COMMA __TYPE1>,(obj)));
		if ( ret == thNULL )
			return thNULL;
	sPtr<stdQueue<__TYPE1> > q;
		q = thNEW( stdQueue<__TYPE1>,());
		for ( ; ret.is_notNull() ; ret = ret->next ) {
			q->ins(MAX_INTEGER64,sPtr<__TYPE1>::d_cast(ret->obj));
		}
		return q;
	}
	virtual void walk(__TYPE2 cond,sPtr<stdBalancedTreeCallback> cb) {
		stdBalancedTree_::walk(cond,cb);
	}
	virtual sPtr<stdQueue<__TYPE1> > get_stdQueue() {
	sPtr<stdBalancedTreeStdQueueCallback<__TYPE1> > cb;
		cb = thNEW( stdBalancedTreeStdQueueCallback<__TYPE1>,());
		cb->q = thNEW( stdQueue<__TYPE1>,());
		stdBalancedTree_::walk(sBalancedTreeCondition::minusInfty,cb);
		return cb->q;
	}
protected:
	virtual int callback(
			sBalancedTreeCondition & c1,
			sPtr<stdBalancedTreeCallback> cb,
			sBalancedTreeCondition & c2,
			sPtr<stdObject>  obj) {
	__TYPE2 & cc1((__TYPE2 &)c1);
		return cc1.callback((__TYPE2 &)c1,cb,
				(__TYPE2 &)c2,
				sPtr<__TYPE1>::d_cast(obj));
	}
};

#endif
