# CLAUDE.md — tinyState 作業手引き (AI assistant 向け)

> v0 草案 (2026-05-24)

このファイルは tinyState コードを足す AI assistant が「フレームワーク作者 (ひさ) が数年かけて潰してきた地雷を再発させない」ためのルール集です。新規 contributor (人間) も対象。

まず [docs/MENTAL_MODEL.md](docs/MENTAL_MODEL.md) を読んでから戻ってきてください。

## 鉄則

以下を守れば、tinyState のお作法の 8 割は守れる。

### 1. オブジェクト生成は thNEW、生 new を使わない

```cpp
sPtr<MyClass> p = thNEW(MyClass, (arg1, arg2));   // ✓
sPtr<MyClass> p(new MyClass(arg1, arg2));         // ✗ refcount が初期化されない
```

`thNEW` は `sPtr` の refcount 機構を正しくセットアップする。生 `new` だと、コンストラクタ内で `sPtr` を使う場合に refcount が割れる。

### 2. 状態遷移は state 関数の return 値だけが正規ルート

```cpp
TS_STATE(ACT_FOO) {
    if ( cond )
        return rDO | ACT_BAR;     // ✓ 即 ACT_BAR を実行
    return 0;                      // ✓ yield、event 待ち
}
```

- `_state = ACT_BAR;` 直書きは<b>いかなる場合でも NG</b>
- デストラクタからも、コールバックからも、`other->_state = ...` も NG
- これが「状態が常にトレース可能」の根拠

### 3. app-mutex を持ったまま other->eventHandler を呼ばない

特に <b>TS_THREAD 内で app-mutex を保持しているとき</b>にやりがち。`other->eventHandler` は内部で `tsApplication::mtx` を取りに行くので、デッドロックが発生する。

| 場所 | ロック状態 | 注意 |
|---|---|---|
| TS_STATE 内で app-mutex 取得 | mtx → app-mutex の順 | OK |
| TS_THREAD 内で app-mutex 取得 | app-mutex 単独 (mtx 解放中) | OK |
| TS_THREAD 内で app-mutex 保持 + `other->eventHandler` | app-mutex → mtx の逆順 | <b>デッドロック源</b> |

採用ルール: <b>app-mutex を持った状態で <code>other->eventHandler</code> を呼ばない方向に倒す</b>。

### 4. 多重継承は原則しない

<b>tinyState 派生クラス同士の多重継承は定義できない。</b> `CLASS_TINYSTATE(name, parent)` マクロがベースクラスをひとつしか受け付けない構造になっているため、コンパイル以前に記述不能。

tinyState 派生クラスと tinyState でないクラスの多重継承は構文上は可能だが、プログラムの見通しを悪くするため推奨しない。`stdObject` 派生同士であっても `virtual public stdObject` を使わないと `sPtr` の refcount が複数になりうる。

### 5. I/O (read_c / write_c) を event でガードした分岐の中で呼ばない

`ts2IO::read_c` / `write_c` は EAGAIN で sException を投げて yield し、状態関数を<b>先頭から再走</b>させる。その再走時 `ev` は I/O 準備イベントに変わっているため、`if (ev->type==TSE_X) { ...write... }` の中に I/O を置くと<b>分岐が成立せず I/O が永久に再開しない</b>(ハング)。さらに 1 状態で複数の write/read を呼ぶと、後段 yield の再走で前段が二重実行される。

```cpp
// ✗ event 分岐の中で write(再走で ev が変わり再開不能 / 複数 write は二重実行)
TS_STATE(ACT_DONE) {
    if ( ev->type==TSE_RETURN && ev->source==child ) {
        pipe->write_str(A, x); pipe->wend();      // ← 危険
        return rDO|ACT_NEXT;
    }
    return 0;
}
// ✓ イベント検出は遷移だけ。I/O は ev 非依存・1 状態 1 I/O の専用状態で
TS_STATE(ACT_DONE)  { if (ev->type==TSE_RETURN && ev->source==child) return rDO|ACT_W1; return 0; }
TS_STATE(ACT_W1)    { pipe->write_str(A, x); return rDO|ACT_W2; }   // ev を見ない・1 write
TS_STATE(ACT_W2)    { pipe->wend();          return rDO|ACT_NEXT; }
```

ルール: <b>イベント検出状態は検出して `rDO` 遷移するだけ。実 I/O は ev 非依存の専用状態で「1 状態 = read_c/write_record 1 回」</b>。yield 再走はその 1 回を呼び直すだけになり、`ts2IO` 側の sPicoState が安全に再開する。読み書きバッファはメンバにする(stack ローカルは再走で別アドレス化)。詳細・背景は [docs/GOTCHAS.md](docs/GOTCHAS.md) §7/§9。

## 禁止リスト

以下は <b>tinyState ストラテジーで書かれたコード内では使わない</b>。
ただし、これらを内部で使う managed library (例: OpenSSL の自前スレッド) を呼び出すのは OK。

| 禁止対象 | 代替 |
|---|---|
| `std::thread` / `pthread_create` 直接呼び出し | `TS_THREAD` 状態を定義 |
| `std::async` / `std::future` | `thNEW` + `TSE_RETURN` 待ち |
| `std::shared_ptr` / `std::unique_ptr` | `sPtr<T>` |
| 生 `mutex` / `std::mutex` | `sThreadMutex` (スレッド同期用) または `stdMutex` (状態遷移用) |
| `TS_STATE` 内での blocking I/O / 長ループ | `TS_THREAD` に移すか、 `ts2IO*` ラッパ経由で yield 化 |

## 推奨イディオム

### 状態関数冒頭の destroy チェック

```cpp
TS_STATE(ACT_SOMETHING) {
    if ( is_destroyed() )
        return rDO | FIN_START;
    // 通常処理
}
```

### FIN_START で graceful shutdown

子を起動したどこかで必ず `listen` を登録しておく。これを忘れると FIN_START が永久に `return 0` で止まる。

```cpp
TS_STATE(ACT_START) {
    child = thNEW(ChildClass, (ifThis, args));
    child->listen(ifThis, TSE_DESTROY);  // ← 忘れずに
    return ACT_WAIT_RETURN;
}
```

```cpp
TS_STATE(FIN_START) {
    if ( child && !C_TEST(child->state(), C_ZOM) )
        return 0;                    // 子がまだ生きている (非 null かつ非 ZOM)
    io->detach(ifThis);              // fwIO を使っていれば。省略可だがパフォーマンスが上がる
    timer.stop(ifThis);              // 同上
    return rDO | FIN_TINYSTATE_START;
}
```

### 子オブジェクトを起動して結果を待つ

通常のプログラムにおける「関数呼び出し」に相当するパターン。

```cpp
TS_STATE(ACT_START) {
    child = thNEW(ChildClass, (ifThis, args));
    return ACT_WAIT_RETURN;          // event 待ちへ
}

TS_STATE(ACT_WAIT_RETURN) {
    if ( ev->type != TSE_RETURN )
        return 0;
    if ( ev->source != child )
        return 0;
    // 結果を取り出して次へ
    return rDO | ACT_NEXT;
}
```

### 親なしで tinyState を作る

親を必要としない場合は `parent = application` として生成する。どの tinyState も `application->mtx` を参照できる必要があるため。

```cpp
thNEW(SomeClass, (application, args));
```

### 親より先に死ぬパターン (シーケンシャライズ)

`parent` を「前のジョブ」として持ち、前が死んでから自分が動き始める。複数の同一ジョブをシリアライズできる。

```cpp
TS_STATE(INI_START) {
    parent->listen(ifThis, TSE_DESTROY);   // 前のジョブの終了を待つ
    return rDO | INI_WAIT;
}

TS_STATE(INI_WAIT) {
    if ( !C_TEST(parent->state(), C_ZOM) )
        return 0;                          // まだ生きている
    return rDO | ACT_START;
}
```

### 長ループを他の tinyState に譲る (application->gc->exe)

`rDO` チェーンが繰り返す状態遷移ループ、または終わりまで長時間かかるループでは、途中で一度スタックを解放して他のステートマシンに実行機会を与える。

```cpp
TS_STATE(ACT_LOOP) {
    // ...処理...
    application->gc->exe(ifThis);    // 自分をキューに入れ直して一時退場
    return ACT_SOME_RET;
}

TS_STATE(ACT_SOME_RET) {
    R_TEST                           // TSE_RETURN を待つ
    // ...
    return rDO | ACT_LOOP;
}
```

`application->gc->exe(ifThis)` は「自分を gc キューに追加して今の実行を終える」。スタックに積まれた他の tinyState を先に動かしてから自分が再起動する。ただし何度も呼ぶとパフォーマンス劣化になるので、ループ N 回に 1 回など頻度を絞る工夫も検討する。

`eventHandler()` 呼び出しが再帰的になる場合も、直前に `application->gc->exe` を挟むとその時点でスタックがパージされ再帰デッドロックを回避できる。

### 派生クラスの INI/FIN 連鎖

INI は A → B → C 順、FIN は C → B → A 順 (逆順) で連鎖させる。
詳細とコード例は [docs/COOKBOOK.md セクション 12](docs/COOKBOOK.md) 参照。

## TS_STATE / TS_THREAD 使い分け

基本は <b>TS_STATE で書き続ける</b>。以下の場合に TS_THREAD へ。

- blocking I/O を使わざるを得ないとき
- パフォーマンス上の理由で長時間ループや、長時間返ってこないライブラリコールが必要なとき

「どこからが長時間か」はアプリケーション依存。開発中の仕様や開発者の判断で決まる。

| | TS_STATE | TS_THREAD |
|---|---|---|
| ロック | `mtx` 保持 | `mtx` 一時 unlock |
| 想定処理時間 | 短時間 (μs〜数 ms) | 長時間 (ms〜s) |
| blocking I/O | 禁止 | OK (むしろ想定) |
| 長ループ | 禁止 | OK |
| `other->eventHandler` 呼び出し時の注意 | mtx を内側で取り直すので無害 | 自分が外部ロックを持ってるかチェック必須 |

開発初期は TS_STATE で書いて、ボトルネックが分かったら該当状態を TS_THREAD に<b>マクロ名だけ変えて</b>昇格、というイテレーション可能。

## ifThis vs thThis 使い分け

- <b><code>ifThis</code></b>: インタフェースクラス (= 公開ヘッダで見えるクラス) の自己 sPtr。<b>他オブジェクトに自分を見せるとき</b>はこちら
  - 定義: `#define ifThis (this->ifp)`
- <b><code>thThis</code></b>: 実装クラス (`_` 付きの内部クラス) の自己 sPtr。<b>外には見せない</b>
  - 定義: `#define thThis (sPtr<typeof(*this)>(this))`
- `stdObject` 派生 (tinyState ではない) では `thThis` で自分を見せる

```cpp
// tinyState 派生クラス内で他オブジェクトに通知
other->eventHandler(thNEW(stdEvent, (TSE_RETURN, ifThis, ...)));  // ✓
// 内部実装クラスを外に漏らさない
other->eventHandler(thNEW(stdEvent, (TSE_RETURN, thThis, ...)));  // ✗ 内部漏洩
```

## sPtr 系の使い分け

| | 用途 |
|---|---|
| `sPtr<T>` | 参照カウント smart pointer (shared_ptr 相当) |
| `sRptr<T, P>` | T と P (T の派生) を同時に指す参照型。元 T が変わると追随。継承で型変換しつつ参照を持ちたい時 |
| `thNULL` | null sentinel。`a = thNULL` で sPtr 解放 |
| `a.is_notNull()` / `a.is_null()` | `(a != thNULL)` / `(a == thNULL)` と等価。null 判定。`if (a)` / `if (!a)` (explicit operator bool) でも可。旧 `a.is_unclear()` / `a.is_clear()` は deprecated (2026-07-13) |
| `a->is_destroyed()` | `a->destroy()` が呼ばれたか refcount=0 になったかの判定。ZOM 到達とは等価でない |

### is_destroyed() の正しい使い方

<b><code>is_destroyed()</code> は自分自身の状態遷移関数内で、自分が終了すべきかを判断するためのもの。</b>

```cpp
TS_STATE(ACT_SOMETHING) {
    if ( is_destroyed() )            // 自分自身への問いかけ
        return rDO | FIN_START;
    // 通常処理
}
```

<b>他の tinyState が死んでいるかを外から知りたい場合は <code>C_TEST</code> を使う。</b><code>a->is_destroyed()</code> を外部から呼んでも ZOM 到達を保証しない。

```cpp
// ✗ 外部から is_destroyed() で死亡確認しようとする (ハマりポイント)
if ( a->is_destroyed() ) { ... }

// ✓ 外部からは C_TEST(C_ZOM) で確認する
if ( C_TEST(a->state(), C_ZOM) ) { ... }
```

`a.is_notNull()` との使い分け:
- `a.is_notNull() == true` のとき `a->is_destroyed()` や `a->state()` は `->` 実行不可 (null 参照)
- 順序: まず `a.is_notNull()` で null チェック → 非 null なら `C_TEST(a->state(), C_ZOM)` で終了確認

## コンストラクタ / デストラクタ

### コンストラクタ

- 引数のセットアップのみに留める
- 各種初期化は `INI_START` 内で行う (現在のお作法)
- 引数セットアップ用マクロは `tscpp2 new` が自動生成

過去はコンストラクタで初期化してたが、現在は INI_START 主体。

### デストラクタ

- 何でも書いてよい (sPtr 解放、mutex 取得、`other->eventHandler` 経由通知 — 全部 OK)
- 理由: gc_thread はスタックトップで走るので、デッドロックの心配無し
- <b>唯一の制限: <code>_state</code> を直接触ること (= 状態いじり) は絶対 NG</b>

## マクロ覚え書き

詳細は [docs/MACROS.md](docs/MACROS.md)。最頻出は:

- `TS_STATE(NAME) { ... }` — 状態関数定義
- `TS_THREAD(NAME) { ... }` — ワーカ専有状態関数 (mtx 外れる)
- `TS_BEGIN_IMPLEMENT` / `TS_END_IMPLEMENT` — 内部クラス宣言ブロック (`#if 0` ガード内)
- `thNEW(Class, (args))` — 唯一の正規 new
- `thNULL` — sPtr の null
- `ifThis` / `thThis` — 自己 sPtr
- `is_notNull()` / `is_destroyed()`
- `rDO`, `C_TEST`, `C_ZOM`, `C_FIN`, `C_THR` 等の状態ビット
- `R_TEST` — `if ( ev->type != TSE_RETURN ) return 0;` の短縮形
- `R_SET(x)` — `R_TEST` + `ev->msg_obj` を x に d_cast して代入
- `R_REF` / `S_SET` — v1 互換マクロ、新規コードでは使わない (廃棄予定)

## ファイル構造の覚え書き

- `co_*` ファイル (例: `co_tsThreadKill.cpp`) は特定クラスの内部専用子クラス。ライブラリユーザが直接 `thNEW` してはいけない
- `foo.cpp` → tscpp2 が以下を生成:
  - `build/_ts2/c++/foo_.h` (foo.cpp からのみ参照される実装情報)
  - `build/_ts2/c++/foo_pb.h` (公開インタフェース情報)
- 公開ヘッダ `src/h/<dom>/c++/foo.h` は `_ts2/c++/foo_pb.h` を include
- `TS_BEGIN_IMPLEMENT ... TS_END_IMPLEMENT` ブロックは `#if 0` で囲うのが現在の方法 (`/* */` はネスト不可で断念)

## 廃止予定 / メンテ必要 (2026-05 現在)

このリストにあるクラスを「便利そう」と再利用しないこと。新規実装は別系統で。

- <b>削除済み (2026-07-11)</b>: 未使用の古いコードを削除 — CURL/OpenSSL 群 (`tsCURL`/`stdCURL`/`co_tsCURLmulti`/`co_tsCURLcleanupHelper`/`stdSSL`/`stdEVP`/`stdOpenSSL`(arch))、`stdQue`(C++版)、`tsFindFiles`、`src/gt2` ツリー全部、`tsInsensitive`(event debounce。`send()` がどこからも呼ばれず tsApplication が生成/破棄するだけの dead weight だった)、**レガシー C ツリー全部** (`src/classes/ts2/c` + `src/h/ts2/c`、~54ファイル・cmake 非ビルド。C++ が唯一依存していた `ts2/c/ts_types.h` は `ts2/c++/ts_types.h` へ繰り込み済み)。必要になれば改めて実装する
- <b>刷新済み (2026-07-11)</b>: `stdMutex` — read_lock/write_lock を「state 引数を取りその state を返す」旧方式から sException ベースの yield へ書き換え(fwIO の唯一の利用 pinWait を削除 → `example/rwlock-demo` で実証)。要メンテ解消
- <b>リネーム完了 (2026-07-11)</b>: `ts2DevNull` → `ts2IOdevNull`(cpp/h/クラス名/CLASS_TINYSTATE/ts2System の参照すべて更新)
- <b>整理完了 (2026-07-15)</b>: 旧 make ビルド系を一掃し、cmake を唯一のビルド系として root へ昇格。`proj/AVR`/`proj/cygwin`/`proj/posix`/`proj/mk`、`image/`、`submodule/make.project`、未使用 arch 層(`AVR`/`posix_Darwin_iOS*`/`default`/`100_old`/`200_*`)を削除。`proj/cmake/*` は `./CMakeLists.txt` + `./cmake/*.cmake` + `./{ts2,math2}` + `./utils` へ移動。`src/classes/ts2/c` に残っていた make overlay の stray symlink も除去。ビルドは root で `cmake -B build .`
- <b>削除済み (2026-07-15)</b>: 2026-07-11 の src/ レガシー C ツリー掃除で**取り残されていた arch/ 側の C ツリー**を削除 (`arch/posix/src/classes/ts2/c/{daemon,fwIO,tsIOraw,tsIOstring}.c` + `arch/posix/src/h/ts2/c/fwIO.h` + `arch/posix_Darwin/src/classes/ts2/c/daemon.c`。cmake 非ビルド・C++ 非依存)。併せて旧 C 実装専用で C++ 化以降未使用だった I/O サブイベント (`TSE_ATTACH`〜`TSE_READ_REMAIN`)・`TSE_SUBMASK`・`TSE_MASK`(submask 撤去で `search_listen` の `type &= TSE_MASK` が no-op 化)・未使用の `TSE_EMPTY_PROTO`/`TSE_INVALID_PROTO` を `stdEvent.h` から削除 (`TSE_EVM` は listen フィルタで使用中のため温存)
- <b>削除済み (2026-07-15)</b>: **rendez-vous 機構を全撤去**。enqueue 経路 `rendez_vous(ev)` に呼び出しが一切なく機構全体が空回り(rv_que 常に空)だったため、`rendez_vous`/`_rendez_vous`/`clear_rendez_vous`・`TS_RENDEZ_VOUS` マクロ(公開ヘッダ tinyState.h の予約 API だった)・メンバ `rv_que`/`rv_event`・および連鎖で未使用化した `TSE_CANCELED` を削除。tinyState.cpp/h 内で完結。full build + unixtest 実機 lifecycle green

詳細な ok / 要メンテ / 廃棄 の全クラス分類は [docs/COOKBOOK.md](docs/COOKBOOK.md) の早見表参照。

## 困ったとき

- 状態が思った通りに遷移しない → `tinyState::trace_all` を非 null にしてログを見る（遷移トレースを駆動するのはこの静的。`stdFrameWork::trace_all` は別変数で現状トレース経路から読まれていない ← 要ひさ確認。詳細 [docs/REFERENCE.md](docs/REFERENCE.md) §4）
- デッドロックっぽい → ロック順序を確認 (上記「鉄則 3」参照)
- refcount が変な事になる → `sPtr` でない smart pointer が混ざってないか確認 (上記「禁止リスト」参照)
- イベント型 (`TSE_*`) / ユーティリティクラス / エラー処理の索引 → [docs/REFERENCE.md](docs/REFERENCE.md)
- まず迷ったら [docs/COOKBOOK.md](docs/COOKBOOK.md) のパターン一覧

## Open issues

- [x] `tscpp2 new` が生成する引数セットアップマクロの正式名称・使い方 → [docs/MACROS.md](docs/MACROS.md) §2 (`TS_DEFARGS` / `TS_ARGS<N>` / `TS_CPARGS<N>`) に記載済み (2026-07-14)

「初心者がよく書く間違いコード」は会話・レビューを通じて随時このファイルに追記していく。現時点での既出例:
- `child->listen(ifThis, TSE_DESTROY)` を忘れて FIN_START が永久 yield (→ FIN_START セクション参照)
- 外部から `a->is_destroyed()` で死亡確認しようとする。正しくは `C_TEST(a->state(), C_ZOM)` (→ sPtr セクション参照)
