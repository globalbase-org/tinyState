
#ifndef ___ts2IOdescriptor_cpp_H___
#define ___ts2IOdescriptor_cpp_H___


#include	"ts2/c++/fwIO.h"
#include	"_ts2/c++/ts2IOdescriptor_pb.h"

#ifdef _WIN32
/* process-unique fwIO/IOCP completion-key allocator, shared by every fwIO user
   that registers with the completion port (ts2IOsockServer accept, ts2System
   child-exit) so their keys can't collide.  Windows-port design memo §E. */
int	ts2io_alloc_key();
/* internal ring buffer for the base threadpool-I/O read/write path;
   defined in ts2IOdescriptor.cpp, held by pointer so this header stays light. */
class ts2io_ring;
#endif

#endif

