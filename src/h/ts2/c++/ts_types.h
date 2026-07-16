

#ifndef ___TS_TYPES_cpp_H___
#define ___TS_TYPES_cpp_H___

/* 旧 ts2/c/ts_types.h の内容を取り込み (2026-07-11)。C 系ツリー (ts2/c/) を削除する
 * にあたり、C++ 側が唯一依存していた基盤型・マクロをここへ統合した。
 * / Inlined from the now-removed ts2/c/ts_types.h — the only header the C++ tree
 * depended on from the legacy C tree. */

extern "C" {

#include	"std2/includes.h"

#define I64_FORMAT	"%lli"

#define INTEGER64	int64_t
#define U_INTEGER64	uint64_t
#define INTPTR		intptr_t
#define U_INTPTR	uintptr_t
#define INT64PTR	int64_t
#define NULPTR		((void*)0)

#define MAX_INTEGER	0x7fffffff
#define MAX_INTEGER64	0x7fffffffffffffffLL

typedef INTPTR (*TS_FUNC)();


typedef struct ts_io_result {
	int			err;
	int			last_ret;
	int			len;
} TS_IO_RESULT;

}


#endif
