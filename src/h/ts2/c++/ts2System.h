
#ifndef ___ts2System_cpp_H___
#define ___ts2System_cpp_H___

/** @defgroup ts2system_dmode ts2System destroy モード / ts2System destroy mode
 * @brief ts2System::destroy(int mode) に渡すモード定数。/ Mode constants for ts2System::destroy(int mode).
 * @{
 */
#define DM_CMD		0x00ff  ///< dmode の下位バイトのマスク (モード部分)。/ Mask for mode part of dmode.
#define DM_NORMAL	0       ///< SIGTERM を 1 回送って終了を待つ (デフォルト)。/ Send SIGTERM once and wait.
#define DM_CONT_TERM	1   ///< SIGTERM を送り 1 秒ごとに再送し続ける。/ Send SIGTERM every 1 s until exit.
#define DM_SETPGID	2       ///< (廃止予定) setpgid は現在デフォルト動作。SIGTERM 1 回。/ (deprecated) setpgid is now default. Single SIGTERM.
#define DM_CONT_KILL	3   ///< SIGKILL を送り 1 秒ごとに再送し続ける。確実に殺したい場合。/ Send SIGKILL every 1 s until exit.

#define DM_FLAG		0xff00  ///< dmode の上位バイトのマスク (フラグ部分)。/ Mask for flag part of dmode.
#define DM_TTY		0x0100  ///< 子プロセスの stdio を PTY で接続する。**Windows(MinGW) では未対応**。/ Connect child stdio via PTY. **Not supported on Windows (MinGW).**
#define DM_APPLY	0x0200  ///< 呼び出し元が fd を事前に用意して渡す。/ Caller pre-provides file descriptors.
/** @} */

/** @brief ts2System で取得した rfd/wfd/efd が不要になったときに destroy する便利マクロ。
 *  / Convenience macro to destroy rfd/wfd/efd obtained from ts2System when no longer needed.
 */
#define GCIO(ior,iow,ioe)	\
	if ( ior.is_notNull() )					\
		((sPtr<tinyState> )ior)->destroy();		\
	if ( iow.is_notNull() )			\
		((sPtr<tinyState> )iow)->destroy();		\
	if ( ioe.is_notNull() )			\
		((sPtr<tinyState> )ioe)->destroy();

/** @page ts2system_cmdline ts2System のコマンド文字列 / ts2System command string
 *
 * @section ts2system_cmdline_modes 先頭文字が起動方式を選ぶ / The leading character selects the launch mode
 *
 * | 文字列 / String | 起動方式 / Launch | `retp` に入る PID / PID written to `retp` |
 * |---|---|---|
 * | `"cmd arg1"` (通常 / plain) | `sh -c` 経由。シェル展開・パイプ・リダイレクトが使える / via `sh -c`; shell expansion, pipes and redirection work | `sh` の PID。実プロセスは孫 / PID of `sh`; the real process is a grandchild |
 * | `"#cmd arg1"` (`#` 始まり / `#`-prefixed) | 空白区切りで直接起動。`sh` を挟まない / split on spaces and exec'd directly; no `sh` | 実プロセスの PID / PID of the real process |
 *
 * @section ts2system_cmdline_win プラットフォーム差 / Platform difference
 *
 * **Windows (MinGW) は `#` 始まりのみ受け付ける。** `sh` の存在を前提にできないため、`#` の無い
 * 文字列は起動されず `retp` に <b>`-6`</b> が入る。cmd.exe へ黙って差し替えることはしない
 * (クォート規則も終了コードの意味も異なるため、別物を起動するより呼び出し側に制御を返す)。
 *
 * / **Windows (MinGW) accepts only the `#`-prefixed form.** There is no `sh` to assume, so a
 * plain string is not launched and `retp` receives <b>`-6`</b>. It is deliberately *not* rerouted
 * to cmd.exe: the quoting rules and the meaning of exit codes both differ, so the call fails
 * and the caller keeps control rather than silently running something else.
 *
 * @section ts2system_cmdline_portable 移植する場合 / Writing portable code
 *
 * **常に `#` を付けておけばよい。** POSIX 側でも直接起動になるだけで、シェル機能
 * (展開・パイプ・リダイレクト) を使っていない限り挙動は変わらず、`retp` が実プロセスの PID に
 * なるぶんむしろ扱いやすい。シェル機能が必要なら、それは POSIX 専用のコードになる。
 *
 * / **Prefix `#` everywhere.** On POSIX that merely skips the shell: unless you rely on shell
 * features the behaviour is unchanged, and `retp` becoming the real PID is easier to work with.
 * If you do need shell features, that code is POSIX-only by construction.
 *
 * @see DM_TTY — PTY 接続も Windows(MinGW) では未対応 / also unsupported on Windows (MinGW)
 */

#include	"_ts2/c++/ts2System_pb.h"

#endif

