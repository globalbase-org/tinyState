# tinyState — よくある疑問・非自明な動作

> このドキュメントは「これはバグではないか？」「なぜこう動くのか？」という疑問に対して、
> コードを追って確認した結果を記録したもの。
> 実装を変えた記録ではなく、<b>もともとそう設計されている</b> 事実のリスト。

<hr>

## 1. destroy() を呼ぶと SIGPIPE が飛ぶ

### 症状

`child->destroy()` を呼んだ後、プロセスが exit code 141 (= 128+SIGPIPE) で終了する。

### 原因

`tinyState_::destroy()` (`tinyState.cpp:379`) は `THR_KILL(SIGPIPE)` を呼ぶ。
これは <b>意図的な設計</b>。TS_THREAD ワーカーがブロッキング I/O で止まっている場合、
SIGPIPE を送って中断させる必要がある。

```
tinyState_::destroy()
  → thrKill(SIGPIPE)     // tinyState.cpp:544
    → pthread_kill(worker_thread, SIGPIPE)  ← TS_THREAD が動いていれば実行
```

### 対処

TS_THREAD を使うクラスを持つアプリは、SIGPIPE を吸収する必要がある。
<b><code>SIG_IGN</code> は使わない</b>（tsSignalCore の pipe_write への書き込みが EPIPE を返すと
SIGINT イベントが失われる副作用がある）。

推奨パターン（`tsSignal` でフレームワーク内に収める）:

```cpp
// hwMain::INI_START
sig_pipe = thNEW(tsSignal, (ifThis, SIGPIPE));

// hwMain::FIN_START
sig_pipe->destroy();
```

または main.cpp で no-op ハンドラ（シンプルな場合）:

```cpp
::signal(SIGPIPE, [](int){});   // SIG_IGN は不可
```

---

## 2. TS_THREAD の refio はラウンド間に 0 にならない

### 疑問

`TS_THREAD(ACT_COMPUTE)` が `rDO|ACT_REPORT` を返し、
`TS_STATE(ACT_REPORT)` が `rDO|ACT_COMPUTE` を返す、というループを繰り返す場合、

```
ACT_COMPUTE 終了
  → delRefio() ← ここで refio==0 になる？
  → ACT_REPORT 実行
  → addRefio() ← refio を 1 に戻す
```

という窓が存在し、fwIO が終了条件を満たして抜けてしまうのではないか？

### 答え：窓は存在しない

`rDO` チェーンは <b>単一の <code>eventHandler</code> 呼び出しの中</b>で完結する。

```
tsThread_::__tsThread_body():
  eventHandler(TSE_THREAD, target) を呼ぶ
    ↓ eventHandler 内の inner for(;;) ループ:
      ACT_COMPUTE (TS_THREAD) — thrInfo 非 null → 直接実行
        return rDO|ACT_REPORT → strip rDO → continue
      ACT_REPORT (TS_STATE) — app mutex 保持で実行
        return rDO|ACT_COMPUTE → strip rDO → continue
      ACT_COMPUTE (TS_THREAD) — thrInfo まだ非 null → 直接実行
        ... (ループ)
      is_destroyed() → FIN_START → ... → ZOM → break
    eventHandler 返る
  run->del(target)
  if (run->count == 0 && ready->count == 0):
    resRefio()   ← delRefio() がここで初めて呼ばれる
```

`resRefio()` / `delRefio()` は `tsThread.cpp:561-566` で `eventHandler` が返った後にのみ評価される。
`refio` は `ins()` 呼び出し時（最初の INI_START 時）から ZOM 到達まで <b>常に 1</b>。

実装根拠:
- `thrInfo` は `eventHandler` 開始時 (`tinyState.cpp:574`) に設定され、返るまで非 null
- TS_THREAD 状態 + `thrInfo` 非 null → `application->getThread()->ins()` は呼ばれず、
  worker thread 上で直接実行 (`tinyState.cpp:613-619`)
- `ready` キューへの追加はないため、`ready->count == 0` のまま

### 結論

TS_THREAD を使う子ステートマシンが生きている限り `refio ≥ 1` が保証される。
fwIO は当該ステートマシンが ZOM に到達するまで終了しない。
keepalive timer や追加の fwIO 保持機構は不要。

---

## 3. リストを destroy() で畳むときは next を先に確保する

### 症状（罠）

tinyState を連結リスト化した場合、連結リストの全ノードを `destroy()`
で一括破棄しようとして

```cpp
for ( c = head ; c ; c = c->next )
    c->destroy();
```

と書くと、<b>1 ノード目しか destroy されない場合がある。</b>（カウンタが
0 まで落ちず、リストオーナーが終了待ちのままハングする等）。

### 原因

`destroy()` は対象 tinyState に `TSE_INVOKE` を送るが、`eventHandler` は
<b>送り先を main loop 非依存で即座に同期実行する</b>（§MENTAL_MODEL 4.5。既に実行中
でなければその場で状態関数を回す）。対象は `is_destroyed()` を見てその場で
FIN → ZOM まで走り切り、<b>FIN の中で自分のインスタンス変数（<code>c->next</code> 等の
リストリンク）をクリアして死ぬ</b>。

従って `c->destroy()` が返った時点で `c` のリンクは無意味（典型的には 0 に
クリア済み）。`c = c->next` は誤ったノード（多くは null）を返し、ループが
1 周で終了する。「`destroy()` は後で非同期に効く」と思い込むと踏む。

### 対処

次ノードを <b><code>destroy()</code> を呼ぶ前に退避する</b>（リンクリスト削除の鉄則）:

```cpp
sPtr<MyNode> c;
sPtr<MyNode> c_next;
for ( c = head ; c ; c = c_next ) {
    c_next = c->next;   // ← destroy() の前に退避
    c->destroy();
}
```

複数のステートマシン/コルーチンをリスト管理して一括破棄する場合も同じ。
`destroy()` が同期実行で自己リンクを壊す、という前提を忘れない。

### 実例

sarExchanger ver.1 での事例。`sarMeasurement` 停止時に、ack 待ちの
`co_sarMeasurement` を `coo_` リスト walk で `destroy()` していくコードで、
next 未退避だったため 1 個しか畳めず、`co_count` が 0 に落ちず measurement が
ZOM できずハングした。next 先取りで全ノード破棄され解消。

---

## 4. tscpp2 はコメント内の <code>TS_STATE(...)</code> / <code>TS_THREAD(...)</code> も状態として拾う

### 症状

ソースのコメントに `TS_THREAD(ACT_START)` のような文字列を書いていると、
ビルド時に生成ヘッダ (`_.h`) で当該状態が <b>二重生成</b>され、
`error: redefinition of 'TS_TRANS ...::trans_ACT_START'`、
`cannot be overloaded with ...state_ACT_START` 等が出る。

### 原因

tscpp2 の状態スキャンは <b>コメントを除去せずに</b> `TS_STATE(` / `TS_THREAD(`
というトークンを探す。説明コメント中の `TS_THREAD(ACT_START)` を実際の状態定義と
区別できず、本物の `TS_STATE(ACT_START)` 定義と合わせて 2 回登録してしまう。

実例として、状態関数の直前コメントや、ファイル頭の設計コメントに
「派生が `TS_THREAD(ACT_START)` で上書き」のように書くと踏む。

### 対処

コメントに `TS_STATE(...)` / `TS_THREAD(...)` の <b>リテラル形を書かない</b>。
「ACT スレッドで上書き」「ACT_START を派生が override」等に言い換える
(状態名そのものは可。`(` を付けたマクロ形が禁物)。

> 恒久対処は §5 と共通(tscpp2 のコメント認識)。

---

## 5. IMPLEMENT ブロック内の宣言に付けたインラインコメントの括弧で codegen が壊れる

### 症状

`#if 0 ... TS_BEGIN_IMPLEMENT ... TS_END_IMPLEMENT` 内のメンバ/メソッド宣言に
インラインコメントを付け、その中に <b>丸括弧</b>が含まれると、
ビルド時に `error: unterminated comment` が出て、続いて
`invalid use of incomplete type` 等が連鎖する。

```cpp
// 壊れる例 (コメント内に "record(rec_*" の括弧)
int next_record();   /* 1=record(rec_* 充填), 0=W_END, -1=エラー */
// OK 例 (括弧なし)
void write_d_meta(const uint8_t *data, int size);   /* INIT gate で派生が書く */
```

### 原因

tscpp2 は IMPLEMENT ブロックの宣言を生成ヘッダの `TS_IMPLEMENT` マクロ
(行末 `\` 継続のマクロ本体) へ転記する際、コメントを丸括弧の位置で切ってしまう。
生成 `_.h` 側に `/* 1=  \`(終端 `*/` を欠いた) 断片が残り、以降がコメント扱いに
なってクラス本体定義が消失 → incomplete type の連鎖になる。
括弧を含まないコメントは無事。

### 対処

IMPLEMENT ブロックの宣言には <b>インラインコメントを付けない</b>(特に `(` を含むもの)。
戻り値やセマンティクスの説明は、ブロック外の関数 <b>定義側</b>のヘッダコメントに書く。

> <b>恒久対処案</b>: tscpp2 を「コメント認識」に改造して §4 / §5 の根本原因
> (コメント未処理)を恒久対処する。直れば本節の回避策は不要になる。

---

## 6. スケジューラ(ワーカスレッド)内から ::exit() を呼ぶと race で落ちる

### 症状

状態関数の中で `::exit(0)` 等を呼ぶと、`stdInterval::now()` 内の
`sThreadMutexHandle __hdr(m)` で仮想呼び出しが `0x0` になり SIGSEGV、など、
<b>自分とは無関係なフレームワーク内部</b>でクラッシュする。再現は決定的。

### 原因

子ステートマシンの FIN が `parent->eventHandler(TSE_RETURN, ...)` を <b>同期実行</b>する
(§MENTAL_MODEL)。TS_THREAD を持つ子の場合、この同期チェーンは <b>ワーカスレッド上</b>で
走る。そこで `::exit()` を呼ぶと、main スレッドが `fwIO::loop` を回したまま
グローバル static (`stdObject::refCond` / `refMtx` / `stdInterval::m` 等) の
静的デストラクタが走り、使用中の static を破壊して落ちる(終了 race)。

### 対処

スケジューラ内から `::exit()` で自爆しない。<b>FIN へ抜けてアプリのアイドル終了に任せる</b>
(全 state machine が ZOM に達すると `fwIO::loop` が返り、`tsApplication` の
コンストラクタが戻る)。プロセス終了コードが必要なら、グローバル変数に結果を残し、
`main()` が `tsApplication` 構築から戻った後に `return` する:

```cpp
int g_exitCode = 0;   // 状態関数が FAIL 時に 1 をセット
int main() {
    thNEW(tsApplication, (thNULL, [](sPtr<tsApplication> app){
        thNEW(MyTest, (app));        // FIN まで走り切るとアプリがアイドル終了
    }));
    return g_exitCode;               // ← ループ終了後に評価される
}
```

### 実例

cgal-processor step4 の `ptsWireStreamTest`(キャッシュ往復テスト)。
並行フェーズの判定状態が reader の TSE_RETURN 同期チェーン(reader のワーカスレッド)上で
走っており、そこで `::exit(0/1)` していたため、main スレッドの `stdInterval::now()` が
破壊済み static を触って毎回 SIGSEGV。FIN 抜け + グローバル終了コード方式で解消。

---

## 7. インスタンスメソッドで sException を投げる呼び出しを 2 回以上使うなら sPicoState

### 症状

状態関数から呼ばれるインスタンスメソッド(状態関数そのものではない)が、
`ts2IO::read_c` / `write_c` のような **EAGAIN で sException を投げて yield する呼び出し**を
1 メソッド内で 2 回以上使うと、2 回目の yield 再入で 1 回目が再実行され副作用が二重になる
(例: レコードのヘッダを書いた後にペイロード write で yield → 再入でヘッダ二重書き)。

### 原因

sException の yield は「<b>状態関数を先頭から再実行</b>」する(その場 resume ではない。§MENTAL_MODEL)。
これは状態関数だけでなく、そこから呼ばれるインスタンスメソッドにも波及する。メソッド内に
yield 点が複数あると、後段 yield のたびにメソッド先頭(= 前段の副作用)からやり直す。

### 対処

`sPicoState.h` でメソッドを分節する。各 yield 点を別の `PS_STATE`(psINI/psDO/...)に置き、
`__state` を進めることで再入時は完了済みの段を飛ばす。永続バッファ(write_c/read_c が保持する
bp の指す先)は <b>pico_state 構造体のメンバ</b>にする(stack ローカルは再入で別アドレス化する)。

```cpp
struct { PS_PRESET uint8_t hbuf[8]; } ps_write_record;   // メンバ。PS_PRESET=__state/__recursive
...
void Foo_::write_record(uint16_t type, uint16_t flags, const uint8_t *payload, uint32_t len) {
  PS_STATEMENT(ps_write_record, PS_DEF(hbuf),
    PS_STATE(psINI)              // ヘッダ: yield 再入で再開、二重書きしない
      wire_put_rechdr(hbuf, type, flags, len);
      io->write_c(hbuf, 8);
    PS_STATE(psDO)               // ペイロード
      if ( payload && len ) io->write_c((void*)payload, (int)len);
      PS_RETURN()
  );
}
```

コンストラクタで `ps_write_record.__state = psINI; ps_write_record.__recursive = 0;` を初期化する。
`ts2IO::read_c/write_c` 自身もこの sPicoState 流。

### 補足: yield をまたぐ read/write バッファはメンバにする

`read_c(buf,len)` の `buf` を stack ローカルにすると yield 再入で別アドレスになり、read_c が
ps_*_c に保持した bp が無効ポインタ化する。状態関数で read_c が 1 回でも、読み先は必ずメンバに。
さらに <b>1 状態 = read_c 1 回</b>(複数 read を 1 状態に書くと後段 yield の再入で前段が再走し別バイトを食う)。

### 実例

cgal-processor `ptsWirePipe::write_record`(ヘッダ/ペイロード分割)、`ptsWireCacheStreamReader`
(rhdr/rpayload をメンバ化、ACT_HDR / ACT_PAYLOAD で read_c を 1 回ずつ)。

---

## 8. sPtr / sWptr のアップキャストに d_cast は不要

### 疑問

`ifThis`(sWptr<派生>)や `thNEW(...)` の返り(sPtr<派生>)を、親引数など `sPtr<基底>` へ
渡すとき `sPtr<基底>::d_cast(...)` が必要か?

### 答え: 不要(アップキャストは暗黙変換)

`d_cast` は dynamic_cast で <b>ダウンキャスト</b>(基底→派生)用。アップキャスト(派生→基底)は
`sWptr` / `sPtr` が `is_base_of` 制約付きの暗黙変換を持つので素の代入で通る。
アップキャストに d_cast を使うと無駄に RTTI を引く。

```cpp
// sWptr.h: sWptr<派生> → sPtr<基底>(静的アップキャスト)
template<class __TYPE2> requires std::is_base_of<__TYPE2, __TYPE>::value
operator sPtr<__TYPE2>() { return sPtr<__TYPE2>(ptr); }
// sPtr.h: sPtr<派生> → sPtr<基底>
template<class __TYPE2> requires std::is_base_of<__TYPE, __TYPE2>::value
sPtr(const sPtr<__TYPE2> inp) { _initial(inp.__get()); }
```

```cpp
sPtr<tinyState> self = ifThis;                   // OK (sWptr<派生> → sPtr<tinyState>)
io0 = thNEW(ts2IOdescriptor, (self, fd));         // OK (sPtr<ts2IO> ← sPtr<ts2IOdescriptor>)
// 不要:  sPtr<tinyState> self = sPtr<tinyState>::d_cast(ifThis);
```

`d_cast` が要るのは本当に <b>ダウンキャスト</b>するとき(`sPtr<派生>::d_cast(基底ハンドル)`、
失敗で null が返る)。例: `ev->source`(sPtr<tinyState>) を具体型ハンドルとして受けたい場合。

---

## 9. I/O (read_c / write_c) を event でガードした分岐の中で呼ばない

### 症状

子の結果やハンドシェイク(`TSE_RETURN` / `TSE_ASSERT` / `TSE_PACKET`)を受けた分岐の中で
`write_c` / `read_c`(やそれを包む write_str/write_record/read 系)を呼ぶと、データが小さいうちは
動くが、EAGAIN で yield が起きた瞬間に<b>処理が止まる(ハング)</b>、または同じレコードが二重に出る。

### 原因

§7 の通り I/O は sException で yield し、状態関数を<b>先頭から再走</b>する。その再走時、`ev` は
もはや元のイベントではなく <b>I/O 準備イベント</b>に変わっている。よって

```cpp
TS_STATE(ACT_DONE) {
    if ( ev->type==TSE_RETURN && ev->source==child ) {   // ← 再走時はここが false
        pipe->write_str(A, x);                            //    → 二度と実行されず write 未完了
        pipe->wend();                                     // ← さらに 1 状態で 2 つ目の write:
        return rDO|ACT_NEXT;                              //    wend が yield すると先頭再走で
    }                                                     //    write_str(A) が二重実行される
    return 0;
}
```

「event 分岐内で I/O」+「1 状態で複数 I/O」の二重の罠。小メッセージで write_c が yield しない間は
顕在化せず、大きいデータや背圧で初めて踏む。

### 対処

<b>イベント検出状態は「検出して `rDO` 遷移」だけにし、実 I/O は ev 非依存の専用状態で
「1 状態 = read_c / write_record 1 回」</b>にする。`ts2IO` の各 I/O ステートが read_c を `if(ev...)` で
囲まず直接呼んでいるのが手本。

```cpp
TS_STATE(ACT_DONE) { if (ev->type==TSE_RETURN && ev->source==child) return rDO|ACT_W1; return 0; }
TS_STATE(ACT_W1)   { pipe->write_str(A, x); return rDO|ACT_W2; }   // ev を見ない・1 write
TS_STATE(ACT_W2)   { pipe->wend();          return rDO|ACT_NEXT; } // yield 再走は write_record の pico が再開
```

こうすると I/O ステートの再走は「同じ 1 回の I/O を呼び直す」だけになり、write_record/read_c 内部の
sPicoState(§7)が安全に再開する。送信失敗(相手が閉じた等)の検出は、後続の read 状態が受ける
`TSE_RETURN`(pipe close)で行えばよい(write 状態自体では見ない)。

### 実例

cgal-processor `pigfAgent`(ACT_HELLO は TSE_ASSERT を検出して SENDOP へ遷移するだけ。C_OP / C_ARG_END /
wend は ev 非依存の SENDOP / SENDEND / SENDWEND で 1 回ずつ送る)、`ptsAgentStub`(WRITING 検出 →
SAVEBEGIN/SAVEDONE/SAVEBYE/SAVEWEND)。

---

## 10. 単一の sPicoState を複数 caller が共有する関数の不可分化(count=1 semaphore + holder)

### 症状

ある関数(例 `write_record`)が §7 の sPicoState で書かれ、その pico 構造体が<b>オブジェクトの
メンバ(= 単一)</b>で、その関数が<b>複数の caller(例 ts2Parallel の複数ワーカー)から並行に
呼ばれる</b>場合、A が pico の途中(例 psDO)で yield 中に B が同じ関数に入ると、B は
共有 `__state`(=psDO)を読んで<b>途中状態に飛び込み</b>、ヘッダを書かずにペイロードを書く等で
ストリームが壊れる。

### 原因

sPicoState の `__state` は単一メンバ。§7 は「1 つの caller が yield を跨いで再開する」前提で、
複数 caller が同じ pico を共有することは想定していない。`__recursive` ガードは同期的な再帰は
弾くが、yield(sException で巻き戻り、`__sFlag` dtor が `__recursive` を 0 に戻す)後に別 caller が
入るケースは弾けない。

### 対処

count=1 の `stdSemaphore` で関数全体を直列化する。ただし<b>取得(get)は pico の switch より前</b>で
行う(pico の psINI 内で get すると、上記の「途中状態に飛び込む」B が get を素通りしてしまう)。
自分自身の yield 再入で再取得して<b>自己デッドロック</b>しないよう、保持中 caller を
`holder`(= `sCallSection::key->caller()`)で覚えて再入は素通りさせる:

```cpp
void Foo_::write_record(...) {
    sPtr<tinyState> me = sCallSection::key->caller();
    if ( holder != me ) { lock->get(); holder = me; }   // ★get は PS_STATEMENT より前
    PS_STATEMENT(ps_write_record, PS_DEF(hbuf),
        PS_STATE(psINI)  ... ヘッダ write_c ...
        PS_STATE(psDO)   ... ペイロード write_c ...
        PS_STATE(psDO2)  holder = thNULL; lock->release(); PS_RETURN()   // 解放は最終 pico 状態で
    );
}
```

状態を 1 つ増やす(`enum { psDO2 = psDO+1 };`)。`stdSemaphore::get()` は count==0 で sException yield、
`release()` で待ち caller を起こす(§COOKBOOK 6.5)。

### 実例

cgal-processor `ptsWirePipe::write_record`(複数の引数送信ワーカーが同じ pipe の write_record を
並行に呼ぶため、wlock + wrHolder で 1 レコード単位を不可分化)。

---

## 11. 派生 tinyState クラスは、状態を足さなくても基底の sPtr<不完全型> メンバの完全型 include が要る

### 症状

`CLASS_TINYSTATE(Child, Parent)` で既存クラスを派生し(状態関数は 1 つも足さず親の状態機械を
そのまま継承)、`agent_cmd()` のような虚関数だけ override したら、`Child.cpp` のコンパイルで
`error: invalid use of incomplete type 'class X'`(X = 基底の sPtr メンバの型)が多数出る。

### 原因

codegen 自体は通る(状態ゼロの派生も可)。だが基底 `Parent` が `sPtr<X> a;`(X は前方宣言のみ)の
ようなメンバを持つと、派生クラスのデストラクタ/コンストラクタ実体化の時点で `sPtr<X>` の
デストラクタが X の完全型を要求する。基底の .cpp はそれらを include しているが、派生の .cpp は
別 TU なので自前で include しない限り不完全型のまま。

### 対処

派生の .cpp に、<b>基底が持つ `sPtr<不完全型>` メンバの完全型ヘッダを include</b>する。

```cpp
// pigfCgalpAgent.cpp(基底 pigfAgent の sPtr メンバを完全型に)
#include "ts2/c++/ts2System.h"
#include "ts2/c++/ts2Parallel.h"
#include "ts2/c++/ts2IO.h"
#include "pig/c++/ptsWirePipe.h"
#include "pig/c++/ptsWireCacheStreamReaderText.h"
```

### 実例

cgal-processor `pigfCgalpAgent`(基底 `pigfAgent` の ts2System / ptsWirePipe / ts2Parallel /
reader / ts2IO メンバ用に完全型 include)。
