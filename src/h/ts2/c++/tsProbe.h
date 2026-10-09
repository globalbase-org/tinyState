#ifndef ___tsProbe_cpp_H___
#define ___tsProbe_cpp_H___

#include	"ts2/c++/sPtr.h"
#include	"ts2/c++/ts_types.h"

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


/**
 * @brief 「深すぎる入れ子を gc へ逃がした」を 1 件記録する。
 *        / Record one eventHandler entry handed to the GC because it nested too deep.
 * @details
 * 狙いは **「このアプリは本当に上限へ届いているのか」を測れるようにすること**。
 * 既定の上限は 8MB のスタックで 1500 段なので、普通のアプリは 1 件も出ない見込みで、
 * **0 件であることが示せれば順序入れ替わりの影響もゼロ**と言える。
 *
 * | 環境変数 | 意味 |
 * |---|---|
 * | `TS2_EVH_PROBE`     | 空でなければ **1 件ごとに stderr へ 1 行**。未設定なら数えるだけ |
 * | `TS2_EVH_PROBE_MAX` | 行数の上限 (既定 64・`0` で無制限)。打ち切ったことは**逃がし側・配送側それぞれで 1 度**言う |
 *
 * 行の形 (タグで grep する):
 * @code
 * TS2-EVH escape seq=3 class=hwDepth ev=0x20 depth=1501 limit=1500 tid=...
 * @endcode
 *
 * ★ **「0 件だった」と言う前に検出器が鳴ることを確かめられる**: アプリ側で
 *   `application->evh_depth = 2` にすれば逃がしは必ず起きるので、それが陽性対照になる。
 *
 * ★ stdout ではなく stderr へ出す (ctest の PASS/FAIL_REGULAR_EXPRESSION に当たらない
 *   ように — 上の撤収プローブと同じ理由)。
 *
 * @param job    逃がされた側の tinyState
 * @param evtype そのとき配送されようとしていた `stdEvent::type`
 * @param depth  逃がした時点の入れ子段数
 * @param limit  その スレッドで有効だった上限
 */
INTEGER64	tsProbeEvhEscape(sPtr<tinyState> job,int evtype,int depth,int limit);

/**
 * @brief 逃がした入場を **配送し直した** ことを記録する。/ Record that a deferred entry was delivered.
 * @details
 * 狙いは **「逃がした ev は本当に全部届いているのか」を外から数えられるようにすること**。
 * 逃がし側の行と配送側の行が `seq` で 1 対 1 に対応するので、
 *
 * @code
 * $ TS2_EVH_PROBE=1 TS2_EVH_PROBE_MAX=0 ./app 2>&1 | grep TS2-EVH >/tmp/p
 * $ grep -c 'escape seq=' /tmp/p ; grep -c 'deliver seq=' /tmp/p      # 数が揃うか
 * $ diff <(grep -oP 'escape seq=\K[0-9]+' /tmp/p | sort -n) \
 *        <(grep -oP 'deliver seq=\K[0-9]+' /tmp/p | sort -n)   # 揃わないならどの seq か
 * @endcode
 *
 * で「落ちた ev が在るか」「在るならどれか」が切れる。これが無いと、利用側は
 * 「順序が変わって自分のコードが壊れた」のか「ev が届いていない」のかを
 * **見分けられない** (利用側からの依頼で追加)。
 *
 * ★ 配送は **gc の側** (tsGC の ACT_EXE) で行われるので、逃がした側とは
 *   別のスレッド・別のディスパッチになる。`seq` だけが両者を結ぶ。
 *
 * @param job    配送された tinyState
 * @param evtype 配送した `stdEvent::type`
 * @param seq    逃がしたときに `tsProbeEvhEscape()` が返した通し番号 (0 = 逃がし由来でない)
 */
void		tsProbeEvhDeliver(sPtr<tinyState> job,int evtype,INTEGER64 seq);

/**
 * @brief 逃がした延べ件数。/ Total number of entries handed to the GC so far.
 * @details
 * 環境変数とは無関係に**常に数えている** (逃がし自体が稀なので、1 件につき
 * アトミック加算 1 回のコストしか無い)。試験から
 * 「この走行では 1 件も逃がしていない」を主張するのに使える。
 */
INTEGER64	tsProbeEvhEscapeCount();

/** @brief 逃がしたものを配送し直した延べ件数。/ Total deferred entries delivered so far.
 *  @details `tsProbeEvhEscapeCount()` と**揃うはず**。揃わなければ、その差が
 *  「gc のキューに残っている」か「落ちた」かのどちらか。 */
INTEGER64	tsProbeEvhDeliverCount();

#endif
