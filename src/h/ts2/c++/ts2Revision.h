

#ifndef ___ts2Revision_cpp_H___
#define ___ts2Revision_cpp_H___

/**
 * @brief 走っている tinyState 自身の版を返す。/ Return the revision of the
 *        tinyState build that is actually running.
 * @details
 * 返す文字列は `git describe --tags --always --dirty --long` の出力そのままで、
 * **ライブラリを建てたときの configure 時点**の値 (`TS_REVISION` と同じ文字列)。
 *
 * ★ **ヘッダのマクロと違い、これは成果物に入る**。マクロ (`TS_REVISION`) が
 * 答えるのは「**いまコンパイルしているソースが見ているヘッダ**」の版で、
 * リンクされた実体の版ではない。両者は食い違い得る (古い /usr/local に対して
 * 新しいヘッダで建てた、生成ヘッダが stale、静的と共有を別の木から建てた 等)。
 * 「**何が動いているのか**」を知りたいならこちらを使うこと。
 *
 * 実行せずに調べるなら、消費側の実行体か .so を `strings` / `nm` で見る:
 *
 *     nm -C --defined-only libmyapp.so | grep ts2_revision
 *     strings myapp | grep -m1 'v2\.0\.0'
 *
 * @note ★ **.a を見てはいけない**。静的ライブラリでは参照されない .o がリンカに
 *       落とされるので「.a に在る」は「exe に入った」を意味しない。逆に消費側の
 *       実行体が薄い (中間の .so に実体が在る) 場合、文字列はその .so 側に入る。
 *       so **持ち主の側**を 1 本当ててから「無い」と言うこと。
 * @note この関数は常に引かれる TU (`tinyState.cpp`) に置いてある。版の文字列を
 *       専用の TU に分けると、誰からも参照されないので静的リンクで落ちる。
 *
 * @return NUL 終端の版文字列。**常に非 NULL** (git 管理外で建てた場合は
 *         `"unknown"`)。静的な寿命を持つので解放してはならない。
 */
const char *	ts2_revision();

#endif	/* ___ts2Revision_cpp_H___ */
