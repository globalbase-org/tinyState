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

---

## 12. 静的ライブラリは未定義参照を隠す — 「使われないから気付かない」バグ

### 症状

`.a` を配ってきた間は誰も困らないのに、共有ライブラリを作ろうとした瞬間、あるいは
利用者が `--whole-archive` で全取り込みしようとした瞬間に、大量の未定義参照が出る。

```
libtinyState2.a(stdBalancedTree.cpp.o):
    undefined reference to `stdBalancedTree_::callback(...)'
    undefined reference to `vtable for sBalancedTreeCondition'
```

### 原因

`.a` は `.o` を束ねた書庫にすぎず、リンカは<b>未解決シンボルを解決できるメンバだけ</b>を
引き出す。誰も使っていない `.o` は取り込まれず、その中の未定義参照は永久に表に出ない。
結果として次のような欠陥が長期間潜在化する:

* 宣言だけあって定義の無い `virtual`。特に<b>key function</b>(最初の非 inline virtual)が
  未定義だと、そのクラスの vtable がどの翻訳単位にも出力されない。
* 実装ファイルがビルド対象に入っていない(例: Objective-C の `.m` を
  `target_sources` に足し忘れる)。
* ライブラリ間の実リンク依存やプラットフォーム依存(winsock / pthread)の宣言漏れ。

同じ理由で<b>テンプレートが一度も実体化されない</b>と、その中の誤字や型エラーも
コンパイルされないまま残る。

### 対処

<b>ライブラリを全取り込みして共有ライブラリを作るビルドを、検証として回す。</b>
未解決参照がリンクエラーになるので、上記すべてが一度に炙り出せる。

```sh
cmake --build build --target whole_archive_test_so
```

ELF の `.so` は既定で未定義シンボルを許すので `-Wl,--no-undefined` が要る。
macOS の `.dylib` は既定でエラーなので不要。この差のため、<b>Linux では通るが
macOS で落ちる</b>という形で露見することがある。

### 実例

tinyState v2.0.0-rc8 で、この検証を入れた結果 4 件が同時に見つかった:
`stdBalancedTree` の未定義 virtual 3 つ、macOS の `darwin_*` 実装が
ライブラリ未収録、`tinyState2Math` → `tinyState2` のリンク依存未宣言、
in-tree ターゲットの winsock/pthread 未宣言。いずれも `.a` 利用者には
無害だったため、長期間発覚していなかった。

---

## 13. Windows で共有ライブラリを複数イメージから使うと、TLS の実体が複製される

### 症状

Windows で、アプリを「exe + 複数の DLL」に分割し、そのうち複数のイメージから tinyState を
使うと、状態機械の親が辿れず `application` が null になって SIGSEGV する。

```
#0  tinyState_::appMtxLock ... application->mtx.lock()   ← application が null
#2  tinyState_::eventHandler
#4  <アプリのクラス>::<コンストラクタ>          ← exe 側
#6  <アプリのクラス>::start                     ← DLL 側
```

`sCallSection::key->caller()` が null を返しているのが起点になる。単一の exe に収めている
間は起きず、DLL に割った瞬間に出る。

### 原因

`sThreadKey<__TYPE>::operator->()` は<b>ヘッダにある inline なテンプレート</b>で、その中に
関数ローカルの `thread_local` 変数を持つ。この実体が<b>イメージごとに 1 つずつ作られる</b>。

ELF ではこの手のシンボルが `STB_GNU_UNIQUE` になり、動的リンカがプロセス内で 1 実体に
統一する:

```
$ nm -D libtinyState2.so | grep sThreadKey
u _ZGVZNK10sThreadKeyI12sCallSectionEptEvE1h    ← u = unique global
```

<b>PE にはこれに相当する仕組みが無い</b>。exe と DLL がそれぞれ自分の TLS スロットを持つので、
DLL 側で積んだ `sCallSection` を exe 側のコードが読むと空に見える。tinyState を共有ライブラリ
(`tinyState::tinyState2Shared`) にしても、ヘッダの inline は各イメージで実体化されるので<b>解消
しない</b>。

Linux でこれが起きないのは上記のとおりで、移植の過程で「Linux では通るのに Windows だけ落ちる」
という形で現れる。

### 対処

<b>ライブラリ側で、その `thread_local` を 1 つの翻訳単位に隔離する。</b>tinyState 本体では
`sThreadKey<sCallSection>` を明示的特殊化し、定義を `sCallSection.cpp` へ移してある
(宣言だけが `sThreadKey.h` に残る)。ヘッダに定義が無ければどのイメージも自前の実体を作れず、
共有ライブラリから import するしかなくなるので、実行体をどう分割しても 1 実体になる。

<b>利用側のパッケージングでは解決できない</b>点に注意。「テンプレートを実体化する層を 1 つの
イメージへ寄せる」という対処は一見効きそうだが、モジュール機構 (実行時に読み込まれる
プラグイン) を持つアプリでは<b>モジュールが定義上つねに別イメージ</b>なので、公開ヘッダから
そのテンプレートに手が届く限り、サードパーティが 1 回実体化した時点で破綻する。

単一の exe に全部静的リンクする構成なら、そもそもイメージが 1 つなので起きない。

### 構成ごとの安全性

| 構成 | ライブラリ本体 | ヘッダ内 static | |
|---|---|---|---|
| 単一 exe に全部静的 | 1 | 1 | 安全 |
| exe + DLL 群、tinyState はどこか 1 つ | 1 | <b>複数</b> | 上記の対処が要る |
| exe と DLL の両方に tinyState を静的 | 複数 | 複数 | 最悪。ただし下記 |

3 番目を PE で試すと、リンカが弾く:

```
libtinyState2.a(stdObject.cpp.obj): multiple definition of `stdObject::addref()';
    libyourapp.dll.a(...): first defined here
```

これは<b>PE が設計上の誤りを検出している</b>のであって、PE が不便なのではない。ELF は同じ構成を
シンボル interposition で黙って通してしまう (exe 側の定義が勝ち、DLL 側が死にコードになって
結果的に 1 つへ収束する)。§12 と同じく、<b>Linux で通ることは設計の正しさを保証しない</b>。

### 実例

Windows(MinGW) で「exe + ランタイム DLL + モジュール DLL 群」に分割した構成。モジュール DLL は
ランタイム DLL から正しく import できていたが、exe とランタイム DLL の間だけ実体が 2 つに
なっていた (`nm` で両方に同じシンボルが定義として現れる。正常なイメージでは 0 個)。

---

## 14. ロックは 3 つしかない — 役割と順序を跨ぐと止まる

### 症状

teardown が終わらない。reactor は `refio > 0` のまま無限待ちで、状態機械側は何も進まない。
gdb で見ると、片方が `fwIO::mu` を保持して誰かの `lm` を待ち、もう片方が `lm` を保持して
`fwIO::mu` を待っている。典型的な AB-BA デッドロック。

Linux では滅多に出ず、Windows で頻発する。OS のスレッドプール(IOCP / RIO の完了通知)が
**外部スレッドから `eventHandler` を駆動する**ので、状態機械とリアクタが同時に走る機会が
桁違いに多いため。<b>Linux で出ないことは、順序が正しいことを意味しない。</b>

### 3 つのロックと役割

| ロック | 守るもの |
|---|---|
| `tinyState::lm` | その tinyState の内部変数(`_state` / `que` / `state_lock` / `event_listener` 等)。<b>オブジェクトごと</b> |
| `tsApplication::mtx` (app-mtx) | 状態関数の実行の一意性。プロセスに 1 つ、再帰 |
| `fwIO::mu` | fwIO 内のキュー(`read_objs` / `write_objs` / `interval_objs` / `refio`) |

ローカルにしか使わない変数は対象外。`protected` に置いてあっても、その状態関数の中でしか
読み書きしないフラグ等は守る必要がない。

### 順序ポリシー

```
   app-mtx  ->  lm          許可      逆は不可
   app-mtx  ->  fwIO::mu    許可      逆は不可
   lm       <-> fwIO::mu    どちらの向きも作らない
```

守り方は 2 つの規律に落ちる。

1. <b>`lm` を保持したまま他のオブジェクトを呼ばない。</b>他オブジェクトの `eventHandler` も、
   スレッドプールへの投入も、fwIO の API も、`lm` の外で行う。
2. <b>`fwIO::mu` を保持したまま fwIO の外を呼ばない。</b>

### 落とし穴: 再帰ミューテックスの「1 段だけ解放」

`lm` と app-mtx は再帰(`sThreadMutexRecursive`)で、`sThreadMutexHandle` /
`sThreadMutexHandleRelease` は<b>1 段しか</b>取得/解放しない。したがって

```cpp
void A::f()            // 呼び出し元が既に lm を 1 段持っている
{
        { sThreadMutexHandle __h(lm); ... }   // ← ここを抜けても lm は解放されない
        other->method();                       // ← lm 保持のまま外へ出ている
}
```

は<b>意図した解放になっていない</b>。「内側で取って離すから安全」という書き方は、呼び出し元が
既に保持していると成立しない。実際 `invoke_listen` がこの形で、リスナへの配送が `lm` 保持下で
行われていた。

### なぜ `tinyState::state()` はロックを取らないのか

reactor は `fwIO::mu` を保持したまま登録キューを走査し、各オブジェクトの ZOM を判定する。
ここで `state()` が `lm` を取ると `mu -> lm` ができ、上のポリシーに反する。

そこで `_state` を `std::atomic` にして `state()` からロックを外してある。これは保証を弱めて
いない — `state()` は値を返した瞬間に `lm` を手放すので、<b>返った値が使う時点でも有効だという
保証は元々無かった</b>(状態関数の実行中は `lm` が解放されているので、実行中でも `state()` は
成功する)。`lm` が実際に与えていた「引き裂けない読み」と「書き手の先行書き込みが見える」は、
`memory_order_release` / `acquire` の対で保たれている。

<b>`C_TEST` / `C_NAME` は引数を 2 回評価する。</b>atomic を直接渡すと 2 回ロードされ、
2 回目が別値だとマクロが `(TS_TRANS*)0` を辿る。必ずローカルへスナップショットしてから渡すこと。

```cpp
TS_STATE_TYPE st = obj->state();
if ( C_TEST(st,C_ZOM) ) ...
```

### 実例

v2.0.0-rc11 で解消。Windows の RIO 経路で teardown 後にプロセスが終了しない事象(約 40%)が、
`fwIO::loop` の ZOM スイープ(`mu` 保持 → `state()` → `lm`)と、TS_THREAD 状態をワーカ未割当で
キューする枝(`lm` 保持 → `getThread()->ins()` → `setRefio()` → `fwIO::addRefio()` → `mu`)の
AB-BA だった。両方の辺を切って 30 連続ノーハング。

この枝は 2025-01 に一度触られており、そのとき `wakeup()` だけが `lm` の外へ移されて `ins()` が
残っていた。<b>同じ枝の中で「外に出すもの」を一部だけ移すと、残りが後で牙を剥く。</b>

観測についても記録しておく: この種のハングは<b>トレースを入れると消える</b>。ホットパスに出力を
足すとタイミングが変わり、ハング率が別物になる(計測版では別経路が支配的になり、素のビルドとは
分布が違った)。ホットパスは出力ゼロのカウンタにし、出力は無限待ちに入る瞬間の 1 行だけにする。
A/B は必ず<b>同一ビルド構成で交互試行</b>する。
