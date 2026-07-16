# tinyState — メンタルモデル

> v1 (2026-07-15) — ひさレビュー反映済み
>
> ソース / Issues: <https://github.com/globalbase-org/tinyState>

## 1. tinyState とは何か

<b>Write state machines. Forget threads.(WSM-FgT)</b>

プログラムの流れは、開始処理(construct)、実行、終了処理(destruct)、消
滅　という一連の流れでできている。オブジェクト指向であれば、一つのオブ
ジェクトは、この一連の流れで完結しないとならない。しかし、オブジェクト
外部からの様々なアクション（インスタンス関数の呼び出し、変数の変更等）
の中で、あってはならない状態を作り出すと、この４つの流れは崩れ、予期不
能な動作となる。そこでオブジェクトを状態遷移マシンとして定義し、外部か
らのアクションに対して＜未定義＞は存在しないようにコントロールできれば、
多くのバグを未然に防ぐことができる。それが、クラスtinyStateのグランド
コンセプトである。

tinyStateオブジェクト = ステートマシン１個 = コルーチン1個の協調的並行
プログラミングの考え方である。当初 tinyState v1では、スレッドは、状態
に対するアクションの挿入が時間的に任意であることから、このような状態遷
移を明確に定義できない機構ということで、利用を控えるべき機構と位置付け
ていた。スレッドは、とくに終了を安全に行うことが難しく、そこにバグの混
入を許すケースが多い。非同期で起こる様々なアクションに対して、状態遷移
を書くのが難しい。

しかし、計算規模の大きいアプリケーションでマルチスレッドが使えないこと
のデメリットの大きさから、tinyState v2 (本バージョン)ではマルチスレッ
ドの統合を真剣に考えた。

v2 では、

- 状態遷移 = yield から yield まで、短時間で遷移し、他のtinyStateオブ
ジェクトの実行を許さない状態遷移。
- スレッド = yield から yield まで、長時間で遷移し、他のtinyStateオブ
ジェクトの実行を許す状態遷移

という対比で、スレッドが定義される。しかし、状態遷移とスレッドは、遷移
中に他のtinyStateオブジェウトの実行が許されるかされないかという、シン
プルな意味比較の中で捉えられる。スレッドの始まりは、あえられた状態から
の遷移の始まりであり、スレッドの終わりは、他の状態への遷移の完了である。

スレッドの実行中は、状態が変わることはない＝アクションが入ってくること
はない、という非常に強い制約の立場にたてば、tinyStateでは、状態遷移の
延長としてスレッドを安全に利用することができる。つまり、<b>Write state
machines. Forget threads.</b>である。

ただしこの場合、スレッドの中で実際に実行できることは、オブジェクトの外
部との交渉はいっさいやってはならない。したがって、ここから、プログラマー
が注意をはらいつつ、制約を少しずつ解いていくことができる。たとえば、
sThreadMutex などでロックを利用しながら、スレッド実行中に他のオブジェ
クトに自分の内部の変数へアクセスを許可する、などである。

pthread等のスレッドの機能全般をすべて利用可能な状態でスレッドを利用し
始めることは、様々な並行プログラミングのバグを踏むことになる。WSM-FgT
から、注意深く制約を解除することによって、バグの発生を見通しよく抑える
ことができる。

tinyStateは「ライブラリ」というより「<b>プログラミングストラテジー</b>」
であった。`tinyState` クラス相当を書ければどの言語でも動く、というスタ
ンスで、過去には Perl 版・JavaScript 版も実装した (JavaScript 版は
`src/scripts2/` に現存)。その意味でtinyStateの意味論を理解すれば、だれ
でも、このストラテジーに乗っ取ったプログラミングができる。ところが、
c++版はtinyStateの実装に最も困難を要した。ある意味、tinyStateと最も相
性が悪い言語だった。そのため、tinyState v2 はライブラリとして共有可能
な版として公開しておくのが便利と考えられる。

## 2. 起源 (2000-)

- 作者のプログラムのポータビリティをアップするための覚書としての手元ラ
  イブラリとして2000ごろから書き溜めてきたものを整理したものである。
- 分散型地理情報システム <b>GLOBALBASE</b> の基幹として書かれた。
- 多数のサーバ間コネクションを「<b>状態が常にトレース可能な形で</b>」管理するためのニーズから
- `boost::asio` と同時代・同問題領域から独立進化（claude akiraによる）
- 作者: 森洋久 / プロジェクト: GLOBALBASE project

## 3. 哲学

- <b>状態遷移を確実にトレース可能であること > 性能</b>
- スレッドの存在を意識しないで書ける環境 (= スレッドの生成・終了・join 管理をフレームワークが吸収)
- マクロ + コード生成 (`tscpp2`) で C++ の型安全性を保ちつつ、状態遷移を named な値として encode
- 「ライブラリ」ではなく「プログラミングストラテジー」(言語非依存)

## 4. 核心メカニズム

### 4.1 大域 mtx でステートマシン整合性は自動 (TS_STATE 中)

- `tsApplication::mtx` で全ステートマシンの状態関数実行をシリアライズ
- どの worker thread が状態関数を実行しても、他のステートマシンの状態関数とは排他
- ステートマシン作者は自分のメンバ変数にロック無しでアクセス可
- TS_THREAD 中は mtx 一時 unlock、TS_STATE 中は mtx 保持

実装: `src/classes/ts2/c++/tinyState.cpp:558` `tinyState_::eventHandler` で `appMtxLock()` / `appMtxUnlock()` を切り替え。

### 4.2 gc_thread + ZOM 二段構えで graceful shutdown を強制

- sPtr の refcount=0 で<b>即座にデストラクタは走らない</b>
- ヒープ回収用の専用スレッド `stdObject::gc_thread` (`src/classes/ts2/c++/stdObject.cpp:141, 193`) が実行
- 「スタックトップ = あらゆる mutex が解放されている状態」で呼ぶことを保証
- → デストラクタ内でクリティカルセクションを書いてもデッドロックしない

tinyState 派生クラスはさらに二段:

1. refcount=0 → `refEvent()` (`tinyState.cpp:212`) が `ref_destroy_flag=1` + `TSE_INVOKE` 自己投入
2. 状態機械内で `is_destroyed()` が true を返すようになる (`tinyState.cpp:397`)
3. アプリ層は `is_destroyed()` を見て `FIN_START` へ graceful 遷移
4. FIN フェーズ完了 → ZOM (C_ZOM) 到達
5. gc_thread が delete

これにより全ステートマシンに「綺麗に終わる機会」が必ず与えられる。

### 4.3 状態遷移は return 値だけが正規ルート

- 状態関数の戻り値で次状態を表現
  - `return 0;` — yield (event 待ち)
  - `return rDO | NEXT_STATE;` — 即 NEXT_STATE を実行
  - `return NEXT_STATE;` — NEXT_STATE を次イベント時に実行
- <b>デストラクタからも、コールバックからも、<code>_state</code> 直書きは絶対 NG</b>
- これが「状態が常にトレース可能」の根拠

### 4.4 sCallSection + sException で I/O 待ちを「呼び出し中断」として表現

`ts2IOdescriptor::read()` などのラッパでは:

```cpp
case EAGAIN:
    io->read(sCallSection::key->caller(), fd);
    throw sException([this](sPtr<tinyState> caller) {
        return !io->is_read(caller);
    });
```

仕組み:

- `sCallSection` (`src/h/ts2/c++/sCallSection.h`) はスレッドローカルな「現在実行中の tinyState の連鎖」を持つ
- `io->read()` で「自分が読みたい fd」を登録
- `throw sException(...)` で <b>現在の状態関数実行を即座にキャンセル</b>
- `eventHandler` の catch (`tinyState.cpp:622`) が拾い、`EX_STAY` なら `state=0` (= yield)
- I/O が ready になったら `is_ready` lambda で再チェック、true なら再開

実装上の効果: <b>ユーザコードは普通に <code>read()</code> を呼んでるように書ける</b>。ブロックすべき所では throw で yield に化けてくれる。

### 4.5 FILO 風スタック + state_lock で再入抑制

- `eventHandler` の `state_lock` フラグ (`tinyState.cpp:571`): 既に実行中なら、新しいイベントをキューに繋いで戻るだけ
- これにより、同じ tinyState の状態関数が他スレッドから再入することは無い
- `sCallSection` は thread-key で持つので、現在のコールチェーンが追跡できる

## 5. ライフサイクル

```
  INI → ACT → FIN → ZOM
  └── 各フェーズに *_START 入口あり
```

- <b>INI</b>: 初期化フェーズ (`INI_START` から始まる)
- <b>ACT</b>: 通常動作フェーズ (`ACT_START`)
- <b>FIN</b>: 終了処理フェーズ (`FIN_START`、外部リソース解放、子完了待ち)
- <b>ZOM</b>: ゾンビ (この状態に入ると gc_thread が delete してよい)

各状態は短く、`return 0;` で待機、`return rDO|NEXT_STATE;` で進行。

## 6. 命名規約

クラス名の prefix で「メモリ上の置き場所」と「派生関係」が分かる暗黙ルール:

| Prefix パターン | 例 | 意味 |
|---|---|---|
| 小文字 1 文字 + CamelCase | `sPtr`, `sObject`, `sException`, `sTimer`, `sCallSection` | ヒープ上になくても良いクラス (スタック上 OK) |
| 小文字 2 文字以上 (`std`, `ts`, `fw` 等) + CamelCase | `stdObject`, `stdEvent`, `stdMutex`, `tsApplication`, `tsThread`, `fwIO` | `stdObject` 派生 = ヒープ上に置くべきクラス、`sPtr` の先となる |
| `co_` + CamelCase (ファイル名) | `co_tsThreadKill` | 特定クラスの<b>内部専用子クラス</b>。ライブラリユーザが直接使うことは想定していない |

特殊例:

- <b><code>stdMutex</code> / <code>stdSemaphore</code></b>: 「状態遷移機械での mutex/semaphore エミュレーション」。スレッドとは無関係。`sThreadMutex` (OS スレッド用) と混同しないこと。
- <b><code>stdEvent</code></b>: stdObject 派生なのでヒープ上 (`thNEW(stdEvent, ...)` で作る)
- <b><code>co_*</code></b>: 親クラスからのみ `thNEW` で起動される内部専用クラス。例: `co_tsThreadKill` は `tsThread` からしか起動しない。外部から直接使わないこと。

## 7. やらない決断

- <b>C++ 例外を投げ続けない</b>: `panic` で abort する系。`sException` だけが特殊用途で使われる (yield 表現)
- <b>C++20 coroutine の awaiter 抽象を入れない</b>: 状態は全て名前付きで見えるべき
- <b>boost に依存しない</b>
- <b><code>std::shared_ptr</code> と混在しない</b>: sPtr の refcount が割れる

## 8. 他フレームワークとの違い (要点)

- <b>vs C++20 coroutine</b>: tinyState は executor (worker pool) 込みで提供。状態は名前で外から見える
- <b>vs boost::asio</b>: 同時代姉妹的存在。asio は I/O 抽象 + 任意の実行モデル、tinyState は状態機械 + 自動スレッド管理
- <b>vs protothreads</b>: tinyState (2000) が早い。OO・分散指向で大規模
- <b>vs Erlang/Akka</b>: 思想は近い (lightweight process + 自動スケジューラ)。tinyState は C++ オブジェクト級の軽さ

詳細は [comparison.md](./comparison.md) (執筆中)。

## 9. 次のステップ

- 詳細 API: [MACROS.md](./MACROS.md)
- 実装パターン: [COOKBOOK.md](./COOKBOOK.md)
- フレームワーク内部: [ARCHITECTURE.md](./ARCHITECTURE.md)
- AI assistant 向けルール: [../CLAUDE.md](../CLAUDE.md)
- ビルド系: [BUILD_INTERNAL.md](./BUILD_INTERNAL.md)
- よくある疑問・非自明な動作: [GOTCHAS.md](./GOTCHAS.md)
