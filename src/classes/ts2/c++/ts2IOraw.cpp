

#include	"_ts2/c++/ts2IOraw_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOraw,ts2/c++/ts2IOdescriptor)


#if 0

TS_BEGIN_IMPLEMENT


/**
 * @brief ファイルパスを open してバイトストリームとして扱う ts2IO 実装。/ ts2IO that opens a file path as a byte stream.
 * @details
 * `INI_START` (TS_THREAD) で `open(filename, flags, mode)` を実行し、
 * 取得した fd を `ts2IOdescriptor` に渡して O_NONBLOCK 化・fwIO 登録を行う。
 *
 * `destroy()` は `ts2IOdescriptor::destroy()` に加えて `THR_KILL(SIGPIPE)` を送り、
 * TS_THREAD 内の blocking open を安全に中断する。
 *
 * @par 使用例 / Example
 * @code{.cpp}
 * rfd = thNEW(ts2IOraw, (ifThis, thNEW(stdString, ("/path/to/file")), O_RDONLY));
 * @endcode
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief ファイルパスと open フラグを指定して作成。/ Create with file path and open flags.
	 * @param parent   親 tinyState。/ Parent tinyState.
	 * @param filename open するファイルパス。/ File path to open.
	 * @param flags    open(2) に渡すフラグ (`O_RDONLY`, `O_WRONLY|O_CREAT` 等)。/ Flags for open(2).
	 * @param mode     ファイル作成時のパーミッション。`O_CREAT` がない場合は無視される。/ Permission bits; ignored unless `O_CREAT` is set.
	 */
	ts2IOraw_(
		sPtr<tinyState> parent,
		sPtr<stdString> filename,
		int flags,
		int mode=0);
	void inherit(
		sPtr<tinyState> parent,
		sPtr<stdString> filename,
		int flags,
		int mode);
	/** @brief destroy 時に `THR_KILL(SIGPIPE)` を送って TS_THREAD 内の open を中断する。
	 *  / Sends `THR_KILL(SIGPIPE)` to abort a blocking open in the TS_THREAD. */
	virtual void destroy();
private:
protected:
	sPtr<stdString> 	filename;
	int flags;
	int mode;
};

TS_END_IMPLEMENT

#endif


ts2IOraw_::ts2IOraw_(
		sPtr<tinyState> _parent,
		sPtr<stdString> filename,
		int flags,
		int mode)
        : ts2IOdescriptor_(_parent)
{
	this->filename = filename;
	this->flags = flags;
	this->mode = mode;
}

void
ts2IOraw_::inherit(
	sPtr<tinyState> _parent,
		sPtr<stdString> filename,
		int flags,
		int mode)
{
	this->TS_BASECLASS::inherit(_parent);
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/

void
ts2IOraw_::destroy()
{
	ts2IOdescriptor_::destroy();
#ifndef _WIN32
	THR_KILL(SIGPIPE);	/* Windows: overlapped cancel handles this (Windows-port design memo §4) */
#endif
}


/*******************************************
	STATE MACHINE
********************************************/


TS_THREAD(INI_START)
{
#ifdef _WIN32
	/* MinGW: CreateFile gives an overlapped HANDLE (what ts2IOdescriptor drives
	   through IOCP).  Basic POSIX-open -> CreateFile flag mapping.  Windows-port design memo §C. */
	{
	DWORD access;
	int am = flags & O_ACCMODE;
		if ( am == O_WRONLY )		access = GENERIC_WRITE;
		else if ( am == O_RDWR )	access = GENERIC_READ | GENERIC_WRITE;
		else				access = GENERIC_READ;
	DWORD disp = (flags & O_CREAT) ? OPEN_ALWAYS : OPEN_EXISTING;
	HANDLE h = CreateFileA(filename->get_str(), access,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, disp,
			FILE_FLAG_OVERLAPPED, NULL);
		if ( h == INVALID_HANDLE_VALUE || is_destroyed() ) {
			err = (int)GetLastError();
			state = TS2IO_ERROR;
			return rDO|FIN_START;
		}
		fd = h;
	}
	return rDO|INI_ts2IOdescriptor_START;
#else
int er;
	THR_CATCH_KILL(
		er = soOPEN(filename->get_str(),flags,mode);
	,
		er = -1;
		errno = EINTR;
	);
	if ( er < 0 || is_destroyed() ) {
		err = errno;
		state = TS2IO_ERROR;
		return rDO|FIN_START;
	}
	fd = er;
	return rDO|INI_ts2IOdescriptor_START;
#endif
}


