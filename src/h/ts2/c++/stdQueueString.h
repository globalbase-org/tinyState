

#ifndef ___stdQueueString_cpp_H___
#define ___stdQueueString_cpp_H___

#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/stdArray.h"

/**
 * @brief ソート済み文字列キューに集合演算とフィルタを追加したクラス。/ Sorted string queue with set operations and glob-style filtering.
 * @details
 * `stdQueue<stdString>` を継承し、以下の操作を追加する:
 * - `filter(flt)`: `^prefix`, `suffix$`, `^exact$`, `partial` パターンでフィルタ
 * - `add(a2)`: 和集合 (ソート済みマージ)
 * - `sub(a2)`: 差集合
 * / Extends `stdQueue<stdString>` with glob-style `filter()`, sorted-merge `add()`, and set-difference `sub()`.
 */
class stdQueueString : public stdQueue<stdString> {
public:
	stdQueueString() {
	}
	stdQueueString(sPtr<stdQueue<stdString> >  str);
	~stdQueueString() {
	}
	sPtr<stdQueueString>  
		filter(sPtr<stdArray<sPtr<stdString> > > flt);

	static int cmp(sPtr<stdString>  s1,sPtr<stdString>  s2) {
		if ( s2 == thNULL ) {
			::printf("%s",s1->get_str());
			return 0;
		}
		return STR_NULL(s1)->cmp(s2);
	}
	static int cmp_del(sPtr<stdString>  s1,sPtr<stdString>  s2) {
		if ( STR_NULL(s1)->cmp(s2) == 0 )
			return 1;
		return 0;
	}

	sPtr<stdQueueString>  add(
		sPtr<stdQueueString> );
	sPtr<stdQueueString>  sub(
		sPtr<stdQueueString> );
	sPtr<stdQueueString>  add(
		sPtr<stdQueue<stdString> > );
	sPtr<stdQueueString>  sub(
		sPtr<stdQueue<stdString> > );
};

#endif


