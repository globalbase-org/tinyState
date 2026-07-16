
#ifndef ___stdDualQueue_H_cpp___
#define ___stdDualQueue_H_cpp___

#include	"ts2/c++/stdObject.h"

#define NODE_AFTER	0
#define NODE_BEFORE	1

template<class __TYPE>
class stdDualQueue : public stdObject {
public:
	__TYPE * 	next;
	__TYPE *	prev;
	__TYPE **	_root;
	stdDualQueue(__TYPE ** __root)
	{
		ins(__root);
	}
	stdDualQueue()
	{
	}
	stdDualQueue(__TYPE * node,int flag=NODE_AFTER)
	{
		ins(node,flag);
	}
	~stdDualQueue() {
		del();
	}
	int is_head() {
		if ( _root == 0 )
			return -1;
		if ( *_root == this )
			return 1;
		return 0;
	}
	int is_tail() {
		if ( _root == 0 )
			return -1;
		if ( this->next == *_root )
			return 1;
		return 0;
	}
	__TYPE * head() {
		return *_root;
	}
	__TYPE * tail() {
		if ( *_root == 0 )
			return 0;
		return (*_root)->prev;
	}

	void del() {
		if ( _root == 0 )
			return;
		if ( *_root == this ) {
			if ( this->next == this )
				*_root = thNULL
			else	*_root = this->next;
		}
		this->prev->next = this->next;
		this->next->prev = this->prev;
		this->next = thNULL;
		this->prev = thNULL;
		_root = 0;
	}
	void ins(__TYPE ** root) {
		if ( _root )
			del();
		_root = root;
		if ( *_root ) {
			this->next = *_root;
			this->prev = (*_root)->prev;
			this->next->prev = 
				dynamic_cast<__TYPE*>(this);
			this->prev->next = 
				dynamic_cast<__TYPE*>(this);
		}
		else {
			this->next = 
				dynamic_cast<__TYPE*>(this);
			this->prev = 
				dynamic_cast<__TYPE*>(this);
			*_root = 
				dynamic_cast<__TYPE*>(this);
		}
	}
	int ins(__TYPE * node,int flag)
	{
		if ( node == 0 || node->_root == 0 )
			return -1;
		if ( _root )
			del();
		_root = node->_root;
		if ( flag == NODE_AFTER ) {
			this->prev = node;
			this->next = node->next;
			this->next->prev = 
				dynamic_cast<__TYPE*>(this);
			this->prev->next = 
				dynamic_cast<__TYPE*>(this);
		}
		else {
			this->next = node;
			this->prev = node->prev;
			this->next->prev = 
				dynamic_cast<__TYPE*>(this);
			this->prev->next = 
				dynamic_cast<__TYPE*>(this);
			if ( node == *_root )
				*_root = 
					dynamic_cast<__TYPE*>(this);
		}
		return 0;
	}
	void close() {
		for ( ; this->next != this ; )
			this->next->del();
		del();
	}
	int count() {
	stdDualQueue * q;
	int ret;
		if ( next == 0 )
			return 1;
		ret = 1;
		for ( q = this->next ; q != this ; q = q->next )
			ret ++;
		return ret;
	}
};

#endif
