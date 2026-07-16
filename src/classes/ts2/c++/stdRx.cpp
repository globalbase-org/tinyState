
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/stdRx.h"

stdRx::stdRx(const char * p,int flags) {
	_pattern = thNEW( stdString,(p));
	initial(flags);
}
stdRx::stdRx(sPtr<stdString>  p,int flags) {
	_pattern = thNEW( stdString,(p));
  	initial(flags);
}
stdRx::~stdRx() {
  	regfree(&re);
	_pattern = thNULL;
}
void
stdRx::initial(int flags) {
char error_buf[ERROR_BUF_SIZE];
const char * pp;

	for ( pp = _pattern->get_str() ; *pp ; pp ++ )
		if ( *pp == '(' )
			numpunc ++;
	numpunc ++;
	err = regcomp(&re,_pattern->get_str(),REG_EXTENDED|flags);
	if ( err ) {
		regerror(err,&re,error_buf,ERROR_BUF_SIZE);
		::printf("stdRx ERROR %s <<%s>>\n",error_buf,_pattern->get_str());
	}
}

