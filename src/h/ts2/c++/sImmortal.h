#ifndef ___sImmortal_cpp_H___
#define ___sImmortal_cpp_H___

#include	<new>
#include	<stddef.h>

/**
 * @brief プロセス終了時に**破棄されない**静的オブジェクトの入れ物。
 *        / Holder for a static object that is never destroyed.
 * @details
 * 静的記憶域のオブジェクトは、構築が終わった時点でデストラクタが `__cxa_atexit`
 * (MinGW では `atexit`) に登録され、プロセス終了 — Windows では DLL detach — で走る。
 * 登録の逆順に呼ばれるだけで、**翻訳単位をまたぐ順序は未規定**。tinyState は
 * 静的オブジェクト同士に依存があるため (代表例: `tsSignalCore_::_signal_list` の
 * デストラクタが `stdObject::relref()` を呼び、それが `stdObject::refMtx[]` を取る)、
 * 破棄順が外れると破棄済みのオブジェクトを触ることになる。実際に起きた形:
 *
 *   `~sThreadMutex()` が走った後の vptr は基底 `sObject` の vtable を指す。
 *   `sThreadMutexHandle` の ctor は virtual な `lock()` を呼ぶので、`sThreadMutex`
 *   の添字 (vptr+16) で読みにいき、4 エントリしかない sObject の vtable を 1 ワード
 *   越えて隣の vtable の先頭 (offset-to-top = 0) を掴み、**0 番地へ飛ぶ**。
 *
 * `abort()` 経路では猶予が無い。MSVCRT の `abort()` は `_exit()` -> `RtlExitUserProcess`
 * -> `LdrShutdownProcess` -> DLL detach と進み、**DLL の静的デストラクタはローダの
 * detach 通知で駆動される**ので、`exit` を避けても素通りできない。テーブルの順序を
 * 実行時に変える API も無い。⇒ **残っている手は「デストラクタを登録させない」だけ。**
 *
 * 使い方はクラス/名前空間スコープの静的として置くこと:
 *
 * @code{.cpp}
 * // 宣言 (ヘッダ)
 * static sImmortal<sThreadCond>                     refCond;
 * static sImmortalArray<sThreadMutex,101>           refMtx;
 * // 定義 (cpp)
 * sImmortal<sThreadCond>                 stdObject::refCond;
 * sImmortalArray<sThreadMutex,101>       stdObject::refMtx;
 * // 使用
 * refCond->broadcast();      // sImmortal は operator-> / operator*
 * refMtx[id]                 // sImmortalArray は operator[] (呼び出し側は素の配列と同じ)
 * @endcode
 *
 * @warning **デストラクタを宣言しないこと。**暗黙のデストラクタが trivial である限り
 *          atexit に登録されない。基底クラスも持たせてはならない — `sObject` を継承すると
 *          `virtual ~sObject()` で非 trivial になり、登録されてしまう。
 * @warning 自動変数として使わないこと。静的記憶域の `store` がゼロ初期化されることを
 *          前提にしている (`sObject::operator new` の memset に相当する)。
 * @note    `pthread_mutex_destroy` / `pthread_cond_destroy` は呼ばれなくなる。
 *          プロセス生存中に一度だけ作られるものなので、OS が回収する。
 * @note    この不変条件 (tinyState2 が静的デストラクタを 1 つも登録しない) は
 *          ctest の `tinyState_no_static_dtor` が `nm` で検査している。
 */
template<class __TYPE>
class sImmortal {
public:
	sImmortal()				{ ::new ((void*)store) __TYPE(); }
	template<class __ARG>
	explicit sImmortal(__ARG arg)		{ ::new ((void*)store) __TYPE(arg); }

	sImmortal(const sImmortal &) = delete;
	void operator =(const sImmortal &) = delete;

	/* ★ デストラクタは宣言しない (trivial なので atexit に載らない)。 */

	__TYPE & operator * ()			{ return *ptr(); }
	__TYPE * operator ->()			{ return ptr(); }
	__TYPE * get()				{ return ptr(); }
private:
	__TYPE * ptr()				{ return (__TYPE*)(void*)store; }
	alignas(__TYPE) unsigned char	store[sizeof(__TYPE)];
};

/**
 * @brief 配列版の @ref sImmortal。呼び出し側は素の配列と同じ `x[i]` で書ける。
 *        / Array flavour; call sites keep the plain `x[i]` form.
 */
template<class __TYPE,int __NUM>
class sImmortalArray {
public:
	sImmortalArray() {
		for ( int i = 0 ; i < __NUM ; i ++ )
			::new ((void*)slot(i)) __TYPE();
	}

	sImmortalArray(const sImmortalArray &) = delete;
	void operator =(const sImmortalArray &) = delete;

	/* ★ デストラクタは宣言しない。 */

	__TYPE & operator [](int i)		{ return *(__TYPE*)(void*)slot(i); }
	int size() const			{ return __NUM; }
private:
	unsigned char * slot(int i)		{ return store + (size_t)i * sizeof(__TYPE); }
	alignas(__TYPE) unsigned char	store[(size_t)__NUM * sizeof(__TYPE)];
};

#endif
