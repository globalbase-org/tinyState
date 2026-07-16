/*
 * s2IOstd — POSIX implementation.  Wrap the process's own std streams as ts2IO.
 * On POSIX stdin/stdout/stderr are just fds 0/1/2 wrapped in ts2IOdescriptor.
 * Windows-port design memo §9.
 */

#include	"ts2/c++/s2IOstd.h"
#include	"ts2/c++/ts2IOdescriptor.h"

int
s2IOstd::init(
	sPtr<tinyState>	parent,
	sPtr<ts2IO> *	in_p,
	sPtr<ts2IO> *	out_p,
	sPtr<ts2IO> *	err_p)
{
	if ( in_p )
		*in_p  = thNEW(ts2IOdescriptor,(parent,0));
	if ( out_p )
		*out_p = thNEW(ts2IOdescriptor,(parent,1));
	if ( err_p )
		*err_p = thNEW(ts2IOdescriptor,(parent,2));
	return 0;
}
