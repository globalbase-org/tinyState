

#include	"_ts2/c++/ts2IOdescriptor_.h"

CLASS_TINYSTATE(ts2/c++/ts2IOdescriptor,ts2/c++/ts2IO)


#if 0

TS_BEGIN_IMPLEMENT


/**
 * @brief ファイルディスクリプタを ts2IO に包む具象クラス。/ Concrete ts2IO wrapping a file descriptor.
 * @details
 * `INI_ts2IOdescriptor_START` で fd を `O_NONBLOCK` に設定し、`fwIO` 経由でイベントループに登録する。
 * `read()`/`write()` 呼び出し時に EAGAIN が返ると sException を投げて yield し、
 * fd が準備できると再開する。`read_c` / `write_c` / `read_s` はこの基盤の上で動く。
 *
 * `ts2IOraw` / `ts2IOsocket` 系のベースクラス。
 * `ts2System` が子プロセスとのパイプとして返す `rfd`/`wfd`/`efd` も実際にはこのクラスのインスタンス。
 * / Base class for `ts2IOraw` / `ts2IOsocket` etc.
 * The `rfd`/`wfd`/`efd` pipes from `ts2System` are also instances of this class.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief fd なしで作成。fd はサブクラスが `INI_START` で設定する。/ Create without fd; subclass sets fd in `INI_START`. */
	ts2IOdescriptor_(
		sPtr<tinyState> parent);
	void inherit(
		sPtr<tinyState> parent);
	/** @brief 既存の fd を渡して作成。`ts2IOsockTCPserver::accept()` など既に開かれた fd 向け。
	 *  / Create with an already-open fd (e.g. from `ts2IOsockTCPserver::accept()`). */
	ts2IOdescriptor_(
		sPtr<tinyState> parent,
		int _fd);
	void inherit(
		sPtr<tinyState> parent,
		int _fd);

	virtual int read(void * buf,int length);
	virtual int write(void * buf,int length);
	virtual INTEGER64 seek(INTEGER64 pos,int where);
private:
protected:
	/** @brief 管理するファイルディスクリプタ。/ The managed file descriptor. */
	int fd;
	int nonBlockFlag;
	/** @brief fwIO へのポインタ。EAGAIN 時の I/O 待ち登録に使う。/ fwIO handle for EAGAIN wait registration. */
	sPtr<fwIO>	io;
};

TS_END_IMPLEMENT

#endif


ts2IOdescriptor_::ts2IOdescriptor_(
		sPtr<tinyState> _parent)
        : ts2IO_(_parent)
{
	fd = -1;
}

void
ts2IOdescriptor_::inherit(
	sPtr<tinyState> _parent)
{
	this->TS_BASECLASS::inherit(_parent);
}

ts2IOdescriptor_::ts2IOdescriptor_(
		sPtr<tinyState> _parent,int _fd)
        : ts2IO_(_parent)
{
	fd = _fd;
}

void
ts2IOdescriptor_::inherit(
	sPtr<tinyState> _parent,int _fd)
{
	this->TS_BASECLASS::inherit(_parent);
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/




int
ts2IOdescriptor_::read(void * buf,int length)
{
int er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; ; ) {
		er = ::read(fd,buf,length);
		if ( er >= 0 )
			return er;
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
//::printf("EAGAIN--\n");
		  	io->read(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_read(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}

int
ts2IOdescriptor_::write(void * buf,int length)
{
int er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; ; ) {
		er = ::write(fd,buf,length);
		if ( er >= 0 )
			return er;
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
		  	io->write(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return !io->is_write(caller);
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
	}
}
INTEGER64
ts2IOdescriptor_::seek(INTEGER64 pos,int where)
{
INTEGER64 er;
	if ( C_TEST(tinyState_::state(),C_ZOM|C_FIN) ) {
		err = EBADF;
		return -1;
	}
	for ( ; ; ) {
		er = ::lseek(fd,pos,where);
		if ( er >= 0 )
			return er;
		switch ( errno ) {
		case EINTR:
			continue;
		case EAGAIN:
		  	io->read(sCallSection::key->caller(),fd);
			throw sException([this](sPtr<tinyState> caller) {
				return 1;
			});
		}
		err = errno;
		state = TS2IO_ERROR;
		return er;
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
int flags;
	flags = fcntl(fd,F_GETFL,0);
	flags |= O_NONBLOCK;
	fcntl(fd,F_SETFL,flags);

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
		io->abort(fd);
	soCLOSE(fd);
	return rDO|FIN_ts2IO_START;
}

