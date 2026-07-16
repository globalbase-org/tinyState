

#ifndef ___mthProgress_cpp_H___
#define ___mthProgress_cpp_H___


#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sPtr.h"

class stdString;
class mthProgress : public stdObject {
public:
	mthProgress();
	~mthProgress();
	void reset(int complete,int dir=0);
	void display(sPtr<stdString>  d);
	void display(const char* d);
	void set(int value);
protected:
	INTEGER64	lastInd;
	sPtr<stdString> 	indicate;
	int 	complete;
	int 	dir;
	int	value;
};

#endif
