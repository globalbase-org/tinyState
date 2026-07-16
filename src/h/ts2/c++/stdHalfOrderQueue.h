

#ifndef ___stdHalfOrderQueue_H___
#define ___stdHalfOrderQueue_H___

#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sPtr.h"

class stdHalfOrderNode : public stdObject {
public:
	stdHalfOrderNode(int key,sPtr<stdObject>  data) {
		this->data = data;
		this->key = key;
		count = 1;
	}
	~stdHalfOrderNode() {
		data = thNULL;
		n[0] = thNULL;
		n[1] = thNULL;
	}
	sPtr<stdObject>  		data;
	int			key;
	int			count;
	sPtr<stdHalfOrderNode> 	n[2];

};

class stdHalfOrderQueue : public stdObject {
public:
	stdHalfOrderQueue();
	~stdHalfOrderQueue();
	void ins(int key,sPtr<stdObject>  data);
	sPtr<stdObject>  del(int * kp=0);
	sPtr<stdHalfOrderNode>  __top() {
		return top;
	};
	sPtr<stdObject>  getTop() {
		if ( top.is_notNull() )
			return top->data;
		return thNULL;
	}
	int count();
protected:
	static void _ins(sPtr<stdHalfOrderNode> ,int key,sPtr<stdObject>  data);
	sPtr<stdHalfOrderNode> 	top;
};

#endif
