/*
 * s2IOstd — MinGW implementation.  Wrap the process's own std streams as ts2IO,
 * choosing the concrete class by GetFileType so the app never touches a raw
 * HANDLE.  Windows-port design memo §9.
 *
 *   FILE_TYPE_CHAR  -> console  -> ts2IOwinConsole (readiness bridge)
 *   FILE_TYPE_PIPE / _DISK      -> ts2IOdescriptor (overlapped)
 *
 * NOTE (refinement): the pipe path assumes the inherited std HANDLE is
 * overlapped-capable.  A child launched by ts2System gets *synchronous* pipe
 * ends today; making a tinyState child's stdio truly async needs ts2System to
 * create the child ends with FILE_FLAG_OVERLAPPED (which would break plain
 * synchronous children) or a per-handle reader thread.  Left as a follow-up.
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out, so
 * <winsock2.h> (pulled by std2/includes.h) stays the sole winsock. Windows-port design memo */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	"ts2/c++/s2IOstd.h"
#include	"ts2/c++/ts2IOdescriptor.h"
#include	"ts2/c++/ts2IOwinConsole.h"

static sPtr<ts2IO>
s2iostd_wrap(sPtr<tinyState> parent,DWORD which)
{
HANDLE h = GetStdHandle(which);
	if ( h == INVALID_HANDLE_VALUE || h == NULL )
		return thNULL;
	if ( GetFileType(h) == FILE_TYPE_CHAR )
		return thNEW(ts2IOwinConsole,(parent,h));
	return thNEW(ts2IOdescriptor,(parent,h));
}

int
s2IOstd::init(
	sPtr<tinyState>	parent,
	sPtr<ts2IO> *	in_p,
	sPtr<ts2IO> *	out_p,
	sPtr<ts2IO> *	err_p)
{
int ret = 0;
	if ( in_p ) {
		*in_p = s2iostd_wrap(parent,STD_INPUT_HANDLE);
		if ( in_p->is_notNull() == 0 ) ret = -1;
	}
	if ( out_p ) {
		*out_p = s2iostd_wrap(parent,STD_OUTPUT_HANDLE);
		if ( out_p->is_notNull() == 0 ) ret = -1;
	}
	if ( err_p ) {
		*err_p = s2iostd_wrap(parent,STD_ERROR_HANDLE);
		if ( err_p->is_notNull() == 0 ) ret = -1;
	}
	return ret;
}
