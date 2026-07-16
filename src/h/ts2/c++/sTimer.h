

#ifndef ___sTimer_cpp_H___
#define ___sTimer_cpp_H___

#include	"ts2/c++/stdInterval.h"

class tinyState;
/**
 * @brief tinyState 向けタイマーヘルパー (値型)。/ Timer helper for tinyState (value type, not reference-counted).
 * @details
 * `sPtr` で包まずにメンバ変数として直接持つタイマー。
 * `start(me, length)` でタイマーをセットし、次回起動時に `is_expire(me)` で満了を確認する。
 * 内部で `stdInterval::wait()` を呼んで tinyState をスリープさせる。
 *
 * @par 使用例 / Example
 * @code{.cpp}
 * TS_PRIVATE(sTimer timer;)
 * TS_STATE(ACT_WAIT) {
 *     timer.start(ifThis, 1000000); // 1 秒
 *     return ACT_TIMER;
 * }
 * TS_STATE(ACT_TIMER) {
 *     if (!timer.is_expire(ifThis)) return 0;
 *     // タイムアウト処理
 *     return rDO | ACT_DONE;
 * }
 * @endcode
 */
class sTimer {
public:
	unsigned debug:1;
	sTimer() {
		setup_time = -1;
		eventType = TSE_TIMER;
	}
	~sTimer() {
	}

	/** @brief タイマーをセットする (イベントタイプ指定)。/ Start timer with a specific event type. */
	void start(sPtr<tinyState>  me,INTEGER64 length,int eventType) {
	INTEGER64 now = stdInterval::now();
		if ( length < 0 )
			length = 0;
		stop_flag = 0;
		expire = now + length;
		this->eventType = eventType;
if ( debug )
::printf("sTimer -- ex=%lli st=%lli\n",expire,setup_time);
		if ( setup_time <= now || expire < setup_time ) {
			setup_time = expire;
if ( debug )
::printf("sTimer -- wait %s %lli\n",me->getClass(),length);
			stdInterval::wait(me,length,eventType);
		}
	}
	/** @brief タイマーをセットする (デフォルトイベントタイプ)。length はマイクロ秒。/ Start timer; length in microseconds. */
	void start(sPtr<tinyState>  me,INTEGER64 length) {
		start(me,length,eventType);
	}
	/** @brief タイマーをキャンセルする。/ Cancel the timer. */
	void stop(sPtr<tinyState>  me = thNULL) {
	     	stop_flag = 1;
		if ( me == thNULL )
		  return;
		me->application->fw()->detach(me,FWTR_INTERVAL);
	}
	/** @brief タイマーが満了していれば 1 を返す。まだなら 0 を返し必要に応じて再登録する。/ Returns 1 if expired, 0 if not (re-registers timer if needed). */
	int is_expire(sPtr<tinyState>  me) {
	INTEGER64 now = stdInterval::now();
if ( debug )
  ::printf("sTimer -- is_expire %i ex=%lli n=%lli\n",stop_flag,expire,now);
		if ( stop_flag )
		     	return 0;
		if ( expire <= now )
			return 1;
		if ( setup_time <= now ) {
if ( debug )
::printf("sTimer-rewrite\n");
			stdInterval::wait(me,expire-setup_time,eventType);
			setup_time = expire;
		}
if ( debug )
  ::printf("sTimer-ex -- is_expire %i ex=%lli st=%lli\n",stop_flag,expire,setup_time);
		return 0;
	}

	void debugPrint() {
	  ::printf("sTimer -- %lli %lli\n",expire,setup_time);
	}
private:
	INTEGER64	expire;
	INTEGER64	setup_time;
	int		eventType;
	unsigned	stop_flag:1;
};

#endif
