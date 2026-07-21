#ifndef ___ts_rio_H___
#define ___ts_rio_H___

/*
 * ts_rio.h — Winsock Registered I/O (RIO) ABI for MinGW.
 *
 * RIO is the Windows high-throughput socket path (registered buffers + user-
 * mapped request/completion queues, batching many datagram ops into ~one
 * syscall — the Windows counterpart of Linux sendmmsg / macOS sendmsg_x).  It is
 * the default datagram path of ts2IOsockUDP on Windows (degrading to plain
 * overlapped when RIO is unavailable).  See the throughput roadmap
 * (socket-ipc-throughput-roadmap) and the RIO design memo.
 *
 * MinGW's headers declare only WSA_FLAG_REGISTERED_IO (winsock2.h); the RIO
 * types, GUID and ioctl live in the Windows SDK's mswsockdef.h / mswsock.h,
 * which MinGW does not ship.  RIO functions are reached at runtime via
 * WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER) — the same
 * function-pointer-table手口 used for AcceptEx / WSARecvMsg — so we only need
 * to self-declare the ABI here (no import library).  Field order/types/calling
 * convention match the SDK exactly.
 *
 * ── v2 ──────────────────────────────────────────────────────────────
 * This header is now PURE ABI.  Under the threadpool record-ring base,
 * sockets no longer touch fwIO's IOCP, so the old process-global "reactor glue"
 * (a shared RIO_CQ bound to fwIO::port(), drained in fwIO::loop() by fdid) is
 * gone.  Instead each ENHANCED socket owns its own RIO_CQ signalled by an EVENT
 * (RIO_EVENT_COMPLETION) that a threadpool-wait (RegisterWaitForSingleObject)
 * bridges into the object's state machine — see ts2IOsocket.cpp and
 * docs/COOKBOOK.md §6.7.  All that per-socket glue lives in ts2IOsocket.cpp;
 * here we only declare the ABI so the RIO_* member types resolve wherever
 * ts2IOsocket_'s generated layout is included.
 */

#ifdef _WIN32

#include	<winsock2.h>	/* SOCKET, WSA_FLAG_REGISTERED_IO, _WSAIORW, IOC_WS2 */
#include	<ws2def.h>
#include	<windows.h>	/* HANDLE, PVOID, DWORD, ULONG, LONG, BOOL */

/* --- ioctl to fetch the RIO function table (absent from MinGW winsock2.h) --- */
#ifndef SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER
#define SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER	_WSAIORW(IOC_WS2,36)
#endif

/* --- opaque RIO handles --- */
typedef struct RIO_BUFFERID_t *	RIO_BUFFERID, **PRIO_BUFFERID;
typedef struct RIO_CQ_t *	RIO_CQ,       **PRIO_CQ;
typedef struct RIO_RQ_t *	RIO_RQ,       **PRIO_RQ;

#define RIO_INVALID_BUFFERID	((RIO_BUFFERID)0xFFFFFFFF)
#define RIO_INVALID_CQ		((RIO_CQ)0)
#define RIO_INVALID_RQ		((RIO_RQ)0)
#define RIO_MAX_CQ_SIZE		(0x8000000)
#define RIO_CORRUPT_CQ		(0xFFFFFFFF)

/* --- send/recv flags --- */
#define RIO_MSG_DONT_NOTIFY	0x00000001
#define RIO_MSG_DEFER		0x00000002
#define RIO_MSG_WAITALL		0x00000004
#define RIO_MSG_COMMIT_ONLY	0x00000008

/* --- completion result (one per dequeued op) --- */
typedef struct _RIORESULT {
	LONG		Status;
	ULONG		BytesTransferred;
	ULONGLONG	SocketContext;
	ULONGLONG	RequestContext;
} RIORESULT, *PRIORESULT;

/* --- a slice of a registered buffer (all RIO I/O references buffers this way) --- */
typedef struct _RIO_BUF {
	RIO_BUFFERID	BufferId;
	ULONG		Offset;
	ULONG		Length;
} RIO_BUF, *PRIO_BUF;

/* --- how a completion queue signals readiness --- */
typedef enum _RIO_NOTIFICATION_COMPLETION_TYPE {
	RIO_EVENT_COMPLETION	= 1,
	RIO_IOCP_COMPLETION	= 2
} RIO_NOTIFICATION_COMPLETION_TYPE, *PRIO_NOTIFICATION_COMPLETION_TYPE;

typedef struct _RIO_NOTIFICATION_COMPLETION {
	RIO_NOTIFICATION_COMPLETION_TYPE	Type;
	union {
		struct {
			HANDLE	EventHandle;
			BOOL	NotifyReset;
		} Event;
		struct {
			HANDLE	IocpHandle;
			PVOID	CompletionKey;
			PVOID	Overlapped;
		} Iocp;
	};
} RIO_NOTIFICATION_COMPLETION, *PRIO_NOTIFICATION_COMPLETION;

/* --- RIO extension function pointer types (WINAPI = __stdcall) --- */
typedef BOOL	(WINAPI * LPFN_RIORECEIVE)(RIO_RQ SocketQueue, PRIO_BUF pData,
			ULONG DataBufferCount, DWORD Flags, PVOID RequestContext);
typedef BOOL	(WINAPI * LPFN_RIORECEIVEEX)(RIO_RQ SocketQueue, PRIO_BUF pData,
			ULONG DataBufferCount, PRIO_BUF pLocalAddress,
			PRIO_BUF pRemoteAddress, PRIO_BUF pControlContext,
			PRIO_BUF pFlags, DWORD Flags, PVOID RequestContext);
typedef BOOL	(WINAPI * LPFN_RIOSEND)(RIO_RQ SocketQueue, PRIO_BUF pData,
			ULONG DataBufferCount, DWORD Flags, PVOID RequestContext);
typedef BOOL	(WINAPI * LPFN_RIOSENDEX)(RIO_RQ SocketQueue, PRIO_BUF pData,
			ULONG DataBufferCount, PRIO_BUF pLocalAddress,
			PRIO_BUF pRemoteAddress, PRIO_BUF pControlContext,
			PRIO_BUF pFlags, DWORD Flags, PVOID RequestContext);
typedef VOID	(WINAPI * LPFN_RIOCLOSECOMPLETIONQUEUE)(RIO_CQ CQ);
typedef RIO_CQ	(WINAPI * LPFN_RIOCREATECOMPLETIONQUEUE)(DWORD QueueSize,
			PRIO_NOTIFICATION_COMPLETION NotificationCompletion);
typedef RIO_RQ	(WINAPI * LPFN_RIOCREATEREQUESTQUEUE)(SOCKET Socket,
			ULONG MaxOutstandingReceive, ULONG MaxReceiveDataBuffers,
			ULONG MaxOutstandingSend, ULONG MaxSendDataBuffers,
			RIO_CQ ReceiveCQ, RIO_CQ SendCQ, PVOID SocketContext);
typedef ULONG	(WINAPI * LPFN_RIODEQUEUECOMPLETION)(RIO_CQ CQ,
			PRIORESULT Array, ULONG ArraySize);
typedef VOID	(WINAPI * LPFN_RIODEREGISTERBUFFER)(RIO_BUFFERID BufferId);
typedef INT	(WINAPI * LPFN_RIONOTIFY)(RIO_CQ CQ);
typedef RIO_BUFFERID (WINAPI * LPFN_RIOREGISTERBUFFER)(PCHAR DataBuffer,
			DWORD DataLength);
typedef BOOL	(WINAPI * LPFN_RIORESIZECOMPLETIONQUEUE)(RIO_CQ CQ,
			DWORD QueueSize);
typedef BOOL	(WINAPI * LPFN_RIORESIZEREQUESTQUEUE)(RIO_RQ RQ,
			DWORD MaxOutstandingReceive, DWORD MaxOutstandingSend);

/* --- the table filled by WSAIoctl(SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER) --- */
typedef struct _RIO_EXTENSION_FUNCTION_TABLE {
	DWORD				cbSize;
	LPFN_RIORECEIVE			RIOReceive;
	LPFN_RIORECEIVEEX		RIOReceiveEx;
	LPFN_RIOSEND			RIOSend;
	LPFN_RIOSENDEX			RIOSendEx;
	LPFN_RIOCLOSECOMPLETIONQUEUE	RIOCloseCompletionQueue;
	LPFN_RIOCREATECOMPLETIONQUEUE	RIOCreateCompletionQueue;
	LPFN_RIOCREATEREQUESTQUEUE	RIOCreateRequestQueue;
	LPFN_RIODEQUEUECOMPLETION	RIODequeueCompletion;
	LPFN_RIODEREGISTERBUFFER	RIODeregisterBuffer;
	LPFN_RIONOTIFY			RIONotify;
	LPFN_RIOREGISTERBUFFER		RIORegisterBuffer;
	LPFN_RIORESIZECOMPLETIONQUEUE	RIOResizeCompletionQueue;
	LPFN_RIORESIZEREQUESTQUEUE	RIOResizeRequestQueue;
} RIO_EXTENSION_FUNCTION_TABLE, *PRIO_EXTENSION_FUNCTION_TABLE;

/* GUID passed to WSAIoctl to obtain the table above. */
#ifndef WSAID_MULTIPLE_RIO
#define WSAID_MULTIPLE_RIO	{0x8509e081,0x96dd,0x4005,{0xb1,0x65,0x9e,0x2e,0xe8,0xc7,0x9e,0x3f}}
#endif

#endif /* _WIN32 */

#endif /* ___ts_rio_H___ */
