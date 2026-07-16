

#ifndef ___stdInterval_cpp_H___
#define ___stdInterval_cpp_H___

#include	"ts2/c++/tinyState.h"
#include	"ts2/c++/sThreadMutex.h"

/**
 * @brief 時刻取得とタイマー待ち機能を提供するユーティリティクラス。/ Utility providing current time and timer-wait for tinyState.
 * @details
 * 全メソッドが `static`。tinyState から時刻取得やタイマー待ちをするときに使う。
 * `now()` はマイクロ秒精度の単調増加タイムスタンプを返す。
 * `wait()` は `stdFrameWork` 経由でタイマーイベントを登録し、me (tinyState) を時間後に起こす。
 * `sTimer` はこのクラスを内部で使う。
 * / All-static utility. `now()` returns a monotonically increasing microsecond timestamp.
 * `wait()` registers a timer event via `stdFrameWork` that wakes `me` after `tm` microseconds.
 * Used internally by `sTimer`.
 */
class stdInterval {
public:
	/** @brief `tm` マイクロ秒後に me を起こすタイマーを登録する。/ Register a timer to wake me after tm microseconds. */
	static int wait(sPtr<tinyState>  obj,INTEGER64 tm,int type);
	/** @brief me に登録済みのタイマーを解除する。/ Cancel any timer registered for me. */
	static int detach(sPtr<tinyState>  obj);
	/** @brief stdFrameWork 経由でフォーマット出力する。/ Formatted output via stdFrameWork. */
	static int printf(sPtr<tinyState> ,const char * fmt,...);
	static int v_printf(sPtr<tinyState> ,const char * fmt,va_list arg);
	static int printf_err(sPtr<tinyState> ,const char * fmt,...);
	static int v_printf_err(sPtr<tinyState> ,const char * fmt,va_list arg);
	/** @brief 現在時刻 (マイクロ秒、単調増加)。/ Current time in microseconds (monotonically increasing). */
	static INTEGER64 now();

 protected:
	static sThreadMutex	m;
	static INTEGER64	lastAccessTime;
};


#endif

