# tinyState リファレンス — イベント型 / ユーティリティ / エラー / デバッグ

> v0 草案 (2026-07-14)
> **コード由来の事実**（ヘッダの定義・実使用）をまとめたもの。`要ひさ確認` と付いた箇所は
> 設計意図の推測を含むので、ひさのレビュー前は鵜呑みにしないこと。

[CLAUDE.md](../CLAUDE.md) / [COOKBOOK.md](COOKBOOK.md) の「したいこと→コード」を引いた後の、
「型・API を一覧で確認したい」向けの索引。

---

## 1. `TSE_*` イベント型カタログ

`ev->type` に入る値。定義は `src/h/ts2/c++/stdEvent.h`。`TS_STATE(...)` の `ev` を
`if ( ev->type == TSE_XXX )` で判定する。

### 1.1 フレームワークで特定の機能から発せられる場合がある。

「FW発生有」欄: 有=フレームワーク側に発生させる機能がある。
「ユーザ発生可」欄: ○=アプリ側から発生させることも許される。/  ✗=不可。

| 定数 | 値 | FW発生有 | ユーザ発生可 | 意味 | 典型的な受け方 |
|---|---|---|---|---|---|
| `TSE_RETURN` | 7 | 有 | ○ | 子 tinyState が結果を返した/役割を終えた場合に1度だけparentに対して送る。stdEvent::msg_...に戻り値。 | `R_TEST` / `if (ev->type==TSE_RETURN && ev->source==child)`（[COOKBOOK §3](COOKBOOK.md)） |
| `TSE_DESTROY` | 6 | 有 | ✗ | `listen(x, TSE_DESTROY)` した対象が破棄された | `if (ev->type==TSE_DESTROY)`（子の終了カウント等。[COOKBOOK §8](COOKBOOK.md)） |
| `TSE_TIMER` | 2 | 有 | ○ | `stdInterval::wait(ifThis, usec, TSE_TIMER)` の満了 | `if (ev->type==TSE_TIMER)`（[COOKBOOK §1](COOKBOOK.md)） |
| `TSE_TIMER2` / `TSE_TIMER3` | 3 / 4 | 有 | ○ | 追加のタイマチャンネル。同時に複数タイマを走らせて区別したいとき `wait` の第3引数に指定 | `if (ev->type==TSE_TIMER2)` |
| `TSE_WAKEUP` | 15 | 有 | ✗ | 自分のwakeup()インスタンスが呼び出された。 | child->wakeup() |
| `TSE_ASSERT` | 8 | 有 | ○ | 処理の途中経過報告。たとえば、接続確立（TCP connect 完了）の通知など | `if (ev->type==TSE_ASSERT)`（[COOKBOOK §2](COOKBOOK.md) の TCP 接続） |
| `TSE_SIGNAL` | 16 | 有 | ✗ | `tsSignal` が捕捉した OS シグナル stdEvent::msg_int がシグナル番号 | `if (ev->type==TSE_SIGNAL)`（[COOKBOOK §7](COOKBOOK.md)） |
| `TSE_STATE` | 5 | 有 | ✗ | 状態変化通知 | if ( ev->source == target && ev->type == TSE_STATE ) {targetの状態変化に伴う処理} |

### 1.2. フレームワークで特定の機能から発せられるわけではなく、アプリでも利用可

上４つはよく使います。下３つは定義されているだけ。基本的にこれらは、 アプリが独自に意味を与えて利用してもよい。

| 定数 | 値 | FW発生有 | ユーザ発生可 | 意味 | 典型的な受け方 |
|---|---|---|---|---|---|
| `TSE_UPDATED` / `2` / `3` | 10–12 | 有 | ○ | インスタンスが更新された。 | なにが更新されたかは発信元によって定義される。 |
| `TSE_ACCESS` | 19 | 無 | ○ | なにかがアクセスされた | なにかは発信元によって定義される。 |
| `TSE_PACKET` | 18 | 無 | ○ | パケット(stdEvent::msg_obj がパケットの内容) | — |
| `TSE_INVOKE` | 1 | 有 | ○ | 一般的な発火 | イベントに意味が薄く、状態遷移を強制的に実行させることに意味がある場合 |
| `TSE_REQUEST` | 9 | 無 | ○ | 要求 | — |
| `TSE_SUBMIT` | 13 | 無 | ○ | 投入 | — |
| `TSE_TRANSACTION` | 14 | 無 | ○ | トランザクション | — |

### 1.3 内部 / 低レベル

フレームワーク専用のイベントで、アプリで直接発生させることは禁止のイベント。場合によっては捕捉することも禁止。

| 定数 | 値 | FW発生有 | ユーザ発生可 | 意味 | 典型的な受け方 |
|---|---|---|---|---|---|
| `TSE_INIT` | 0 | 有 | ✗ | 初期状態(INI_START)発火 | INI_STARTの時だけ得られるイベントで捕捉してもそれ以上の意味がない。 |
| `TSE_THREAD` | 17 | 有 | ✗ | tsThread.cpp が使用 | アプリから使用も捕捉も禁止 |
| `TSE_PRIORITY` | 20 | (有) | ✗ | 実行優先度の変化 | アプリから使用も捕捉も禁止 (現在機能未実装) |

### 1.4 補助マクロ

- `TSE_EVM(x)` = `(1<<x)`：イベント種別のビットマスク化（`listen` のフィルタ等で種別集合を表す）
- `TSE_MAX` = 21（基本種別の総数）

> 旧 I/O サブイベント（`TSE_ATTACH`〜`TSE_READ_REMAIN`、上位ワード）・`TSE_SUBMASK`・`TSE_MASK`・
> `TSE_EMPTY_PROTO`/`TSE_INVALID_PROTO`・`TSE_CANCELED` は、旧 C 実装（arch の `ts2/c` ツリー）や
> rendez-vous 機構（いずれも撤去済み）専用で、C++ 現行コードから使われていなかったため
> コード（`stdEvent.h` ほか）とも削除した（2026-07-15）。イベント型は基本種別（0..`TSE_MAX`）
> のみになり、上位ワード/マスクは不要。C++ の read_c/write_c は `sException` yield 方式（§3 参照）。

---

## 2. ユーティリティクラス早見

早見表（[COOKBOOK](COOKBOOK.md)）で「✓ OK」のうち、アプリでよく使う非状態機械クラスの
主要 API。全メソッドは各ヘッダ `src/h/ts2/c++/<name>.h` を参照。すべて `thNEW` で生成し
`sPtr<T>` で持つ。

### 2.1 `stdString` — 文字列

最頻出。生成・連結・比較・変換・正規表現。

```cpp
sPtr<stdString> s = thNEW(stdString, ("hello"));
sPtr<stdString> n = thNEW(stdString, ("count=%d", 42));   // printf 風フォーマット
const char * c    = s->get_str();                          // C 文字列
INTEGER64    v    = n->get_int();                          // 数値化
s->cmp("hello");            // 比較（0 == 一致）。partial_cmp は前方一致
s->add(other);             // 連結（新 stdString を返す）。addIn は破壊的追記
s->to_upper(); s->to_lower();
s->percent_encode(); s->percent_decode();                  // URL %エンコード
// 正規表現（→ COOKBOOK §12.5）
s->rx("mv", re); s->rxVar(0); s->rxNext();
```

### 2.2 `sArray<T>` / `stdArray<T>` — 動的配列

`sArray` は非 stdObject の軽量テンプレート配列、`stdArray` は stdObject 派生。

```cpp
sPtr<stdArray<sPtr<Foo>>> a = thNEW(stdArray<sPtr<Foo>>, ());
a->push(x);        // 末尾追加
a->unshift(x);     // 先頭追加
a->length();       // 要素数（length(n) で伸縮）
```

> `sArray` は**コピー禁止**（コピーコンストラクタ・代入が `= delete`。浅いコピーによる
> dangling 防止）。`sPtr` で共有すること。

### 2.3 `stdBuffer` — バイト列バッファ

I/O 向けの可変バイト列。`copyin` / `copyout` / `shift`（先頭切り出し）等。

```cpp
int erp;
buf->copyin(&erp, ptr, data, len, arraySize);
buf->copyout(&erp, ptr, out, len);
sPtr<stdBuffer> head = buf->shift(&erp, &rest, &len);   // 先頭 len を切り出す
```

### 2.4 `stdBalancedTree<T>` / `stdStringTree` — 順序付き集合 / キー木

- `stdBalancedTree`：平衡木（順序付き集合）。`ins` / `del` / `count`。
- `stdStringTree`：文字列キーの木。`ins(key, obj, partial_flag)` / `search(key)` / `del(key)` /
  `match(...)`（**前方一致検索**が特徴。`partial_flag` で部分一致挿入）。

詳細 API はヘッダ参照。用途別の使い分け指針は `要ひさ確認`。

---

## 3. エラー処理：`sException` / `EX_*` / `ERR_` 状態

### 3.1 `sException`（コード由来）

`src/h/ts2/c++/sException.h`。コードは 2 種類のみ:

| コード | 値 | 意味 |
|---|---|---|
| `EX_STAY` | 0 | **yield して状態関数を先頭から再走**させたい（I/O 待ち・ロック待ち）。エラーではない |
| `EX_ERROR` | 1 | エラー |

`ts2IO::read_c` / `write_c`、`stdMutex::read_lock` / `write_lock` などは、まだ準備が
できていないとき `sException(EX_STAY)` を投げて yield する。だから**イベント検出と実 I/O
を分け、I/O は「1 状態 = 1 操作・ev 非依存」で書く**（[CLAUDE.md 鉄則 5](../CLAUDE.md) /
[GOTCHAS §7,§9](GOTCHAS.md)）。この再走モデルこそ tinyState の I/O 待ちの核心。

### 3.2 `ERR_` 状態カテゴリ

状態名の接頭辞 `ERR_` はカテゴリ `C_ERR`（`tinyState.h`、状態名先頭 3 文字がカテゴリ）に
対応する。`C_TEST(x->state(), C_ERR)` でエラー状態か判定できる。

### 3.3 個別クラスの `err` メンバ

ソケット系（`ts2IOsockServer` / `ts2IOsockUDP` 等）は、生成の成否を**イベントではなく
`err` メンバ**（int）で持つ（例: `server->err`、`server->errpos` に失敗箇所）。
準備完了 `TSE_WAKEUP` を受けた後に `err` をチェックする（[COOKBOOK §2](COOKBOOK.md)）。

> **`要ひさ確認`**：「アプリはエラーを *どの層で* 表現・処理すべきか」——`err` メンバ /
> `ERR_` 状態 / `sException(EX_ERROR)` の**設計上の使い分け指針**は、ここでは断定しない。
> 現状のコードから読める事実のみ記載。

---

## 4. デバッグ：状態遷移トレース

### 4.1 `trace_all`（コード由来）

状態遷移トレースの発火経路（`tinyState.cpp`）が読むのは **`tinyState::trace_all`**（静的
`const char *`、既定 `0`）。**非 null をセットするとトレースが有効**になり、`trace_msg` を
ともっなった状態遷移位置のプリントを出力する。

```cpp
// main などで（アプリ起動の早い段階で）:
tinyState::trace_all = "1";     // 非 null にしてトレース有効化
```

**`stdFrameWork::trace_all`** は、fwIO など、selectループ状態の待ち行
  列のトレースを有効にする。stdFrameWork::trace_bit とペアで使用する。
  trace_bit は、出力する情報の種類を表すビットである。

trace_bit::
FWTR_INTERVAL :  インターバルタイマ待ちtinyState
FWTR_READ : read 待ちtinyState
FWTR_WRITE : write 待ちtinyState
FWTR_ACTIVE : 発火したtinyState
FWTR_RWI : FWTR_READ|FWTR|WRITE|FWTR_INTERVAL
FWTR_ALL : FWTR_READ|FWTR|WRITE|FWTR_INTERVAL|FWTR_ACTIVE


### 4.2 その他の切り分け

- デッドロックっぽい → ロック順序（[CLAUDE.md 鉄則 3](../CLAUDE.md)）
- refcount が変 → `sPtr` 以外の smart pointer 混入を疑う（[CLAUDE.md 禁止リスト](../CLAUDE.md)）
- 状態が飛ばない/ハング → I/O をイベント分岐の中で呼んでいないか（[GOTCHAS §9](GOTCHAS.md)）

---

## 関連ドキュメント

- [CLAUDE.md](../CLAUDE.md) — 鉄則・禁止リスト
- [COOKBOOK.md](COOKBOOK.md) — したいこと→コード
- [MACROS.md](MACROS.md) — マクロ詳解
- [GOTCHAS.md](GOTCHAS.md) — 非自明な動作・ハマりどころ
