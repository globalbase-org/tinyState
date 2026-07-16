

#ifndef ___stdTouch_cpp_H___
#define ___stdTouch_cpp_H___

#include	"ts2/c++/stdQueue.h"

class tinyState;


class stdTouch : public stdObject {
public:
	stdTouch() {
	}
	~stdTouch() {
		attention = thNULL;
	}
	void invoke() {
		if ( attention ) {
			attention->reset();
		}
		else {
			attention = 
				thNEW( stdQueue<tinyState>,());
		}
	}
	int is_touch(sPtr<tinyState>  caller) {
		if ( attention == 0 )
			return 0;
		if ( attention->check(caller,0) )
			return 0;
		attention->ins(MAX_INTEGER64,caller);
		return 1;
	}
private:
	sPtr<stdQueue<tinyState> > 		attention;
};

#endif


