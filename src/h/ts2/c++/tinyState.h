

#ifndef ___tinyState_cpp_H___
#define ___tinyState_cpp_H___

#include	"ts2/c++/ts_types.h"
#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/stdThreadInfo.h"

class stdEvent;
class stdEventHandle;
class tinyState;
class tinyState_;
class tsApplication;
class stdThreadInfo;
class sThreadMutexRecursive;

/* tinyState 専用の always_inline マクロ。Windows SDK (winnt.h) が
 * FORCEINLINE (__forceinline) を定義し衝突・再定義警告になるため、専用名を使う。 */
#define TS_FORCEINLINE __attribute__((always_inline))

typedef INTPTR TS_STATE_TYPE;
typedef int (*TS_HANDLER_FUNC)(sPtr<tinyState> ,sPtr<stdEvent> );
typedef sPtr<stdEvent> (*TS_FILTER_FUNC)(sPtr<tinyState> ,sPtr<stdEvent> );
typedef TS_STATE_TYPE (*TS_STATE_FUNC)(sPtr<tinyState_>,sPtr<stdEvent> );
typedef struct ts_trans {
	const char *		name;
	TS_STATE_FUNC		func;
} TS_TRANS;
typedef struct ts_refer {
	const char *		cls;
	int			count;
} TS_REFER;

#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/stdEvent.h"
#include	"ts2/c++/stdEventHandle.h"
#include	"ts2/c++/stdString.h"

#include	"_ts2/c++/tinyState_pb.h"
#include	"ts2/c++/tsApplication.h"

#define TS_DEFAULT_PRIORITY	10000

#define C_INI		001
#define C_ACT		002
#define C_FIN		004
#define C_ZOM		010
#define C_ERR		020
#define C_PUB		040
#define C_THR		0100

#define CC_INI		"\001"
#define CC_ACT		"\002"
#define CC_FIN		"\004"
#define CC_ZOM		"\010"
#define CC_ERR		"\020"
#define CC_PUB		"\040"

#define CC_THR_INI		"\101"
#define CC_THR_ACT		"\102"
#define CC_THR_FIN		"\104"
#define CC_THR_ZOM		"\110"
#define CC_THR_ERR		"\120"
#define CC_THR_PUB		"\140"

#define C_TEST(x,y)	((x) == 0 ? C_INI&(y) : (((TS_TRANS*)((x)&TS_STATE_BIT))->name[0]&(y)))
#define C_NAME(x)	((x) == 0 ? "INI" : &(((TS_TRANS*)((x)&TS_STATE_BIT))->name[1]))
#define C_STR(x,y)	( memcmp((x),(y),strlen(y)) == 0 )

#define rDO		((TS_STATE_TYPE)0x1)
#define TS_STATE_BIT	(~rDO)

#define TS_STATE(x)	\
TS_STATE_TYPE		\
TS_THISCLASS::state_##x(sPtr<tinyState_> _this,sPtr<stdEvent> ev)	\
{								\
sPtr<TS_THISCLASS> __this = sPtr<TS_THISCLASS>::d_cast(_this);	\
	return __this->_state_##x(ev);				\
}								\
inline TS_STATE_TYPE TS_FORCEINLINE				\
TS_THISCLASS::_state_##x(sPtr<stdEvent> ev)

#define TS_THREAD(x)	\
TS_STATE_TYPE		\
TS_THISCLASS::state_##x(sPtr<tinyState_> _this,sPtr<stdEvent> ev)	\
{								\
sPtr<TS_THISCLASS> __this = sPtr<TS_THISCLASS>::d_cast(_this);	\
	return __this->_state_##x(ev);				\
}								\
inline TS_STATE_TYPE TS_FORCEINLINE				\
TS_THISCLASS::_state_##x(sPtr<stdEvent> ev)

/**
 * @def TS_PRIVATE(x)
 * @brief Declare a private member variable near its point of use. / 実装クラスのプライベートメンバ変数をソース内に分散宣言する。
 * @details
 * Place this macro outside the class declaration block (`#if 0` ... `TS_END_IMPLEMENT`),
 * typically just before the state function (`TS_STATE` / `TS_THREAD`) that uses the variable.
 * `tscpp2` scans the source file, collects all `TS_PRIVATE(...)` declarations, and emits
 * them into the private section of the generated `_ts2/c++/foo_.h`.
 * At C++ compile time the macro expands to nothing, so it never causes a compile error.
 *
 * @code{.cpp}
 * TS_PRIVATE(int counter;)
 * TS_STATE(ACT_START) {
 *     counter = 0;
 *     ...
 * }
 * @endcode
 *
 * @note Declaring the same variable in two places causes a duplicate-member compile error.
 *
 * ---
 *
 * クラス宣言ブロック (`#if 0` ... `TS_END_IMPLEMENT`) の外、
 * 通常は使用する状態関数 (`TS_STATE` / `TS_THREAD`) の直前に置く。
 * `tscpp2` がソースファイルを走査して `TS_PRIVATE(...)` を拾い上げ、
 * 生成する `_ts2/c++/foo_.h` の private セクションへメンバ変数として出力する。
 * C++ コンパイル時にはこのマクロは空に展開されるため、コンパイルエラーにはならない。
 *
 * @code{.cpp}
 * TS_PRIVATE(int counter;)
 * TS_STATE(ACT_START) {
 *     counter = 0;
 *     ...
 * }
 * @endcode
 *
 * @note 同じ宣言を 2 箇所以上に書くと重複メンバでコンパイルエラーになる。
 * @see TS_PROTECTED
 */
#define TS_PRIVATE(x)
/**
 * @def TS_PROTECTED(x)
 * @brief Protected-section variant of TS_PRIVATE. /
 *        TS_PRIVATE の protected 版。生成クラスの protected セクションに出力される。
 * @see TS_PRIVATE
 */
#define TS_PROTECTED(x)
#define TS_REF(x)
#define IMP_PRIVATE

#define BODY_EVENT_FILTER(__class,__func)	\
sPtr<stdEvent> 					\
__class##_::__func(sPtr<tinyState>  THIS,sPtr<stdEvent>  ev)	\
{						\
	return sPtr<__class>::d_cast(THIS)	\
			->__func(ev);		\
}						\
sPtr<stdEvent> 					\
__class##_::__func(sPtr<stdEvent>  ev)

#define	R_TEST	if ( ev->type != TSE_RETURN ) return 0;
#define R_REF(x)	\
	R_TEST		\
	(x) = (x).d_cast(ev->msg_obj);
#define R_SET(x)	\
	R_TEST		\
	(x) = (x).d_cast(ev->msg_obj);
#define S_SET(x)	\
	R_TEST		\
	(x) = (x).d_cast(sPtr<stdObject>(ev->msg_obj));

#endif
