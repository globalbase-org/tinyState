


#ifndef __sCallSection_cpp_H___
#define __sCallSection_cpp_H___

#include	"ts2/c++/sThreadKey.h"
#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/tinyState.h"

/**
 * @brief sCallSection 内部で使う tinyState スタックのノード (スタックに積む値型)。/ Stack node used internally by sCallSection (value type placed on the C++ stack).
 */
class sCallSectionNode : public sObject {
public:
	sCallSectionNode(sPtr<tinyState> ts) {
		this->ts = ts;
		next = 0;
	}
	sCallSectionNode * 		next;
	sPtr<tinyState>			ts;
};

/**
 * @brief 現在実行中の tinyState を示すスレッドローカルスタック。/ Thread-local stack tracking the currently executing tinyState.
 * @details
 * `ts2IO::read()` / `write()` などが sException を投げる際に、どの tinyState が
 * 呼び出し元かを知るために使う。`sCallSection::key->caller()` で取得できる。
 * `sCALL_SECTION(code)` マクロで push/pop を安全に管理する。
 * / Used by `ts2IO::read()`/`write()` (etc.) when throwing sException, so the yield target
 * tinyState can be retrieved via `sCallSection::key->caller()`.
 * Use the `sCALL_SECTION(code)` macro to safely manage push/pop.
 */
class sCallSection : public sObject {
public:
	sCallSection();
	~sCallSection();
	void push(sCallSectionNode * n);
	sPtr<tinyState> pop(sCallSectionNode * n);
	sPtr<tinyState> caller();

	static sThreadKey<sCallSection> key;
protected:
	sCallSectionNode *		list;
};


/** @brief 現在実行中 tinyState (`ifThis`) を sCallSection スタックに積んで `code` を実行する。
 *  sException がスローされても安全に pop される。
 *  / Push `ifThis` onto the sCallSection stack, execute `code`, then pop (exception-safe).
 */
#define sCALL_SECTION(__x)	\
	{						\
		sCallSectionNode n(ifThis);		\
		sCallSection::key->push(&n)		\
		try {					\
			for ( ; ; ) {			\
				__x			\
				break;			\
			}				\
		} catch ( sException& ex ) {		\
			sCallSection::key->pop(&n);	\
			throw ex;			\
		}					\
		sCallSection::key->pop(&n);		\
	}

#endif
