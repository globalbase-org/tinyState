

#include	"ts2/c++/stdOSfundamental.h"
#include	"ts2/c++/cDarwin.h"
#include	"ts2/c++/stdString.h"

sPtr<stdString> 
stdOSfundamental::getDirectoryHome()
{
char buffer[1000];
	darwin_getDirectoryHome(buffer,1000);
	return thNEW( stdString,(buffer));
}

sPtr<stdString> 
stdOSfundamental::getDirectoryCaches()
{
char buffer[1000];
	darwin_getDirectoryCaches(buffer,1000);
	return thNEW( stdString,(buffer));
}

sPtr<stdString> 
stdOSfundamental::getDirectoryTmp()
{
char buffer[1000];
	darwin_getDirectoryTmp(buffer,1000);
	return thNEW( stdString,(buffer));
}

sPtr<stdString> 
stdOSfundamental::getDirectoryBundle()
{
char buffer[1000];
	darwin_getDirectoryBundle(buffer,1000);
	return thNEW( stdString,(buffer));
}
