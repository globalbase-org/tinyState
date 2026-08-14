/*
 * exe 側。自分から見た sCallSection::key の実体と、共有ライブラリから見た実体を
 * 比べる。同じスレッドから呼ぶので、TLS が 1 実体なら必ず一致する。
 *
 * ELF では元から一致する (STB_GNU_UNIQUE)。PE には相当機構が無いため、
 * sThreadKey<sCallSection>::operator->() がヘッダの inline のままだと exe と DLL が
 * 別スロットを持ち、ここが不一致になる。docs/GOTCHAS.md §13。
 */
#include "ts2/c++/sCallSection.h"
#include <stdio.h>

extern "C" void * mit_lib_key_instance(void);

int main(void)
{
void * from_exe = (void *)(sCallSection::key.operator->());
void * from_lib = mit_lib_key_instance();

	::printf("[multi-image] exe=%p lib=%p\n",from_exe,from_lib);
	if ( from_exe != from_lib ) {
		::printf("[multi-image] FAILED: sCallSection::key is duplicated"
				" across images\n");
		return 1;
	}
	::printf("[multi-image] OK: one instance across images\n");
	return 0;
}
