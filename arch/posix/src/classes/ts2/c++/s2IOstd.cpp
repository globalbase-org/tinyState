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
	/* Register the inherited std fds (0/1/2) in the descriptor ledger before
	   wrapping them: ts2IOdescriptor's FIN unconditionally soCLOSE()s its fd,
	   and __close() panics on a fd that was never opened through ts2.  Inherited
	   fds are not in the ledger otherwise, so without this the wrappers' teardown
	   aborts the process.  Calling s2IOstd::init() more than once per process is
	   itself a bug (std must be wrapped in a single call); fd_stdio() is not
	   idempotent and will panic on the double registration, surfacing it. */
	sObject::fd_stdio();
	if ( in_p )
		*in_p  = thNEW(ts2IOdescriptor,(parent,0));
	if ( out_p )
		*out_p = thNEW(ts2IOdescriptor,(parent,1));
	if ( err_p )
		*err_p = thNEW(ts2IOdescriptor,(parent,2));
	return 0;
}
