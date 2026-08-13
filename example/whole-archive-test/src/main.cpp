/*
 * whole-archive test: tinyState2 + tinyState2Math の全シンボル取り込みテスト
 *
 * tinyState2.a / tinyState2Math.a を --whole-archive でリンク可能か確認する。
 * これにより以下を検証する:
 *   1. stdBalancedTree の 3 つの未定義 virtual が実装されている
 *   2. テンプレート派生が実体化され、単体でのリンク不能な隠れた参照がない
 *   3. GMP/MPFR シンボルがすべて外部依存として解決可能
 *
 * このモジュールは dlopen されることを想定し、機能実装は最小限
 * (呼び出し可能な exported symbol があれば十分)。
 */

#include "ts2/c++/stdInterval.h"
#include <cstdint>

extern "C" {
	/* 呼び出し可能な exported symbol。ローダーが存在を確認するために必要 */
	int whole_archive_test_probe(void)
	{
		/* tinyState2 のシンボル参照を作成。何かを呼び出して生存性を確認 */
		int64_t now = stdInterval::now();
		return (int)(now & 1);
	}
}
