# tinyState マクロリファレンス

> v0 草案 (2026-07-14)
> [CLAUDE.md](../CLAUDE.md) の「マクロ覚え書き」の詳細版。Open issue「`tscpp2 new` が生成する引数セットアップマクロの正式名称・使い方」に対応。

tinyState のマクロは出所が 2 系統ある。混同するとハマるので最初に押さえる。

| 出所 | 実体の場所 | 例 |
|---|---|---|
| **コアヘッダ** (手書き・固定) | `src/h/ts2/c++/tinyState.h` ほか | `TS_STATE`, `thNEW`, `C_TEST`, `rDO`, `R_TEST` |
| **tscpp2 生成** (クラスごとに毎回作られる) | `build/_ts2/c++/<foo>_.h` / `<foo>_pb.h` | `TS_ARGS0`, `TS_CPARGS0`, `TS_DEFARGS`, `TS_THISCLASS`, `TS_BASECLASS` |

生成系マクロは **その `.cpp` から `#include "_ts2/c++/<foo>_.h"` した文脈でしか正しく展開されない**。同じ名前 (`TS_ARGS0` 等) でもクラスごとに中身が違う。ビルドが通らないときは「生成ヘッダが古い/未生成」をまず疑う。

---

## 1. クラス骨組みマクロ

`tscpp2 new` が吐くテンプレートの骨格。詳細な生成フローは [COOKBOOK.md セクション 0](COOKBOOK.md) 参照。

```cpp
#include	"ts2/c++/tsApplication.h"
#include	"_ts2/c++/tsHoge_.h"

CLASS_TINYSTATE(ts2/c++/tsHoge, ts2/c++/ts2IO)   // (このクラスのパス, ベースクラスのパス)

#if 0
TS_BEGIN_IMPLEMENT
class TS_THISCLASS : public TS_BASECLASS {
    ...
};
TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine (前方宣言) をここに
TS_END_INTERFACE
#endif
```

| マクロ | 役割 |
|---|---|
| `CLASS_TINYSTATE(type, base)` | クラス登録の親玉。`type`/`base` は**ヘッダルートからのパス**表記 (`ts2/c++/tsHoge`)。多重継承不可の根拠 (ベースを 1 つしか取れない、[CLAUDE.md 鉄則 4](../CLAUDE.md)) |
| `TS_BEGIN_IMPLEMENT` … `TS_END_IMPLEMENT` | **実装クラス** (`_` 付き内部クラス) の宣言ブロック。`#if 0` で囲う (C++ からは見えない。tscpp2 だけが読む)。`/* */` はネスト不可なので `#if 0` を使う |
| `TS_BEGIN_INTERFACE` … `TS_END_INTERFACE` | 公開ヘッダに出す前方宣言・include を書くブロック |
| `TS_THISCLASS` | このクラスの実装クラス名 (`tsHoge_`) に展開。tscpp2 が `#define TS_THISCLASS tsHoge_` を生成 |
| `TS_BASECLASS` | ベースクラスの実装クラス名 (`ts2IO_`) に展開 |

`#if 0` ブロックは C++ コンパイラからは丸ごと無視される。**tscpp2 がソースをテキストとして走査**し、この中の宣言を拾って `_ts2/c++/<foo>_.h` / `<foo>_pb.h` を生成する。

---

## 2. 引数セットアップマクロ (TS_DEFARGS / TS_ARGS&lt;N&gt; / TS_CPARGS&lt;N&gt;)

**tinyState 流コンストラクタの核心。** コンストラクタの「引数リスト」「メンバ宣言」「メンバへの代入」の 3 つを、**コンストラクタ宣言 1 箇所を単一ソース**にして自動生成する仕組み。手で 3 回書かない。

### 3 つのマクロの役割

`#if 0` 実装ブロックに**コンストラクタを宣言**し、メンバ部に `TS_DEFARGS` を置く:

```cpp
#if 0
TS_BEGIN_IMPLEMENT
class TS_THISCLASS : public TS_BASECLASS {
public:
	tsFoo_(
		sPtr<tsApplication>	parent,     // ← 第1引数 parent は特別扱い (後述)
		sPtr<stdString>		name,
		int			port);
private:
protected:
	TS_DEFARGS      // ← parent 以外の引数が「メンバ宣言」として展開される
};
TS_END_IMPLEMENT
...
#endif
```

これを tscpp2 に食わせると `_ts2/c++/tsFoo_.h` に次が生成される (概念):

| マクロ | 展開内容 | 由来 |
|---|---|---|
| `TS_ARGS0` | `sPtr<tsApplication> parent, sPtr<stdString> name, int port` | コンストラクタ宣言の**引数リストそのもの** (デフォルト値は除去) |
| `TS_CPARGS0` | `this->name = name; this->port = port;` | 各引数を**同名メンバへ代入する本体** (`parent` は除外) |
| `TS_DEFARGS` (クラス本体内) | `sPtr<stdString> name; int port;` | 各引数の**メンバ宣言** (`parent` は除外) |

### コンストラクタ本体はこう書く

```cpp
tsFoo_::tsFoo_(TS_ARGS0)
	: ts2IO_(parent),              // ベースコンストラクタには parent を渡す
	  parent(tinyState_::parent)   // parent メンバは明示初期化
{
	TS_CPARGS0                     // name/port は自動代入
}
```

引数を追加/変更したいときは **`#if 0` ブロックの宣言を直すだけ**。tscpp2 が `TS_ARGS0`/`TS_CPARGS0`/`TS_DEFARGS` を作り直すので、コンストラクタ本体・メンバ宣言は無修正で追随する。

### なぜ `parent` だけ特別か

すべての tinyState は生成時に親を要求する (`application->mtx` を参照できる必要があるため、[CLAUDE.md「親なしで tinyState を作る」](../CLAUDE.md) 参照)。`parent` は**ベースクラスへ受け渡す**もので自分のメンバにはしないため、`TS_CPARGS`/`TS_DEFARGS` の自動代入・自動宣言から除外される。第 1 引数を `parent` という名前にするのは規約。

### 複数コンストラクタ (TS_ARGS1, TS_ARGS2, …)

実装ブロックにコンストラクタを複数宣言すると、**宣言順に 0, 1, 2 … と採番**され、それぞれ `TS_ARGS<N>` / `TS_CPARGS<N>` が生成される。

```cpp
// 宣言 (implement ブロック)
ts2IOsockTCPconnect_(sPtr<tinyState> parent);                          // → TS_ARGS0 / TS_CPARGS0
ts2IOsockTCPconnect_(sPtr<tinyState> parent, sPtr<stdString> target);  // → TS_ARGS1 / TS_CPARGS1

// 定義
ts2IOsockTCPconnect_::ts2IOsockTCPconnect_(TS_ARGS0)
	: ts2IOdescriptor_(parent) { TS_CPARGS0 }

ts2IOsockTCPconnect_::ts2IOsockTCPconnect_(TS_ARGS1)
	: ts2IOdescriptor_(parent) {
	TS_CPARGS1                          // 自動代入ぶん
	target_name = target->get_str();    // 追加の手当ては後ろに足せる
}
```

### ⚠ 落とし穴: デフォルト引数の `{}`

コンストラクタ宣言のデフォルト値 (`= ...`) は `TS_ARGS<N>` 生成時に除去される。ただし **`{}` (空ブレース) をデフォルト値に使うと `TS_ARGS0` 生成が壊れる**。デフォルトが要る場合は `{}` ではなく **`nullptr` / 明示値**を使うこと。

```cpp
tsFoo_(sPtr<tsApplication> parent, sPtr<stdString> name = {});        // ✗ 生成失敗
tsFoo_(sPtr<tsApplication> parent, sPtr<stdString> name = nullptr);   // ✓
```

（背景: メモリ `feedback_tscpp2_default_brace` 参照）

---

## 3. メンバ変数の分散宣言 (TS_PRIVATE / TS_PROTECTED)

`#if 0` 実装ブロックに全メンバをまとめて書く代わりに、**使う状態関数の直前**でメンバを宣言できる。tscpp2 が拾って生成クラスの private/protected セクションに出力する。C++ からはマクロが空展開なのでコンパイルに影響しない。

```cpp
TS_PRIVATE(int counter;)          // private セクションへ
TS_PROTECTED(sPtr<stdString> buf;) // protected セクションへ

TS_STATE(ACT_START) {
	counter = 0;
	...
}
```

- 実装ブロック (`#if 0 … TS_END_IMPLEMENT`) の**外**に置く。通常は使う状態関数の直前。
- 同じ変数を 2 箇所で宣言すると重複メンバでコンパイルエラー。
- `TS_DEFARGS` で生えるコンストラクタ引数メンバと名前が衝突しないよう注意。

---

## 4. 状態関数 (TS_STATE / TS_THREAD)

状態遷移関数の定義マクロ。使い分けは [CLAUDE.md「TS_STATE / TS_THREAD 使い分け」](../CLAUDE.md)。

```cpp
TS_STATE(ACT_FOO) {          // mtx 保持のまま短時間で回す状態
	...
	return rDO | ACT_BAR;    // 即 ACT_BAR / return 0 で yield
}

TS_THREAD(INI_START) {       // mtx を一時解放。blocking I/O・長ループ可
	...
}
```

- 展開すると `state_##x` (ディスパッチ用 static) と `_state_##x` (インライン実体) の 2 関数になる。
- **どちらも `return` 値だけが正規の遷移ルート** ([CLAUDE.md 鉄則 2](../CLAUDE.md))。`_state = ...` 直書きは NG。
- 状態名の**先頭 3 文字**がカテゴリを決める (`INI` / `ACT` / `FIN` …、§6)。命名時に必ずこの規約に乗せる。

---

## 5. オブジェクト生成・自己参照

| マクロ | 展開 / 意味 |
|---|---|
| `thNEW(Class, (args))` | **唯一の正規 new**。`sPtr<Class>(new (__FILE__,__LINE__) Class args, 1)`。refcount を正しく初期化する。生 `new` 禁止 ([CLAUDE.md 鉄則 1](../CLAUDE.md)) |
| `thNULL` | `sPtr` の null sentinel。`p = thNULL` で解放 |
| `ifThis` | `this->ifp`。**インタフェースクラス**の自己 `sPtr`。他オブジェクトに自分を見せるときはこれ |
| `thThis` | `sPtr<typeof(*this)>(this)`。**実装クラス**の自己 `sPtr`。外に漏らさない (内部用) |

`ifThis` / `thThis` の使い分けは [CLAUDE.md「ifThis vs thThis」](../CLAUDE.md) 参照。他オブジェクトへ渡すイベントの source には `ifThis` を使う。

---

## 6. 状態ビットと判定

状態カテゴリのビットフラグ (`tinyState.h`)。状態名先頭 3 文字と対応する。

| ビット | 意味 | 対応する状態名接頭辞 |
|---|---|---|
| `C_INI` (001) | 初期化中 | `INI_*` |
| `C_ACT` (002) | 稼働中 | `ACT_*` |
| `C_FIN` (004) | 終了処理中 | `FIN_*` |
| `C_ZOM` (010) | ゾンビ (終了済み) | — |
| `C_ERR` (020) | エラー | `ERR_*` |
| `C_PUB` (040) | published | `PUB_*` |
| `C_THR` (0100) | スレッド実行中 (`TS_THREAD`) | — |

| マクロ | 意味 |
|---|---|
| `C_TEST(state, bit)` | `state` が `bit` カテゴリか判定。**他オブジェクトの死亡確認は `C_TEST(a->state(), C_ZOM)`** ([CLAUDE.md sPtr 節](../CLAUDE.md)。`a->is_destroyed()` を外部から呼ぶのは誤り) |
| `C_NAME(state)` | 状態名の文字列を返す (デバッグ用) |
| `rDO` | 遷移値の下位ビット。`return rDO \| ACT_NEXT` で「今すぐ ACT_NEXT を実行」。`rDO` 無しの `return ACT_NEXT` は「次のイベントを待ってから」 |

---

## 7. イベント受信の補助

`TS_STATE` の `ev`（受信イベント）を捌く短縮マクロ。

| マクロ | 展開 | 用途 |
|---|---|---|
| `R_TEST` | `if ( ev->type != TSE_RETURN ) return 0;` | 子からの `TSE_RETURN` 以外は yield。「子の完了待ち」定番 |
| `R_SET(x)` | `R_TEST` + `x = x.d_cast(ev->msg_obj)` | 完了待ち + 戻り値オブジェクトを `x` へ d_cast 代入 |

### ⚠ 廃棄予定 (新規コードでは使わない)

| マクロ | 状態 |
|---|---|
| `R_REF(x)` | v1 互換。`R_SET` と同義。**新規コード禁止** |
| `S_SET(x)` | v1 互換 (`ev->msg_obj` を `stdObject` 経由で d_cast)。**新規コード禁止** |

---

## 8. 生成の裏側 (通常は触らない)

`CLASS_TINYSTATE` 展開時、tscpp2 は登録用のマクロ塊も生成する。ユーザが直接書くことはまずない。

| マクロ | 役割 |
|---|---|
| `TS_IMPLEMENT` | クラス名・`getClass`/`getRefer`・全状態の遷移テーブル (`TS_TRANS`) をまとめて定義。`.cpp` の末尾で一括展開される |
| `TS_REFER` | クラス参照情報 (`{"クラス名", 0}`) の型 |
| `TS_TRANS` | 状態 1 つぶんの遷移エントリ (`CC_<CAT> "状態名"` と関数ポインタ) |

これらは `_ts2/c++/<foo>_.h` に生成される。挙動を追うときは生成ヘッダを直接読むと早い。

---

## 関連ドキュメント

- [CLAUDE.md](../CLAUDE.md) — 鉄則・禁止リスト・推奨イディオム (まずこれ)
- [COOKBOOK.md](COOKBOOK.md) — 「したいこと → コード」。セクション 0 に `tscpp2 new` の使い方
- [MENTAL_MODEL.md](MENTAL_MODEL.md) — 状態機械としての世界観
- [BUILD_INTERNAL.md](BUILD_INTERNAL.md) — tscpp2 がいつ何を生成するか
