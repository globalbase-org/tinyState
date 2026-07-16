

#ifndef ___stdArray_cpp_H___
#define ___stdArray_cpp_H___

#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sArray.h"
#include	"ts2/c++/sPtr.h"

/**
 * @brief `sArray<T>` を `stdObject` (参照カウント付き) で包むラッパー。/ Reference-counted wrapper around `sArray<T>`.
 * @details
 * `sPtr<stdArray<T>>` として tinyState フレームワーク内で動的配列を扱うときに使う。
 * `push()`/`pop()`/`unshift()`/`shift()` および `operator[]` を提供する。
 * `thNEW(stdArray<int>, (10))` のように生成する。
 * / Use `sPtr<stdArray<T>>` to hold a dynamic array within the tinyState framework.
 * @tparam __TYPE 要素の型。/ Element type.
 */
class stdArrayBase : public stdObject {
public:
	stdArrayBase() {};
	~stdArrayBase() {};

	virtual void length(int len) {
	}
	virtual int length() {
		return 0;
	}
	virtual void expand(int ex) {
	}
	virtual int expand() {
		return 0;
	}
};

/**
 * @brief `stdArrayBase` の具体的な型付き実装。/ Concrete typed implementation of `stdArrayBase`.
 * @tparam __TYPE 要素の型。/ Element type.
 */
template<class __TYPE>
class stdArray : public stdArrayBase {
public:
	sArray<__TYPE>	ary;
	__TYPE *		ptr;
	stdArray(int len) :
		ary(len),
		stdArrayBase()
	{
	}
	stdArray(sPtr<stdArray> a) :
		ary(a->ary.length()),
		stdArrayBase()
	{
		int len = a->ary.length();
		for ( int i = 0 ; i < len ; i++ )
			ary[i] = a->ary[i];
		ptr = len ? &ary[0] : 0;
	}

	~stdArray() {
	}
	__TYPE& operator[](int index) {
		return ary[index];
	}

	virtual void length(int len) {
		ary.length(len);
	}
	virtual int length() {
		return ary.length();
	}
	virtual void expand(int ex) {
		ary.expand = ex;
	}
	virtual int expand() {
		return ary.expand;
	}

	void push(__TYPE inp) {
		ary.push(inp);
	}
	__TYPE pop() {
		return ary.pop();
	}

	void unshift(__TYPE inp) {
		ary.unshift(inp);
	}
	__TYPE shift() {
		return ary.shift();
	}
};


#endif

