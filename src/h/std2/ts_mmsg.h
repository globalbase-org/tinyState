
#ifndef ___ts_mmsg_H___
#define ___ts_mmsg_H___

/*
 * ts_mmsg.h — portable batch-message types for ts2IOsocket::recvmmsg/sendmmsg.
 * Windows-port design memo throughput roadmap step #1 (see socket-ipc-throughput-roadmap).
 *
 * The scatter/gather message vector API is POSIX (`struct mmsghdr` /
 * `struct msghdr` / `struct iovec`).  We adopt the POSIX shape as the
 * interface type on BOTH platforms:
 *
 *   - Linux: the kernel's own structs (via <sys/socket.h> with _GNU_SOURCE);
 *     the caller's mmsghdr[] is passed straight to ::recvmmsg / ::sendmmsg with
 *     zero translation — the whole point of step #1 ("Linux 即効").
 *   - Windows (MinGW): winsock has WSAMSG/WSABUF but NOT POSIX msghdr/iovec/
 *     mmsghdr, so we define them here with the exact Linux field names/layout.
 *     ts2IOsocket (MinGW) translates each element to a WSAMSG and calls
 *     WSARecvMsg / WSASendMsg one datagram at a time (throughput unchanged —
 *     the accepted fallback; this only matches the API shape).
 *
 * Only pointer-to-mmsghdr appears in the generated _pb.h signature, so callers
 * that never touch the struct need not include this; those that build a
 * message vector include it and get the right definition per platform.
 */

#ifdef _WIN32

#include	<winsock2.h>	/* WSAMSG/WSABUF; must precede <windows.h> */
#include	<ws2tcpip.h>	/* socklen_t */
#include	<stddef.h>	/* size_t */
#include	<time.h>	/* struct timespec (recvmmsg timeout; ignored) */

/* Max scatter/gather elements the Windows per-datagram fallback translates per
   message (WSABUF array size held in ts2IOsocket_).  Datagram apps use 1; deep
   scatter beyond this returns EMSGSIZE on Windows. */
#define TS2_MMSG_MAXIOV		16

/* MinGW winsock provides no POSIX msghdr/iovec/mmsghdr — supply them with the
   Linux field names and layout so ts2IOsocket's signature and caller code are
   source-identical across platforms. */
struct iovec {
	void *		iov_base;	/* base address of a memory region */
	size_t		iov_len;	/* size of the memory region */
};

struct msghdr {
	void *		msg_name;	/* optional address */
	socklen_t	msg_namelen;	/* size of address */
	struct iovec *	msg_iov;	/* scatter/gather array */
	size_t		msg_iovlen;	/* # elements in msg_iov */
	void *		msg_control;	/* ancillary data (cmsg) */
	size_t		msg_controllen;	/* ancillary data buffer len */
	int		msg_flags;	/* flags on received message */
};

struct mmsghdr {
	struct msghdr	msg_hdr;	/* message header */
	unsigned int	msg_len;	/* # bytes transmitted for this hdr */
};

#else /* POSIX */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE		/* recvmmsg/sendmmsg + struct mmsghdr (glibc) */
#endif
#include	<sys/socket.h>
#include	<time.h>

#if defined(__CYGWIN__) || defined(__APPLE__)
/* Cygwin and macOS (BSD) provide POSIX msghdr/iovec + recvmsg/sendmsg but NOT
   the Linux `struct mmsghdr` type.  Define just the vector wrapper so the
   ts2IOsocket signature and caller code are source-identical.  Cygwin then
   falls back to a per-datagram recvmsg/sendmsg loop (arch/posix, __CYGWIN__);
   macOS uses the real batched syscalls below. */
struct mmsghdr {
	struct msghdr	msg_hdr;
	unsigned int	msg_len;
};
#endif

#ifdef __APPLE__
/* macOS/Darwin DOES have a batched-datagram syscall pair — sendmsg_x /
   recvmsg_x (struct msghdr_x []) — the Darwin equivalent of Linux
   sendmmsg/recvmmsg.  But the CommandLineTools SDK headers do NOT declare them
   or struct msghdr_x (they live in XNU bsd/sys/socket.h behind
   __APPLE_API_PRIVATE and are stripped from the shipped SDK).  The symbols ARE
   exported by libSystem, so we self-declare the XNU ABI here and link against
   the default libSystem.  Verified on macOS 14.8.3/arm64: both resolve at link
   time.  ts2IOsocket (arch/posix, __APPLE__ branch) translates
   struct mmsghdr[] <-> struct msghdr_x[] and issues ONE batched syscall — the
   real mmsg acceleration for macOS, not the per-datagram fallback.

   struct msghdr_x == POSIX struct msghdr + a trailing size_t msg_datalen (the
   per-message byte count, analogous to mmsghdr.msg_len).  Field order/types
   match XNU exactly (note msg_iovlen is int, as in the BSD msghdr). */
#include	<sys/types.h>	/* ssize_t */

#define TS2_MMSG_MAXMSG	64	/* struct msghdr_x scratch cap per batched call */

struct msghdr_x {
	void *		msg_name;	/* optional address */
	socklen_t	msg_namelen;	/* size of address */
	struct iovec *	msg_iov;	/* scatter/gather array */
	int		msg_iovlen;	/* # elements in msg_iov */
	void *		msg_control;	/* ancillary data (cmsg) */
	socklen_t	msg_controllen;	/* ancillary data buffer len */
	int		msg_flags;	/* flags on received message */
	size_t		msg_datalen;	/* byte length transferred for this hdr */
};

#ifdef __cplusplus
extern "C" {
#endif
ssize_t sendmsg_x(int, const struct msghdr_x *, unsigned int, int);
ssize_t recvmsg_x(int, struct msghdr_x *, unsigned int, int);
#ifdef __cplusplus
}
#endif
#endif /* __APPLE__ */

#endif

#endif /* ___ts_mmsg_H___ */
