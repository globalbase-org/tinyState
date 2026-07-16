


#ifndef ___INCLUDES_H___
#define ___INCLUDES_H___

/*
 * MinGW (x86_64-w64-mingw32) variant of std2/includes.h — overrides the base
 * posix version (arch/posix/src/h/std2/includes.h), which pulls a lot of POSIX
 * headers that do not exist on MinGW (sys/socket.h, netinet/in.h, arpa/inet.h,
 * netdb.h, termios.h, sys/uio.h, sys/ioctl.h).
 *
 * Here we substitute winsock2/ws2tcpip for the BSD-socket headers and drop the
 * unavailable terminal/uio/ioctl headers (those are handled per-file in the
 * posix_MinGW overlays). See Windows-port design memo (MinGW port design).
 *
 * FIRST CUT (2026-07-11, groundwork): compiles the common headers; individual
 * POSIX-symbol gaps are surfaced by the cross build and ported per file.
 */

/* winsock must come before <windows.h>; ws2tcpip for getaddrinfo/inet_ntop */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX          /* keep min()/max() macros out of the way */
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/* windows.h / COM headers define these as macros that collide with tinyState
   identifiers (THIS is a parameter name across the codebase). Undo them. Windows-port design memo */
#undef THIS
#undef THIS_

/* POSIX scatter/gather socket types (struct iovec/msghdr/mmsghdr) that MinGW
   lacks — winsock has WSABUF/WSAMSG instead.  Supplied MinGW-wide here so
   ts2IOsocket_'s mmsg fallback member (WSABUF[TS2_MMSG_MAXIOV]) and any
   subclass TU that includes its layout resolve.  Windows-port design memo throughput step #1. */
#include "std2/ts_mmsg.h"

#include <math.h>
#include <stdint.h>
#include "tinyState_config.h"
#include	<errno.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<strings.h>
#include 	<sys/types.h>
#include 	<sys/time.h>
/* <sys/socket.h> <netinet/in.h> <arpa/inet.h> <netdb.h>  -> winsock2/ws2tcpip above */
/* <sys/ioctl.h>  -> ioctlsocket() etc. per-file */
#include 	<unistd.h>
/* <termios.h>    -> no terminal layer on MinGW (DM_TTY stubbed) */
#include	<sys/stat.h>
#include	<fcntl.h>
#include 	<pthread.h>
#include	<dirent.h>
/* <sys/uio.h>    -> readv/writev shimmed per-file */
#include	<stdarg.h>
#include	<signal.h>
#include	<process.h>
#include	<io.h>

/* --- POSIX signal numbers absent on MinGW ---
 * tinyState apps may reference these (e.g. mask SIGPIPE via tsSignal).  On
 * Windows only SIGINT/SIGTERM actually fire (mapped from console events in
 * tsSignalCore); tsSignal *accepts* any of these but the unmapped ones are a
 * silent no-op (their tsSignalCore is registered but never signalled).  Define
 * dummy numbers (guarded — MinGW already defines SIGINT/SIGTERM/SIGABRT/…) so
 * such app code compiles.  Windows-port design memo. */
#ifndef SIGHUP
#define SIGHUP   1
#endif
#ifndef SIGQUIT
#define SIGQUIT  3
#endif
#ifndef SIGKILL
#define SIGKILL  9
#endif
#ifndef SIGUSR1
#define SIGUSR1  10
#endif
#ifndef SIGUSR2
#define SIGUSR2  12
#endif
#ifndef SIGPIPE
#define SIGPIPE  13
#endif
#ifndef SIGALRM
#define SIGALRM  14
#endif
#ifndef SIGCHLD
#define SIGCHLD  17
#endif

/* --- rand_r (POSIX reentrant rand) absent on MinGW ---
 * classic glibc LCG so thread-safe rand_r(seed) app code builds. */
static inline int ts_rand_r(unsigned int * __seed)
{
	*__seed = *__seed * 1103515245u + 12345u;
	return (int)((*__seed >> 16) & 0x7fff);
}
#ifndef rand_r
#define rand_r ts_rand_r
#endif

#include	"std2/m_include.h"


#define ___us		((INTEGER64)1)
#define ___ms		(1000*___us)
#define ___sec		(1000*___ms)
#define ___min		(60*___sec)
#define ___h		(60*___min)
#define ___day		(24*___h)
#endif
