

#ifndef ___stdRx_cpp_H___
#define ___stdRx_cpp_H___

#include	<regex.h>
#include	"ts2/c++/sPtr.h"

#define ERROR_BUF_SIZE	100

class stdString;
/**
 * @brief POSIX 拡張正規表現 (`regcomp`/`regexec`) のラッパー。/ POSIX extended regex (`regcomp`/`regexec`) wrapper.
 * @details
 * `stdString::rx()` と組み合わせて使う。コンパイル済みの `regex_t` を保持し、
 * 同じパターンを繰り返し使う場合にオブジェクトとして保持することでコンパイルコストを節約できる。
 * / Used with `stdString::rx()`. Holds a compiled `regex_t`; reuse the object to amortize compile cost.
 *
 * @par 使用例 / Example
 * @code{.cpp}
 * sPtr<stdRx> re = thNEW(stdRx, ("^Host: (.+)"));
 * if (line->rx("mv", re).is_notNull()) {
 *     host = line->rxVar(1);
 * }
 * @endcode
 */
class stdRx : public stdObject {
public:
	stdRx(const char * pattern,int flags=0);
	stdRx(sPtr<stdString>  pattern,int flags=0);
	~stdRx();

	sPtr<stdString>  pattern() {
		return _pattern;
	}

	regex_t re;     ///< コンパイル済み正規表現。/ Compiled regex.
	int numpunc;   ///< キャプチャグループ数 + 1。/ Number of capture groups + 1.
	int err;       ///< regcomp のエラーコード。0 で成功。/ regcomp error code; 0 on success.

private:
	void initial(int flags);

	sPtr<stdString> 	_pattern;
};


#endif
