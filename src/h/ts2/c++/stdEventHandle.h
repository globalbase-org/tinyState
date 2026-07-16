

#ifndef ___stdEventHandle_cpp_H___
#define ___stdEventHandle_cpp_H___


#include	"ts2/c++/tinyState.h"

class tinyState;

class stdEventHandle : public stdObject {
public:
	sPtr<tinyState>  		source;
	sPtr<tinyState>  		listener;
	int			type;
	TS_HANDLER_FUNC		handler;

	~stdEventHandle();
	stdEventHandle(
		sPtr<tinyState>  source,
		sPtr<tinyState>  listener,
		int type, 
		TS_HANDLER_FUNC handler);
	void remove();
};


#endif


