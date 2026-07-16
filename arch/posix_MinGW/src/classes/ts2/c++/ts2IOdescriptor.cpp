/*
 * ts2IOdescriptor — MinGW (HANDLE + overlapped I/O) implementation.  Windows-port design memo §C.
 *
 * POSIX wraps an int fd with ::read/::write and, on EAGAIN, registers the fd
 * with fwIO(select) and yields.  Windows has no readiness-wait for pipe/file
 * HANDLEs, so this wraps a HANDLE and drives *overlapped* reads/writes through
 * fwIO's IOCP:
 *
 *   - INI associates the HANDLE with fwIO's completion port, keyed by a unique
 *     per-object id (fdid).  read()/write() issue overlapped ops; when they
 *     cannot complete synchronously they register (io->read/write(caller,fdid))
 *     and throw sException — EXACTLY the POSIX contract, just "overlapped
 *     pending" instead of "EAGAIN".  The completion arrives at fwIO keyed by
 *     fdid, which resumes the caller; on resume read() serves the bytes that
 *     the kernel already placed in the prefetch buffer.
 *   - read_c/write_c/read_s/write_s (ts2IO base) are UNTOUCHED; only read/write/
 *     seek are overridden (the override boundary, Windows-port design memo §1c).
 *
 * FIRST CUT (2026-07-12): compiles; uniform "always yield, completion drives"
 * (no FILE_SKIP_COMPLETION_PORT_ON_SUCCESS) for simplicity.  End-to-end run
 * verification pending on the real Windows box.
 */

/* WIN32_LEAN_AND_MEAN before <windows.h> keeps legacy <winsock.h> out, so
 * <winsock2.h> (pulled by std2/includes.h) stays the sole winsock. Windows-port design memo */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include	<windows.h>
#include	<errno.h>
#include	"_ts2/c++/ts2IOdescriptor_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOdescriptor,ts2/c++/ts2IO)

/* unique id per descriptor, used as the IOCP completion key and as the fwIO
   registration key so a completion can be matched back to this object. */
static volatile LONG	g_ts2io_fdid = 0;

/* shared key allocator (declared in ts2IOdescriptor.h) for non-descriptor fwIO
   users (e.g. ts2System child-exit), drawing from the same space. Windows-port design memo §E. */
int ts2io_alloc_key() { return (int)InterlockedIncrement(&g_ts2io_fdid); }

#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief HANDLE を ts2IO に包む具象クラス (MinGW)。/ Concrete ts2IO wrapping a Win32 HANDLE.
 * @details overlapped I/O + IOCP。read_c/write_c/read_s はこの上で動く。Windows-port design memo §C。
 * ts2IOraw / ts2System のパイプ端のベース。
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2IOdescriptor_(
		sPtr<tinyState> parent);
	void inherit(
		sPtr<tinyState> parent);
	/** @brief 既存の HANDLE を渡して作成 (ts2System のパイプ端など)。 */
	ts2IOdescriptor_(
		sPtr<tinyState> parent,
		HANDLE _fd);
	void inherit(
		sPtr<tinyState> parent,
		HANDLE _fd);

	virtual int read(void * buf,int length);
	virtual int write(void * buf,int length);
	virtual INTEGER64 seek(INTEGER64 pos,int where);
protected:
	/** @brief 管理する HANDLE。/ The managed HANDLE. */
	HANDLE		fd;
	/** @brief IOCP 完了キー兼 fwIO 登録キー (この object 固有)。 */
	int		fdid;
	sPtr<fwIO>	io;
	int		associated;

	/* one overlapped per direction; the op reads/writes straight into the
	   caller's (member) buffer — no internal buffering here.  Buffer/framing is
	   read_c/write_c's job (or higher).  Windows-port design memo §C. */
	OVERLAPPED	read_ov;
	OVERLAPPED	write_ov;
	int		read_pending;
	int		write_pending;

	void		ensure_assoc();
};

TS_END_IMPLEMENT

#endif


ts2IOdescriptor_::ts2IOdescriptor_(
		sPtr<tinyState> _parent)
        : ts2IO_(_parent)
{
	fd = INVALID_HANDLE_VALUE;
	fdid = (int)InterlockedIncrement(&g_ts2io_fdid);
	associated = 0;
	read_pending = write_pending = 0;
}

void
ts2IOdescriptor_::inherit(
	sPtr<tinyState> _parent)
{
	this->TS_BASECLASS::inherit(_parent);
}

ts2IOdescriptor_::ts2IOdescriptor_(
		sPtr<tinyState> _parent,HANDLE _fd)
        : ts2IO_(_parent)
{
	fd = _fd;
	fdid = (int)InterlockedIncrement(&g_ts2io_fdid);
	associated = 0;
	read_pending = write_pending = 0;
}

void
ts2IOdescriptor_::inherit(
	sPtr<tinyState> _parent,HANDLE _fd)
{
	this->TS_BASECLASS::inherit(_parent);
}


/* associate our HANDLE with fwIO's IOCP port, keyed by fdid (once). */
void
ts2IOdescriptor_::ensure_assoc()
{
	if ( associated )
		return;
	if ( fd != INVALID_HANDLE_VALUE && io.is_notNull() ) {
		CreateIoCompletionPort(fd, (HANDLE)io->port(), (ULONG_PTR)fdid, 0);
		associated = 1;
	}
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/

int
ts2IOdescriptor_::read(void * buf,int length)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	/* resume path: the overlapped read (issued straight into the caller's
	   member buffer) has been driven to completion by fwIO's IOCP. */
	if ( read_pending ) {
	DWORD got = 0;
		if ( GetOverlappedResult(fd,&read_ov,&got,FALSE) ) {
			read_pending = 0;
			return (int)got;			/* 0 == EOF */
		}
	DWORD e = GetLastError();
		if ( e == ERROR_IO_INCOMPLETE ) {
			io->read(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_read(caller); });
		}
		read_pending = 0;
		if ( e == ERROR_HANDLE_EOF || e == ERROR_BROKEN_PIPE )
			return 0;
		err = e; state = TS2IO_ERROR; return -1;
	}
	/* first call: issue one overlapped read directly into the caller's buffer.
	   No data yet -> register with fwIO + yield (== POSIX EAGAIN path). */
	ensure_assoc();
	memset(&read_ov,0,sizeof(read_ov));
	{
	DWORD got = 0;
		if ( ReadFile(fd,buf,(DWORD)length,&got,&read_ov) ||
				GetLastError() == ERROR_IO_PENDING ) {
			read_pending = 1;
			io->read(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_read(caller); });
		}
	DWORD e = GetLastError();
		if ( e == ERROR_HANDLE_EOF || e == ERROR_BROKEN_PIPE )
			return 0;
		err = e; state = TS2IO_ERROR; return -1;
	}
}

int
ts2IOdescriptor_::write(void * buf,int length)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	/* resume path: overlapped write (from the caller's buffer) has drained. */
	if ( write_pending ) {
	DWORD got = 0;
		if ( GetOverlappedResult(fd,&write_ov,&got,FALSE) ) {
			write_pending = 0;
			return (int)got;			/* bytes actually written */
		}
	DWORD e = GetLastError();
		if ( e == ERROR_IO_INCOMPLETE ) {
			io->write(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_write(caller); });
		}
		write_pending = 0;
		err = e; state = TS2IO_ERROR; return -1;
	}
	/* first call: issue one overlapped write straight from the caller's buffer.
	   Can't complete now -> register + yield.  write_c writes the remainder. */
	ensure_assoc();
	memset(&write_ov,0,sizeof(write_ov));
	{
	DWORD got = 0;
		if ( WriteFile(fd,buf,(DWORD)length,&got,&write_ov) ||
				GetLastError() == ERROR_IO_PENDING ) {
			write_pending = 1;
			io->write(sCallSection::key->caller(),fdid);
			throw sException([this](sPtr<tinyState> caller){ return !io->is_write(caller); });
		}
		err = GetLastError(); state = TS2IO_ERROR; return -1;
	}
}

INTEGER64
ts2IOdescriptor_::seek(INTEGER64 pos,int where)
{
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	{
	LARGE_INTEGER li, out;
		li.QuadPart = pos;
		if ( !SetFilePointerEx(fd, li, &out, (DWORD)where) ) {
			err = GetLastError(); state = TS2IO_ERROR; return -1;
		}
		return (INTEGER64)out.QuadPart;
	}
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	return rDO|INI_ts2IOdescriptor_START;
}
TS_STATE(INI_ts2IOdescriptor_START)
{
	io = io.d_cast(application->fw());
	ensure_assoc();
	state = TS2IO_ACTIVE;
	return rDO|INI_ts2IO_START;
}


TS_STATE(FIN_START)
{
	return rDO|FIN_ts2IOdescriptor_START;
}
TS_THREAD(FIN_ts2IOdescriptor_START)
{
	state = TS2IO_CLOSE;
	if ( io.is_notNull() )
		io->detach(ifThis);
	if ( fd != INVALID_HANDLE_VALUE )
		CloseHandle(fd);
	fd = INVALID_HANDLE_VALUE;
	return rDO|FIN_ts2IO_START;
}
