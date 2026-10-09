

#ifndef ___sThreadStack_cpp_H___
#define ___sThreadStack_cpp_H___

#include	"ts2/c++/ts_types.h"

/**
 * @brief 呼び出したスレッド自身のスタック長を OS に聞いてバイト数で返す。
 *        / Ask the OS for the calling thread's own stack size, in bytes.
 * @details
 * `eventHandler()` の入れ子をどこまで許すか (`tsApplication::evh_depth` の既定値) を
 * 決めるために使う。段数ではなく「この スレッドに実際どれだけ積めるか」を聞くので、
 * スタックの小さい機では閾値が勝手に下がり、沈黙死 (スタックオーバーフローによる
 * メッセージ無しの SIGSEGV) ではなく「浅めで gc に逃がす」に落ちる。
 *
 * 聞き方は OS ごとに違い、arch overlay の層で差し替わる:
 *
 * | 層 | API |
 * |---|---|
 * | `posix` (既定・Linux/glibc) | `pthread_getattr_np` + `pthread_attr_getstacksize` |
 * | `posix_Darwin`             | `pthread_get_stacksize_np` |
 * | `posix_MinGW`              | `GetCurrentThreadStackLimits` |
 * | `posix_Cygwin`             | `GetCurrentThreadStackLimits` (★ `pthread_getattr_np` が無い) |
 *
 * ★ **使ってはいけないもの** (4 機で実測して外した):
 * TEB の `StackBase - StackLimit` は**コミット済みの領域しか返さない** (Cygwin 実機で
 * 実際 2048KB のスレッドを main 24KB / worker 64KB と答えた)。
 * `getrlimit(RLIMIT_STACK)` は **worker の実値を反映しない** (8MB で作った worker でも
 * 2032KB のまま)。どちらも「成功を返す無効な答え」なので黙って通る。
 *
 * @return スタック長 (バイト)。**0 = 聞けなかった**。呼び手は 0 を受けたら
 *         自分の組み込み既定へ落とすこと (0 を長さとして使ってはならない)。
 */
INTEGER64	ts2_thread_stack_size();

/**
 * @brief フレームワークが作る スレッド (worker / gc) に与えるスタック長 (バイト)。
 *        / Stack size the framework gives the threads it creates (worker / gc), in bytes.
 * @details
 * 既定は 8 MB で、4 機すべて同じ値を使う (**規則は同じ・設定の仕方だけが機ごとに違う**)。
 * `ts2_thread_stack_size()` が示す「既定の worker スタック」は機ごとにばらついていて、
 * macOS だけ 519KB しかない (main の 1/16)。そこに合わせると mac の worker だけ
 * 入れ子を 129 段しか許せないので、作る側で揃えておく。<br>
 * <br>
 * 8 MB は**予約**であって確保ではない (Linux はページを触った分だけコミット、
 * Windows も reserve はアドレス空間のみ)。64bit 前提。<br>
 * <br>
 * ★ 与えたことの確認は `pthread_attr_setstacksize()` の**戻り値ではなく**、
 * そのスレッド自身が `ts2_thread_stack_size()` で答える値で行う (戻り値は
 * 「成功を返す無効な操作」を見逃す)。<br>
 * <br>
 * **main スレッドは別の話**: POSIX では exec 時の `RLIMIT_STACK` でカーネルが決め、
 * リンカ指定 (`-z stacksize`) は無視される。Windows だけは PE の
 * `SizeOfStackReserve` をリンク時に決められるので `-Wl,--stack,0x800000` を使う
 * (`cmake/tinyStateConfig.cmake.in` が利用側へ伝播させる)。どちらにしても
 * 足りなければ閾値が勝手に下がるだけで、沈黙死にはならない。
 *
 * @return 与えるスタック長 (バイト)。
 */
INTEGER64	ts2_worker_stack_size();

#endif
