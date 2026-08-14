

#ifndef __sThreadKey_cpp_H___
#define __sThreadKey_cpp_H___


#include	"ts2/c++/sObject.h"
#include	<pthread.h>

/**
 * @brief pthread スレッドローカルストレージのラッパー。/ pthread thread-local storage wrapper.
 * @details
 * `operator->()` でスレッドごとに独立した `__TYPE` インスタンスへアクセスする。
 * スレッドが終了すると自動的に `delete` される。
 * `sCallSection::key` (現在実行中 tinyState の追跡) 等の実装に使われる。
 * / Access per-thread `__TYPE` instances via `operator->()`. Automatically deleted on thread exit.
 * Used to implement `sCallSection::key` (tracking the current tinyState).
 * @tparam __TYPE スレッドローカルに保持する型。/ Per-thread type.
 */
template<class __TYPE>
class sThreadKey : public sObject {
public:
	sThreadKey() {}
	~sThreadKey() {}

	/* Per-thread instance via C++11 thread_local (native TLS) instead of
	   pthread_key_create/getspecific/setspecific.  On Windows, winpthreads'
	   pthread_key emulation returned a *different* (fresh, empty) value for the
	   SAME thread mid-execution under heavy worker-pool churn
	   (pthread_getspecific -> 0), which corrupted sCallSection's yield/resume
	   stack -> "ENTER_CALL is required" panic + hangs on real hardware (never on
	   Linux/glibc).  Native thread_local storage is stable for the thread's
	   lifetime; the holder's destructor frees the object at thread exit.  Works
	   identically on POSIX and MinGW.  Windows-port design memo */
	__TYPE * operator -> () const {
		struct holder {
			__TYPE * p;
			holder() : p(0) {}
			~holder() { if ( p ) delete p; }
		};
		thread_local holder h;
		if ( !h.p )
			h.p = new(__FILE__,__LINE__) __TYPE();
		return h.p;
	}
};

class sCallSection;

/* sThreadKey<sCallSection> だけは、上の inline 定義を *使わない*。
 *
 * 関数ローカルの thread_local は、その関数を実体化したイメージごとに 1 つずつ
 * できる。ELF ではこの手のシンボルが STB_GNU_UNIQUE になり、動的リンカが
 * プロセス内で 1 実体へ統一するので問題にならない (nm -D で `u` と出る)。
 * PE にはこれに相当する仕組みが無く、exe と DLL がそれぞれ自分のスロットを
 * 持つ。sCallSection は「現在実行中の tinyState」を保持しているので、片方が
 * 積んだものをもう片方が読むと空に見え、caller() が null → 親が辿れず
 * application が null → appMtxLock で落ちる。
 *
 * 宣言だけをここに置き、定義は sCallSection.cpp (ライブラリ内の 1 つの TU) に
 * 置く。こうするとどのイメージも自前の定義を作れず、共有ライブラリから import
 * するしかなくなるので、実行体をどう分割しても 1 実体になる。
 *
 * これが効くのは tinyState を *共有ライブラリとして* 使う場合。静的ライブラリを
 * 複数のイメージへリンクすれば当然それぞれが自分の複製を持つが、その構成は PE の
 * リンカが multiple definition で弾く。docs/GOTCHAS.md §13。
 */
template<> sCallSection * sThreadKey<sCallSection>::operator -> () const;

#endif
