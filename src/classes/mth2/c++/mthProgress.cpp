

#include		"mth2/c++/mthProgress.h"
#include		"ts2/c++/stdString.h"
#include		"ts2/c++/stdInterval.h"

mthProgress::mthProgress() {
}
mthProgress::~mthProgress()
{
	indicate = thNULL;
}
void
mthProgress::reset(int complete,int dir)
{
	this->complete = complete;
	this->dir = dir;
	this->value = 0;
	lastInd = 0;
}
void 
mthProgress::display(sPtr<stdString>  str) {
	::printf("\n");
	indicate = str;
}
void 
mthProgress::display(const char * str) {
	indicate = thNEW( stdString,(str));
}
void 
mthProgress::set(int value) {
	if ( dir == 0 )
		this->value = value;
	else	this->value = complete - value;
	if ( indicate.is_notNull() ) {
	INTEGER64 n = stdInterval::now();
		if ( n - lastInd >= 100*1000 ) {
			::printf("%s %i/%i (%.1lf %%)     \r",
				indicate->get_str(),
				this->value, complete,
				100.0*this->value/complete
				);
			fflush(stdout);
			lastInd = n;
		}
	}
}
