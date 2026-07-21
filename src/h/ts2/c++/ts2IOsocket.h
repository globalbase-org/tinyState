
#ifndef ___ts2IOsocket_cpp_H___
#define ___ts2IOsocket_cpp_H___

#include	"_ts2/c++/ts2IOsocket_pb.h"

#ifdef _WIN32
/* datagram record ring for the threadpool sendto/recvfrom path;
   defined in ts2IOsocket.cpp, held by pointer so this header stays light. */
class ts2io_dgram_ring;
#endif

#endif

