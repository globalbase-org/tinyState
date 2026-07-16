
#include	<unistd.h>
#include	"_ts2/c++/tsSignal_.h"

CLASS_TINYSTATE(tsSignal,tinyState)

#if 0
TS_BEGIN_IMPLEMENT

class tsSignal;

/**
 * @brief UNIX シグナルを tinyState イベント (TSE_SIGNAL) に変換する。/ Converts a UNIX signal to a tinyState event (TSE_SIGNAL).
 * @details
 * `tsSignalCore` にシグナルハンドラを登録し、シグナル受信時に
 * `invoke_listen` と `parent->eventHandler` 経由で `TSE_SIGNAL` を送る。
 * 同一シグナル番号の `tsSignal` が複数あってもハンドラは共有される (tsSignalCore が管理)。
 * / Registers a handler in `tsSignalCore`; on signal receipt, dispatches `TSE_SIGNAL` via
 * `invoke_listen` and `parent->eventHandler`. Multiple `tsSignal` instances for the same
 * signal number share one handler (managed by `tsSignalCore`).
 *
 * @par 使用例 / Example
 * @code{.cpp}
 * sig = thNEW(tsSignal, (ifThis, SIGINT));
 * // TSE_SIGNAL を受け取るには listen が必要 (sig->listen や parent への転送)
 * @endcode
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	tsSignal_(
		sPtr<tinyState>  parent,
		int sig);
	void inherit(
		sPtr<tinyState>  parent,
		int sig);
	/** @brief シグナルを手動で発火する (テスト用)。/ Manually fire the signal (for testing).
	 *  @details 実際のシグナルを送らずに TSE_SIGNAL を emit する。/ Emits TSE_SIGNAL without sending an actual UNIX signal.
	 */
	void send_signal();

	int			sig; ///< 待ち受けるシグナル番号 (例: SIGINT, SIGTERM)。/ Signal number being watched (e.g. SIGINT, SIGTERM).

private:
	sPtr<tsSignalCore> 		core;
};
TS_END_IMPLEMENT
#endif


/*******************************************
	PUBLIC FUNCTIONS
********************************************/

tsSignal_::tsSignal_(
	sPtr<tinyState>  parent,
	int sig)
	:
	tinyState_(parent)
{
	this->sig = sig;
}

void
tsSignal_::inherit(
	sPtr<tinyState>  parent,
	int sig)
{
	this->TS_BASECLASS::inherit(parent);
}


void
tsSignal_::send_signal()
{
	if ( C_TEST(this->state(),C_ZOM|C_FIN) )
		return;
	this->invoke_listen(thNEW( stdEvent,(TSE_SIGNAL,ifThis,(INTEGER64)sig)));
	this->parent->eventHandler(
		thNEW( stdEvent,(TSE_SIGNAL,ifThis,(INTEGER64)sig)));
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{

	this->core =  tsSignalCore::search_signal(this->sig);
	if ( this->core == thNULL )
		this->core =  thNEW( tsSignalCore,(
			ifThis,
			this->sig));
	this->core->ins_front(ifThis);
	return ACT_START;
}
TS_STATE(FIN_START)
{
	this->core->del_front(ifThis);
	this->core = thNULL;
	return rDO|FIN_TINYSTATE_START;
}

