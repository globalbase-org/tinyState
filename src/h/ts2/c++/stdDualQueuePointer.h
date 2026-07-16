

#ifndef ___stdDualQueuePointer_cpp_H___
#define ___stdDualQueuePointer_cpp_H___



class stdDualQueuePointer__ : public stdDualQueue<stdDualQueuePointer__> {
public:
	sPtr<tinyState> 		ptr;
	stdDualQueuePointer__(stdDualQueuePointer__ ** root,sPtr<tinyState>  ptr)
		:
		stdDualQueue<stdDualQueuePointer__>(root) {
		this->ptr = ptr;
	}
	~stdDualQueuePointer__() {
		this->ptr = thNULL;
	}
};

template<class __TYPE>
class stdDualQueuePointer : public stdDualQueuePointer__ {
public:
	__TYPE *&		ptr;
	stdDualQueuePointer *&	next;
	stdDualQueuePointer *&	prev;
	stdDualQueuePointer **&	_root;

	stdDualQueuePointer(stdDualQueuePointer ** root,__TYPE * ptr)
		:
		stdDualQueuePointer__((stdDualQueuePointer__**)root,ptr),
		ptr((__TYPE*&)stdDualQueuePointer__::ptr),
		next((stdDualQueuePointer*&)stdDualQueuePointer__::next),
		prev((stdDualQueuePointer*&)stdDualQueuePointer__::prev),
		_root((stdDualQueuePointer**&)stdDualQueuePointer__::_root)
	{
	}
};


#endif

