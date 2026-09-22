#ifndef ___tsProbe_cpp_H___
#define ___tsProbe_cpp_H___

#include	"ts2/c++/sPtr.h"

class tinyState;

/**
 * @brief 撤収中にスレッドプールへ仕事を積みに来た相手を素性ごと記録する診断プローブ。
 *        / Diagnostic probe: record *who* tried to queue work while the pool was winding down.
 * @details
 * 既存の表明は「捨てられる側」(`printParent()`) しか出さない。窓 A に至っては
 * `ins()` が成功してしまうので一行も出ない。ここでは **積みに来た側**を、
 * 呼び出しスタックごと固定して記録する。狙いは「誰がそこまで居残っているのか」。
 *
 * **既定では無効**。環境変数で有効化する:
 *
 * | 環境変数 | 意味 |
 * |---|---|
 * | `TS2_TEARDOWN_PROBE`       | 出力先のファイルパス。`-` で stderr。**未設定なら完全に無効** |
 * | `TS2_TEARDOWN_PROBE_ABORT` | `0` 以外なら記録後に `abort()` (core を取る) |
 * | `TS2_TEARDOWN_PROBE_MAX`   | 1 プロセスあたりの記録上限 (既定 64・`0` で無制限) |
 * | `TS2_TEARDOWN_PROBE_DEPTH` | スタックの段数 (既定 24・上限 64) |
 * | `TS2_TEARDOWN_PROBE_FORCE` | **較正用**。`0` 以外なら撤収前の通常の `ins()` も窓 `"N"` として記録する |
 *
 * ★ **stdout へは出さない**。ctest の PASS/FAIL_REGULAR_EXPRESSION に当たるのと、
 *   出力量が増えると ctest 側が破滅的バックトラックで空回りする前例があるため。
 *
 * @param job    積まれようとした tinyState (その TS_THREAD 状態を走らせたい側)
 * @param window 窓の識別。`"A"` = 受理されたが誰も走らせない / `"B"` = プールが畳み済みで拒否 /
 *               `"C"` = プールそのものが既に無い
 * @param pool   プール側のスナップショット文字列。呼び出し側が `mtx` の下で作る
 *               (このプローブは `mtx` を握らないし、握ったまま呼んでもならない)
 */
void	tsProbeTeardown(sPtr<tinyState> job,const char * window,const char * pool);

/**
 * @brief プローブが有効かどうか。/ Is the probe armed?
 * @details
 * 無効なら呼び出し側はスナップショット文字列の生成ごと省ける。環境変数は初回に
 * 一度だけ読む。
 *
 * ★ 「0 件だった」と言う前に、検出器が鳴ることを先に確かめられるようにしてある。
 *   `TS2_TEARDOWN_PROBE_FORCE=1` で撤収前の通常の `ins()` まで記録させれば、出力と
 *   grep が噛み合うことを確認できる。
 *
 * @return `0` = 無効 / `1` = 有効 / `2` = 有効かつ較正モード (FORCE)
 */
int	tsProbeTeardown_enabled();

#endif
