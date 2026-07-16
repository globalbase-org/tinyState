

#ifndef ___stdQueue_cpp_H___
#define ___stdQueue_cpp_H___

#include	<functional>
#include	<stdio.h>
#include	"ts2/c++/ts_types.h"
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sPtr.h"
#include	"ts2/c++/sWptr.h"

class stdQueueDebug {
public:
	static int debug_flag;
};

template<class __TYPE>
class stdQueue_;

/**
 * @brief 優先度キュー (ソート済み連結リスト) の要素。/ Element of a priority queue (sorted linked list).
 * @details `stdQueue_<T>` の内部リンクリストノード。直接使うことはあまりない。/ Internal linked-list node of `stdQueue_<T>`; rarely used directly.
 * @tparam __TYPE キューの要素の型。/ Element type.
 */
template<class __TYPE>
class stdQueueElement : public stdObject {
public:
	sPtr<stdQueueElement>		next;
	sPtr<__TYPE>			data;
	INTEGER64			key;
	sWptr<stdQueue_<__TYPE> >	top;

	stdQueueElement()
	{
	}
	virtual ~stdQueueElement()
	{
		data = thNULL;
		next = thNULL;
		key = 0;
	}

protected:
};




/**
 * @brief 優先度付きソート済みキューの基底実装。/ Base implementation of a sorted priority queue.
 * @details
 * `ins(key, data)` で key 昇順に挿入、`del()` で先頭から取り出す。
 * `key = MAX_INTEGER64` を指定すると末尾追加 (FIFO として使える)。
 * `check(data, cmp)` で検索、`del(data, cmp)` で条件削除も可能。
 * / Insert with `ins(key, data)` (sorted ascending by key); dequeue from head with `del()`.
 * Use `key = MAX_INTEGER64` for FIFO (append-to-tail). Supports search and conditional delete.
 * @tparam __TYPE 要素の型。/ Element type.
 */
template<class __TYPE>
class stdQueue_ : public stdObject {
public:
	sPtr<stdQueueElement<__TYPE> >	head;
	sPtr<stdQueueElement<__TYPE> >	tail;

	int count;
	unsigned insNeq:1;



	stdQueue_()
	{
		head = thNULL;
		tail = thNULL;
		count = 0;
	}
	virtual ~stdQueue_()
	{
		head = thNULL;
		tail = thNULL;
		count = 0;
	}



	void ins(INTEGER64 key,sPtr<__TYPE> data)
	{
	sPtr<stdQueueElement<__TYPE> > el, * elp;
		el = thNEW(stdQueueElement<__TYPE>,());
		el->top = this;
        	el->data = data;
	        el->key = key;
        	if ( head.is_notNull() ) {
	        	if ( key == MAX_INTEGER64 ) {
                		tail->next = el;
        	                tail = el;
	                }
                	else {
        	                elp = &head;
	                        for ( ; (*elp).is_notNull() ; elp = &(*elp)->next ) {
				  	if ( insNeq ) {
	                        	        if ( (*elp)->key > key )
        	                        	        break;
					}
					else {
	                        	        if ( (*elp)->key >= key )
        	                        	        break;
					}
                	        }
        	                el->next = *elp;
	                        *elp = el;
                        	if ( el->next == thNULL )
                	                tail = el;
        	        }
	        }
        	else {
	        	head = el;
                	tail = el;
        	}
		count ++;
	}
	void ins(sPtr<stdQueue_<__TYPE> > q)
	{
	sPtr<stdQueueElement<__TYPE> > elp;
		for ( elp = q->head ; elp.is_notNull() ; elp = elp->next )
			ins(MAX_INTEGER64,elp->data);
	}
	static int
	cmp_stdQueue3(sPtr<__TYPE > d1,sPtr<stdObject>  d2)
	{
		if ( d1 == 0 )
			return 1;
		if ( d1 == d2 )
        		return 1;
        	return 0;
	}
	void ins(sPtr<stdObject>  ref,sPtr<__TYPE> d,int (*cmp)(sPtr<__TYPE> d1,sPtr<stdObject>  d2))
	{
	sPtr<stdQueueElement<__TYPE> >* elpp, elp;
	int ret;
		if ( cmp == 0 )
			cmp = cmp_stdQueue3;
		for ( elpp = &head ; *elpp ; elpp = &(*elpp)->next ) {
			ret = (*cmp)((*elpp)->data,ref);
			if ( ret == 0 )
				continue;
			elp = thNEW( stdQueueElement<__TYPE>,());
			elp->top = this;
			elp->data = d;
			if ( ret > 0 ) {
				elp->next = (*elpp)->next;
				(*elpp)->next = elp;
				if ( tail->next )
					tail = tail->next;
				return;
			}
			elp->next = *elpp;
			*elpp = elp;
			if ( tail->next )
				tail = tail->next;
			return;
		}
		ret = (*cmp)(0,ref);
		if ( ret == 0 )
			return;
		elp = thNEW( stdQueueElement<__TYPE>,());
		elp->top = this;
		elp->data = d;
		if ( ret > 0 ) {
			elp->next = head;
			head = elp;
			if ( tail == 0 )
				tail = elp;
		}
		else {
			if ( tail == 0 ) {
				tail = elp;
				head = elp;
				return;
			}
			tail->next = elp;
			tail = elp;
		}
	}

	sPtr<__TYPE> del()
	{
	sPtr<stdQueueElement<__TYPE> > el;
	sPtr<__TYPE> rdata;
		el = head;
        	if ( el == thNULL )
	        	return thNULL;
        	head = el->next;
		if ( head == thNULL )
			tail = thNULL;
		count --;
		rdata = el->data;
        	return rdata;
	}

	static int
	cmp_stdQueue(sPtr<__TYPE> d1,sPtr<__TYPE> d2)
	{
		if ( d1 == d2 )
        		return 1;
        	return 0;
	}
	static int
	_debug_check(sPtr<__TYPE> d1,sPtr<__TYPE> d2)
	{
		if ( d1 == d2 )
        		return 0;
        	return 0;
	}
	void
	debug_check() {
		this->check(0,_debug_check);
	}


	sPtr<__TYPE>
	del(sPtr<__TYPE> data,int (*cmp)(sPtr<__TYPE>,sPtr<__TYPE>))
	{
	sPtr<stdQueueElement<__TYPE> > el, ret;
	sPtr<__TYPE> rdata;
		if ( head == thNULL )
        		return thNULL;
	        if ( cmp == 0 )
        		cmp = cmp_stdQueue;
	        if ( (*cmp)(head->data,data) ) {
        		return del();
	        }
        	for ( el = head ; el->next.is_notNull() ; el = el->next ) {
	        	if ( (*cmp)(el->next->data,data) ) {
				ret = el->next;
                		if ( el->next == tail )
        	                	tail = el;
	                	el->next = el->next->next;
				count --;
				rdata = ret->data;
                	        return rdata;
        	        }
	        }
        	return thNULL;
	}

	sPtr<__TYPE>
	del(std::function<int(sPtr<__TYPE>)> cmp)
	{
	sPtr<stdQueueElement<__TYPE> > el, ret;
	sPtr<__TYPE> rdata;
		if ( head == thNULL )
        		return thNULL;
	        if ( cmp(head->data) ) {
        		return del();
	        }
        	for ( el = head ; el->next.is_notNull() ; el = el->next ) {
	        	if ( cmp(el->next->data) ) {
				ret = el->next;
                		if ( el->next == tail )
        	                	tail = el;
	                	el->next = el->next->next;
				count --;
				rdata = ret->data;
                	        return rdata;
        	        }
	        }
        	return thNULL;
	}

	sPtr<__TYPE>
	check(sPtr<__TYPE> data,int (*cmp)(sPtr<__TYPE>,sPtr<__TYPE>))
	{
	sPtr<stdQueueElement<__TYPE> > el;
		if ( head == thNULL )
        		return thNULL;
	        if ( cmp == 0 )
        		cmp = cmp_stdQueue;
	        for ( el = head ; el.is_notNull() ; el = el->next ) {
        		if ( (*cmp)(el->data,data) )
        	        	return el->data;
	        }
        	return thNULL;
	}


	static int head_stdQueue(sPtr<__TYPE> d1,sPtr<__TYPE> d2)
	{
        	return 1;
	}


	void set_tail()
	{
	sPtr<stdQueueElement<__TYPE> > el;
		if ( head == thNULL ) {
			count = 0;
			tail = thNULL;
			return;
		}
		count = 1;
		for ( el = head ; el->next.is_notNull() ; el = el->next , count ++ )
				;
		tail = el;
	}



	sPtr<stdQueueElement<__TYPE> >
	_sort(sPtr<stdQueueElement<__TYPE> > inp,int (*cmp)(sPtr<__TYPE>,sPtr<__TYPE>))
	{
	sPtr<stdQueueElement<__TYPE> > a, b, ret,_ret, * rp;
	int conv;

		if ( inp == thNULL || inp->next == thNULL )
			return inp;
		_ret = 0;
		_div(&a,&b,inp,cmp);
		rp = &_ret;
		for ( ; a.is_notNull() && b.is_notNull() ; ) {
			conv = (*cmp)(a->data,b->data);
			if ( conv < 0 ) {
				*rp = a;
				a = a->next;
				rp = &(*rp)->next;
			}
			else if ( conv > 0 ) {
				*rp = b;
				b = b->next;
				rp = &(*rp)->next;
			}
			else {
				*rp = a;
				a = a->next;
				rp = &(*rp)->next;
				*rp = b;
				b = b->next;
				rp = &(*rp)->next;
			}
		}
		if ( a.is_notNull() ) {
			*rp = a;
		}
		else if ( b.is_notNull() ) {
			*rp = b;
		}
		else	*rp = thNULL;
		ret = _ret;
		_ret = thNULL;
		return ret;
	}
	void _div(
		sPtr<stdQueueElement<__TYPE> >* a,
		sPtr<stdQueueElement<__TYPE> >* b,
		sPtr<stdQueueElement<__TYPE> > inp,
		int (*cmp)(sPtr<__TYPE>,sPtr<__TYPE>))
	{
	sPtr<stdQueueElement<__TYPE> > bp, bp2;
	int len,i;
		len = 0;
		for ( bp = inp ; bp.is_notNull() ; bp = bp->next, len ++ );
		if ( len <= 1 ) {
			*a = inp;
			*b = 0;
			return;
		}
		len = len / 2;
		len --;
		for ( i = 0 , bp = inp ; bp.is_notNull() && i < len ; bp = bp->next , i ++ );
		bp2 = bp;
		bp = bp->next;
		bp2->next = thNULL;
		for ( bp2 = inp ; bp2->next.is_notNull() ; bp2 = bp2->next )
			if ( (*cmp)(bp2->data,bp2->next->data) > 0 ) {
				inp = _sort(inp,cmp);
				break;
			}
		for ( bp2 = bp ; bp2->next.is_notNull() ; bp2 = bp2->next )
			if ( (*cmp)(bp2->data,bp2->next->data) > 0 ) {
				bp = _sort(bp,cmp);
				break;
			}
		*a = inp;
		*b = bp;
	}

	void reset() {
		head = thNULL;
		set_tail();
	}
};

template<class __TYPE>
class stdQueue;


/**
 * @brief `stdObject` 型専用の stdQueue 特殊化。コピーコンストラクタを持つ。/ Specialization of stdQueue for `stdObject`; supports copy construction.
 */
template<>
class stdQueue<stdObject> : public stdQueue_<stdObject> {
public:
	stdQueue() : stdQueue_<stdObject>() {}

	static int copy_stdQueue(sPtr<stdObject>  obj1,sPtr<stdObject>  obj2)
	{
	sPtr<stdQueue<stdObject> >  me = sPtr<stdQueue<stdObject> >::d_cast(obj2);
		me->ins(MAX_INTEGER64,obj1);
		return 0;
	}

	stdQueue(sPtr<stdQueue<stdObject> >  q)
	{
		this->head = thNULL;
		this->tail = thNULL;
		this->count = 0;
		this->insNeq = q->insNeq;
		q->check(this,copy_stdQueue);
	}

};

/**
 * @brief 型安全な優先度キュー。/ Type-safe priority queue.
 * @details `stdQueue_<T>` に型安全な `del()`/`check()` オーバーロードと `sort()` を追加する。
 * `thNEW(stdQueue<MyClass>, ())` で生成。`sPtr<stdQueue<MyClass>>` として使う。
 * / Adds type-safe `del()`/`check()` overloads and `sort()` to `stdQueue_<T>`.
 * @tparam __TYPE 要素の型 (`stdObject` 派生)。/ Element type (must derive from `stdObject`).
 */
template<class __TYPE>
class stdQueue : public stdQueue_<__TYPE> {
public:
	stdQueue() : stdQueue_<__TYPE>() {}

	static int copy_stdQueue(sPtr<__TYPE> obj1,sPtr<stdObject>  obj2)
	{
	sPtr<stdQueue<__TYPE> > me = sPtr<stdQueue<__TYPE> >::d_cast(obj2);
		me->ins(MAX_INTEGER64,obj1);
		return 0;
	}

	stdQueue(sPtr<stdQueue<__TYPE> > q)
	{
		this->head = this->tail = 0;
		this->count = 0;
		this->insNeq = 0;
		if ( q == thNULL )
			return;
		this->insNeq = q->insNeq;
		q->check(this,copy_stdQueue);
	}

	static int
	cmp_stdQueue2(sPtr<__TYPE > d1,sPtr<stdObject>  d2)
	{
		if ( d1 == d2 )
        		return 1;
        	return 0;
	}


	sPtr<__TYPE> del()
	{
		return this->stdQueue_<__TYPE>::del();
	}
	sPtr<__TYPE>
	del(sPtr<__TYPE> data,int (*cmp)(sPtr<__TYPE>,sPtr<__TYPE>))
	{
		return this->stdQueue_<__TYPE>::del(data,cmp);
	}
	sPtr<__TYPE>
	del(std::function<int(sPtr<__TYPE>)> cmp)
	{
		return this->stdQueue_<__TYPE>::del(cmp);
	}



	sPtr<__TYPE>
	del(sPtr<stdObject>  data,int (*cmp)(sPtr<__TYPE>,sPtr<stdObject> ))
	{
	sPtr<stdQueueElement<__TYPE> > el, ret;
		if ( this->head == thNULL )
        		return thNULL;
	        if ( cmp == 0 )
        		cmp = cmp_stdQueue2;
	        if ( (*cmp)(this->head->data,data) ) {
        		return del();
	        }
        	for ( el = this->head ; el->next.is_notNull() ; el = el->next ) {
	        	if ( (*cmp)(el->next->data,data) ) {
			sPtr<__TYPE> rdata;
				ret = el->next;
                		if ( el->next == this->tail )
        	                	this->tail = el;
	                	el->next = el->next->next;
				this->count --;
				rdata = ret->data;
                	        return rdata;
        	        }
	        }
        	return thNULL;
	}

	int move(int (*cmp)(sPtr<__TYPE>))
	{
		return move(thNULL,cmp);
	}

	int move(sPtr<stdQueue<__TYPE> > dest,int (*cmp)(sPtr<__TYPE>))
	{
	sPtr<stdQueueElement<__TYPE> >* elp;
	int count;
		count = 0;
		for ( elp = &this->head ; (*elp).is_notNull() ; ) {
		  	int ret = (*cmp)((*elp)->data);
		  	if ( ret > 0 ) {
				if ( dest.is_notNull() )
					dest->ins(MAX_INTEGER64,(*elp)->data);
				*elp = (*elp)->next;
				count ++;
				continue;
			}
			else if ( ret < 0 ) {
				break;
			}
			elp = &(*elp)->next;
		}
		this->set_tail();
		return count;
	}

	sPtr<__TYPE>
	check(sPtr<__TYPE> data,int (*cmp)(sPtr<__TYPE>,sPtr<__TYPE>))
	{
		return this->stdQueue_<__TYPE>::check(data,cmp);
	}

	sPtr<__TYPE>
	check(sPtr<stdObject>  data,int (*cmp)(sPtr<__TYPE>,sPtr<stdObject> ))
	{
	sPtr<stdQueueElement<__TYPE> > el;
		if ( this->head == thNULL )
        		return thNULL;
	        if ( cmp == 0 )
        		cmp = cmp_stdQueue2;
	        for ( el = this->head ; el.is_notNull() ; el = el->next ) {
        		if ( (*cmp)(el->data,data) )
        	        	return el->data;
	        }
        	return thNULL;
	}
	sPtr<__TYPE>
	check(std::function<int(sPtr<__TYPE>)> func)
	{
	sPtr<stdQueueElement<__TYPE> > el;
		if ( this->head == thNULL )
        		return thNULL;
	        for ( el = this->head ; el.is_notNull() ; el = el->next ) {
        		if ( func(el->data) )
        	        	return el->data;
	        }
        	return thNULL;
	}

	sPtr<stdQueue<__TYPE> >
	sort(int (*cmp)(sPtr<__TYPE>,sPtr<__TYPE>))
	{
	sPtr<stdQueue<__TYPE> > ret;
		ret = thNEW( stdQueue<__TYPE>,(this));
		ret->head = this->_sort(ret->head,cmp);
		ret->set_tail();
		return ret;
	}


};


#endif


