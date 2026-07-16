
#ifndef ___s2IOstd_h___
#define ___s2IOstd_h___

#include	"ts2/c++/tinyState.h"
#include	"ts2/c++/ts2IO.h"

/**
 * @brief 起動されたプロセスの標準入出力を ts2IO 化する portable ファクトリ。
 *        / Portable factory wrapping a process's own stdin/stdout/stderr as ts2IO.
 * @details
 * アプリが生 fd/HANDLE を触らずに標準ストリームを ts2IO として得るための静的
 * ユーティリティ (tinyState でも stdObject でもないので `s2` 接頭辞)。arch 別実装:
 * Linux は fd 0/1/2 を ts2IOdescriptor で包む。Windows は GetStdHandle +
 * GetFileType 判定で、コンソールなら ts2IOwinConsole、パイプ等なら ts2IOdescriptor。
 * ポインタが 0 の口は生成しない。Windows-port design memo §9。
 */
class s2IOstd {
public:
	/** @return 0=成功 / <0=エラー (要求した口のいずれかの生成に失敗)。 */
	static int init(
		sPtr<tinyState>	parent,
		sPtr<ts2IO> *	in_p=0,
		sPtr<ts2IO> *	out_p=0,
		sPtr<ts2IO> *	err_p=0);
};

#endif
