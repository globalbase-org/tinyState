

#ifndef ___sException_cpp_H__
#define ___sException_cpp_H__

#include	"ts2/c++/sPtr.h"

#define EX_STAY		0
#define EX_ERROR	1

class tinyState;
class sThreadMutex;
class sException : public sObject {
public:
	sException(std::function<int(sPtr<tinyState>)>,int type=0);
	~sException();

	int		type;
	std::function<int(sPtr<tinyState>)> is_ready;
};

#endif
