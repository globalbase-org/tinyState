
#ifndef ___ts2IOdescriptor_cpp_H___
#define ___ts2IOdescriptor_cpp_H___


#include	"ts2/c++/fwIO.h"
#include	"_ts2/c++/ts2IOdescriptor_pb.h"

#ifdef _WIN32
/* process-unique fwIO/IOCP completion key from the same counter as the
   per-descriptor fdid, so non-descriptor users (ts2System child-exit) can't
   collide with a live descriptor registration.  Windows-port design memo §E. */
int	ts2io_alloc_key();
#endif

#endif

