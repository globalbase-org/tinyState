

#ifndef ___stdString_cpp_H___
#define ___stdString_cpp_H___

#include	"ts2/c++/stdArray.h"
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/stdRx.h"


class stdString;
class rxWork : public stdObject {
public:
	sPtr<stdArray<sPtr<stdString> > >values;
	sPtr<stdArray<regmatch_t> > 	match;

	rxWork() {
	}
	~rxWork() {
		values = thNULL;
		match = thNULL;
	};
};

/**
 * @brief C 文字列を `stdObject` として扱う文字列クラス。/ String class that wraps a C string as a `stdObject`.
 * @details
 * `stdArray<char>` を継承し、null 終端文字列として管理する。
 * イミュータブルではない — `add()` は新しいオブジェクトを返し、`addIn()` はインプレース追加。
 * 正規表現マッチ (`rx()`) も内蔵している。
 *
 * よく使うコンストラクタ:
 * - `stdString("literal")` — C 文字列から生成
 * - `stdString("%d", n)` — sprintf 形式でフォーマット
 * - `stdString(str, sp, ep)` — 部分文字列 [sp, ep)
 *
 * / Inherits `stdArray<char>`. Not immutable: `add()` returns a new object; `addIn()` appends in-place.
 * Built-in regex support via `rx()`.
 */
class stdString : public stdArray<char> {
public:
	stdString(const char * name);
	stdString(sPtr<stdString>  s);
	stdString(int);
	stdString(const char * fmt,int data);
	stdString(const char * fmt,INTEGER64 data);
	stdString(const char * fmt,double data);
	stdString(const char * fmt,void * data);
	stdString();
	stdString(char ch);
	stdString(const char * str,int sp,int ep);
	stdString(sPtr<stdString>  str,int sp,int ep);
	stdString(sPtr<stdString>  str,int sp);

	~stdString() {
		rx_work = thNULL;

	};

	/** @brief 文字列長 (null 終端を除く)。/ String length excluding null terminator. */
	virtual int length();
	virtual void length(int);
	/** @brief 文字列を連結した新しい stdString を返す。/ Returns a new stdString with str appended. */
	sPtr<stdString>  add(sPtr<stdString>  str);
	sPtr<stdString>  add(const char * str) {
		return add(thNEW( stdString,(str)));
	}
	/** @brief C 文字列ポインタを返す。/ Returns the raw C string pointer. */
	const char * get_str()
	{
	  	return &this->ary[0];
	}
	/** @brief 文字列を INTEGER64 に変換する。/ Parse the string as INTEGER64. */
	INTEGER64 get_int()
	{
	INTEGER64 ret;
		::sscanf(&this->ary[0],"%lli",&ret);
		return ret;
	}
	double get_flt() {
	double ret;
		::sscanf(&this->ary[0],"%lf",&ret);
		return ret;
	}
	void addIn(sPtr<stdString>  str);
	/** @brief strcmp 相当の比較。/ strcmp-equivalent comparison. */
	int cmp(sPtr<stdString>  str);
	int cmp(const char * str);
	sPtr<stdString>  to_upper();
	sPtr<stdString>  to_lower();
	void copyout(char * dest,int size);
	void copyin(char * dest,int size);
	int partial_cmp(const char * str);
	int partial_cmp(sPtr<stdString>  str);

	/** @brief URL パーセントエンコード。/ URL percent-encode the string. */
	sPtr<stdString>  percent_encode();
	sPtr<stdString>  percent_decode();

	/** @brief POSIX 拡張正規表現で操作する。method: `""` / `"m"` / `"mv"` / `"s"` / `"sg"` 等。/ POSIX ERE operations (match, replace, etc.). */
	sPtr<stdString>  rx(const char * method,sPtr<stdRx>  re,const char * rstr);
	sPtr<stdString>  rx(const char * method,sPtr<stdRx>  re,sPtr<stdString>  rstr=thNULL);
	sPtr<stdString>  rxVar(int no);
	sPtr<stdString>  rxNext();
	sPtr<stdString>  rxPrev();

	unsigned debug:1;

private:
	sPtr<rxWork> 	rx_work;

};

inline static sPtr<stdString>  STR_NULL(sPtr<stdString>  inp)
{
	if ( inp == thNULL )
		return thNEW( stdString,(""));
	return inp;
}

#endif

