

/* ★★ このブロックは **どの #include より前**でなければならない。
 *
 * GetCurrentThreadStackLimits は Windows 8 以降の API で、mingw-w64 / Cygwin の
 * processthreadsapi.h が **_WIN32_WINNT >= 0x0602 で囲っている**。既定値は環境依存
 * (Debian の mingw-w64 は 0xa00・MSYS2 はこれより低い) なので明示して差を消す。
 * 既に高い値が立っているときは下げない。tsProbe.cpp と同じ作法。
 *
 * ★ **位置が効く理由**: `ts2/c++/sThreadStack.h` -> `ts_types.h` ->
 * `std2/includes.h` (MinGW 層) が **winsock2.h と windows.h を引く**。そこで
 * _WIN32_WINNT が確定してしまうと、後から #define しても **include ガードで
 * windows.h は二度処理されない** ので宣言は現れない。
 * ⇒ 2026-10-04 に box の MSYS2 (gcc 16.1.0) で実際に落ちた。
 *    Debian のクロスでは既定が 0xa00 なので**偶然通っていた**。
 */
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0602
#elif _WIN32_WINNT < 0x0602
#undef  _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif

#include	"ts2/c++/sThreadStack.h"

#include	<windows.h>

/* MinGW。posix_Cygwin の同名ファイルと中身は同じだが、共通側に #ifdef を置かない
 * 規約なので層ごとに持つ (Cygwin は _WIN32 を定義しないため、仮に共通化すると
 * Cygwin が posix の pthread_getattr_np 版に落ちて壊れる)。
 *
 * ★ TEB の StackBase - StackLimit を見てはいけない。あれはコミット済み領域しか
 * 返さず、2048KB のスレッドを 24KB と答える。 */
INTEGER64
ts2_thread_stack_size()
{
ULONG_PTR	lo = 0,  hi = 0;
	GetCurrentThreadStackLimits(&lo,&hi);
	if ( hi <= lo )
		return 0;
	return (INTEGER64)(hi - lo);
}

/* フレームワークが作る スレッドに与えるスタック長。4 機とも同じ 8 MB。
 * 層ごとに持たせてあるのは、この機だけ変えたくなったときに**ここだけ**
 * 書き換えれば済むようにするため (共通側に #ifdef を置かない規約)。 */
INTEGER64
ts2_worker_stack_size()
{
	return (INTEGER64)8 * 1024 * 1024;
}
