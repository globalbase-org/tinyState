

#ifndef ___stdOSfundamental_cpp_H___
#define ___stdOSfundamental_cpp_H___


#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sPtr.h"

class stdString;

class stdOSfundamental : public stdObject {
public:
  	static sPtr<stdString>  getDirectoryHome();
  	static sPtr<stdString>  getDirectoryCaches();
  	static sPtr<stdString>  getDirectoryTmp();
  	static sPtr<stdString>  getDirectoryBundle();
};

#endif
