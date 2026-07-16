

#ifndef ___sArray_cpp_H___
#define ___sArray_cpp_H___

#if __cplusplus >= 201103L
#else
#include	<typeinfo>
#endif
#include	"ts2/c++/stdObject.h"

/**
 * @brief 動的サイズの配列 (非参照カウント)。/ Dynamically-sized array without reference counting.
 * @details
 * `stdObject` を継承しないローカル用配列。`stdArray<T>` の内部ストレージとして使われる。
 * `expand` フラグが 1 のとき、範囲外アクセスで自動的にリサイズする。
 * `push()`/`pop()`/`unshift()`/`shift()` でスタック・キュー的に使える。
 *
 * `length(int)` は幾何成長 (capacity を 2 倍ずつ拡大) で O(1) 償却 push を実現する。
 * 縮小時は余剰要素をデフォルト値にリセットして sPtr の参照を確実に解放する。
 * コピーコンストラクタ・`operator=` は `= delete` (浅いコピーによる dangling 防止)。
 * / Stack-allocated dynamic array (no refcount). Used as storage inside `stdArray<T>`.
 * Geometric growth (2x capacity) gives O(1) amortized `push()`.
 * Excess elements are reset on shrink to release sPtr references.
 * Copy constructor and `operator=` are deleted to prevent dangling pointers.
 * @tparam __TYPE 要素の型。/ Element type.
 */
template<class __TYPE>
class sArray: public sObject {
private:
	int leng;
	int cap;	///< 確保済みキャパシティ (>= leng)。/ Allocated capacity (>= leng).
	__TYPE*		ary;

	const char * __file;
	int __line;

	void _realloc(int new_cap) {
	int i,clen;
		__TYPE * tmp = ary;
		ary = new_cap ? new (__file,__line) __TYPE[new_cap]() : 0;
		clen = leng < new_cap ? leng : new_cap;
		for ( i = 0 ; i < clen ; i ++ )
			ary[i] = tmp[i];
		delete[] tmp;
		cap = new_cap;
	}
public:
	unsigned expand:1;

	sArray(int len): leng(len), cap(len),
					ary(new (__FILE__,__LINE__)  __TYPE[len]()),
					expand(1), __file(__FILE__), __line(__LINE__) {
	}
	sArray(): leng(0), cap(0), ary(0), expand(1),
			__file(__FILE__), __line(__LINE__) {
	}

	/** @brief コピーコンストラクタ禁止 (浅いコピーによる dangling を防ぐ)。/ Deleted to prevent dangling from shallow copy. */
	sArray(const sArray&) = delete;
	/** @brief コピー代入禁止。/ Copy assignment deleted. */
	sArray& operator=(const sArray&) = delete;

	~sArray() {
		if ( this->ary == 0 )
			return;
		expand = 0;
		leng = 0;
		cap = 0;
		delete[] this->ary;
	}
	__TYPE& operator[](int index) {
		if ( index < 0 )
			stdObject::panic("ARRAY OVER RUN!!");
		if ( expand == 0 ) {
			if ( index >= leng )
				stdObject::panic("ARRAY OVER RUN EXPAND!!");
			return ary[index];
		}
		if ( index >= leng )
			length(index+1);
		return ary[index];
	}
	__TYPE& operator[](int index) const {
		if ( index < 0 )
			stdObject::panic("ARRAY OVER RUN!!");
		if ( index >= leng )
			stdObject::panic("ARRAY OVER RUN EXPAND!!");
		return ary[index];
	}

	void refine_sobj(const char * file,int line)
	{
		__file = file;
		__line = line;
		if ( ary == 0 )
			return;
		sObject::refine_sobj((void*)ary,file,line);
	}

	int length() {
	  	return leng;
	}
	void length(int len) {
		if ( len < 0 ) {
			// shrink-to-fit: cap を leng に合わせて再確保
			if ( leng == cap || ary == 0 )
				return;
			_realloc(leng);
			return;
		}
		if ( leng == len )
			return;
		if ( len > cap ) {
			// 幾何成長: cap が 0 なら 8 から開始、以降 2 倍ずつ
			int new_cap = cap == 0 ? 8 : cap * 2;
			if ( new_cap < len ) new_cap = len;
			_realloc(new_cap);
		} else if ( len < leng ) {
			// 縮小: 余剰要素をデフォルト値にリセット (sPtr 等の参照を解放)
			for ( int i = len ; i < leng ; i ++ )
				ary[i] = __TYPE();
		}
		leng = len;
	}
	void push(__TYPE inp) {
	int len = leng;
		length(leng+1);
		ary[len] = inp;
	}

	__TYPE pop() {
	__TYPE ret;
		if ( leng == 0 )
			return ret;
		ret = ary[leng-1];	// fix: 元コードは ary[leng] で範囲外だった
		length(leng-1);
		return ret;
	}

	void unshift(__TYPE inp) {
		length(leng+1);
	int i;
		for ( i = leng-1 ; i > 0 ; i -- )
			ary[i] = ary[i-1];
		ary[0] = inp;
	}

	__TYPE shift() {
	__TYPE ret;
	int i;
		if ( leng == 0 )
			return ret;
		ret = ary[0];
		for ( i = 0 ; i < leng-1 ; i ++ )
			ary[i] = ary[i+1];
		length(leng-1);
		return ret;
	}
};




#endif
