
#ifndef ___ts2IO_cpp_H___
#define ___ts2IO_cpp_H___

/** @defgroup ts2io_state ts2IO::state の値 / ts2IO::state values
 * @brief `ts2IO::state` が取りうる値。/ Possible values of `ts2IO::state`.
 * @{
 */
#define TS2IO_INIT	0  ///< 初期状態。接続前。/ Initial state; not yet connected.
#define TS2IO_ACTIVE	1  ///< 接続済み・動作中。/ Connected and active.
#define TS2IO_CLOSE	2  ///< 正常クローズ済み。/ Closed normally.
#define TS2IO_ERROR	3  ///< エラー発生。`err` にエラーコードが入る。/ Error occurred; see `err`.
/** @} */

#include	"_ts2/c++/ts2IO_pb.h"

#endif

