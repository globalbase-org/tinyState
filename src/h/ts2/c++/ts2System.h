
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
#define DM_TTY		0x0100  ///< 子プロセスの stdio を PTY で接続する。/ Connect child stdio via PTY.
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

#include	"_ts2/c++/ts2System_pb.h"

#endif

