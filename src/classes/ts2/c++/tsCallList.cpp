

#include	"_ts2/c++/tsCallList_.h"

CLASS_TINYSTATE(ts2/c++/tsCallList,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


/**
 * @brief 複数の tinyState を並列実行して全完了を待つコーディネータ。/ Coordinator that runs multiple tinyStates in parallel and waits for all to finish.
 * @details
 * `call()` で実行中の tinyState を登録し、`finish()` を呼んだ後に全部の TSE_RETURN を待つ。
 * 全完了したら親に `TSE_RETURN` + `stdArray<sPtr<stdEvent>>` を送る。
 * `CL_SET(n, x)` マクロで n 番目の戻り値の `msg_obj` を取り出す。
 *
 * @par 使用例 / Example
 * @code{.cpp}
 * cl = thNEW(tsCallList, (ifThis));
 * cl->call(thNEW(Worker1, (cl)));
 * cl->call(thNEW(Worker2, (cl)));
 * cl->finish();
 * // TSE_RETURN 受信後:
 * CL_SET(0, result1);
 * CL_SET(1, result2);
 * @endcode
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	tsCallList_(
		sPtr<tinyState> parent);

	/** @brief 待ち対象の tinyState を追加する。finish() より前に全て登録すること。
	 *  / Add a tinyState to wait for. Must be called before finish().
	 * @param caller 実行中の tinyState。/ The tinyState to wait for.
	 * @return 自身 (`tsCallList`) を返す (メソッドチェーン用)。/ Returns self for chaining.
	 */
	sPtr<tsCallList> call(sPtr<tinyState> caller);
	/** @brief 登録完了を宣言し、待機フェーズに移行する。/ Declare registration complete and start waiting.
	 * @details 呼び出し後、全登録 tinyState の TSE_RETURN を待つ。/ After this call, waits for TSE_RETURN from all registered tinyStates.
	 */
	void finish();
private:
protected:
	TS_DEFARGS
	sArray<sPtr<tinyState> > list;
	sPtr<stdArray<sPtr<stdEvent> > > ret;

	sPtr<stdQueue<stdEvent> > 	eq;
	unsigned	finish_flag:1;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;

/** @brief tsCallList の戻り値配列から n 番目の結果を取り出す。/ Extract the n-th result from tsCallList's return array.
 *  @param __n 0-based インデックス。/ 0-based result index.
 *  @param __x 代入先変数 (適切な型の sPtr)。/ Destination variable (sPtr of appropriate type).
 */
#define CL_SET(__n,__x)		\
	(__x) = (__x).d_cast(sPtr<stdArray<sPtr<stdEvent> > >::d_cast(ev->msg_obj)->ary[__n]->msg_obj);

TS_END_INTERFACE

#endif


tsCallList_::tsCallList_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/

sPtr<tsCallList>
tsCallList_::call(sPtr<tinyState> caller)
{
	list[list.length()] = caller;
	ret->length(list.length());
	return ifThis;
}

void
tsCallList_::finish()
{
	finish_flag = 1;
	wakeup();
}


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	ret = thNEW(stdArray<sPtr<stdEvent> >,(0));
	eq = thNEW(stdQueue<stdEvent>,());
	return rDO|INI_WAIT;
}
TS_STATE(INI_WAIT)
{
	if ( ev->type == TSE_RETURN )
		eq->ins(MAX_INTEGER64,ev);
	if ( finish_flag == 0 )
		return 0;
	for ( ; (ev=eq->del()).is_notNull() ; ) {
	int i;
		for ( i = 0 ;i < list.length() ; i ++ )
			if ( ev->source == list[i] ) {
				ret->ary[i] = ev;
				list[i] = thNULL;
				break;
			}
	}
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
int i;
	if ( ev->type != TSE_RETURN )
		return 0;
int cnt = 0;
	for ( i = 0 ; i < list.length() ; i ++ ) {
		if ( ev->source == list[i] ) {
			ret->ary[i] = ev;
			list[i] = thNULL;
		}
		if ( list[i] != thNULL )
			cnt ++;
	}
	if ( cnt )
		return 0;
	return rDO|FIN_START;
}
TS_STATE(FIN_START)
{
	parent->eventHandler(thNEW(stdEvent,
		(TSE_RETURN,ifThis,ret)));
	return rDO|FIN_TINYSTATE_START;
}
