

#include	"_ts2/c++/ts2IOdevNull_.h"
#include	"ts2/c++/ts2IO.h"


CLASS_TINYSTATE(ts2/c++/ts2IOdevNull,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#define BUFFER_SIZE		1000

/**
 * @brief Drains output from a ts2IO, discarding all data. / ts2IO の出力を読み捨てる tinyState クラス。
 * @details
 * Repeatedly calls `read()` on the provided `ts2IO`, discarding all data.<br>
 * Stops when `io` closes or the parent is destroyed.<br>
 * Useful for draining a stream (e.g. child stderr) that the application does not need to process.<br>
 *
 * ---
 *
 * `io` から `read()` を繰り返してデータを破棄し続ける。<br>
 * `io` が閉じるか親が destroy されると自動終了する。<br>
 * 子プロセスの stderr など、読まなくてよいストリームをバックグラウンドで吸い出すために使う。<br>
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief Create with the ts2IO stream to drain. / 読み捨て対象の ts2IO を渡して作成。
	 * @param parent Parent tinyState. / 親 tinyState。
	 * @param io     The I/O stream to drain. / 読み捨て対象の I/O ストリーム。
	 */
	ts2IOdevNull_(
		sPtr<tinyState> parent,
		sPtr<ts2IO> io);

private:
protected:
	TS_DEFARGS
	uint8_t 		buffer[BUFFER_SIZE];
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
TS_END_INTERFACE

#endif


ts2IOdevNull_::ts2IOdevNull_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/



/*******************************************
	STATE MACHINE
********************************************/


TS_STATE(INI_START)
{
	listen(parent,TSE_DESTROY);
	return rDO|ACT_PREV;
}

TS_STATE(ACT_PREV)
{
	if ( io == thNULL )
		return rDO|FIN_START;
  	if ( C_TEST(io->tinyState::state(),C_ZOM|C_FIN) )
		return rDO|FIN_START;
	io->listen(parent,TSE_DESTROY);
	return rDO|ACT_START;
}
TS_STATE(ACT_START)
{
	if ( is_destroyed() )
		return rDO|FIN_START;
int er;
	er = io->read(buffer,BUFFER_SIZE);
	if ( er<= 0 )
		return rDO|FIN_START;
	return rDO|ACT_START;
}


TS_STATE(FIN_START)
{
	if ( io.is_notNull() )
		io->destroy();
	io = thNULL;
	return rDO|FIN_TINYSTATE_START;
}



