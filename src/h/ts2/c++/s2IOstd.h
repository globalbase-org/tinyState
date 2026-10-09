
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
 *
 * **Windows (MinGW) の制約**: パイプ / ファイルの口は overlapped I/O を要求する。
 * MinGW のバックエンドは OS スレッドプールの完了通知でデータを動かすため、
 * `FILE_FLAG_OVERLAPPED` で開かれた HANDLE でないと `read()`/`write()` が
 * `ENOTSUP` で失敗する (診断行が 1 度だけ出る)。tinyState の `ts2System` が作る
 * 子プロセスのパイプ端はその条件を満たすので、**tinyState の親から起動された
 * tinyState の子は普通に動く**。満たさないのは *tinyState 以外の親* から継承した
 * 口で、代表例が **シェルのパイプ** (`cmd | app.exe`) — Windows はこれを同期
 * ハンドルとして作るので、標準入力は読めない。コンソール (`FILE_TYPE_CHAR`) は
 * `ts2IOwinConsole` が readiness で扱うのでこの制約を受けない。
 * POSIX (Linux / Cygwin) は素の fd + `select` なのでいずれも制限は無い。
 *
 * / **Constraint on Windows (MinGW)**: the pipe/file path requires overlapped I/O.
 * The MinGW backend moves data through OS thread-pool completions, so a HANDLE not
 * opened `FILE_FLAG_OVERLAPPED` makes `read()`/`write()` fail with `ENOTSUP` (one
 * diagnostic line is printed).  Pipe ends created by tinyState's `ts2System` do
 * satisfy it, so **a tinyState child started by a tinyState parent works normally**.
 * What does not is a stream inherited from a *non-tinyState* parent — typically a
 * **shell pipe** (`cmd | app.exe`), which Windows creates synchronous, leaving stdin
 * unreadable.  A console (`FILE_TYPE_CHAR`) goes through `ts2IOwinConsole` instead
 * and is not affected.  POSIX (Linux / Cygwin) uses plain fds + `select`: no limit.
 */
class s2IOstd {
public:
	/** @return 0=成功 / <0=エラー (要求した口のいずれかの生成に失敗)。
	 *  @note 生成は成功しても、Windows(MinGW) で overlapped でない口は最初の
	 *        `read()`/`write()` が `ENOTSUP` で落ちる (上記の制約参照)。init は
	 *        HANDLE が取れたかだけを見るので、ここでは分からない。 */
	static int init(
		sPtr<tinyState>	parent,
		sPtr<ts2IO> *	in_p=0,
		sPtr<ts2IO> *	out_p=0,
		sPtr<ts2IO> *	err_p=0);
};

#endif
