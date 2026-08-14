/*
 * 共有ライブラリ側。sCallSection::key が指す実体のアドレスを返す。
 * 実体化はこの翻訳単位でも起きるので、修正が無ければ exe 側と別物になる。
 */
#include "ts2/c++/sCallSection.h"

extern "C" void * mit_lib_key_instance(void)
{
	return (void *)(sCallSection::key.operator->());
}
