# tsSignalCore — Windows (MinGW) 実装メモ

出典/関連: tinyState Windows 移植 検討メモ。回帰テスト: `example/socktest`
mode3 / mode4。対象コード: `arch/posix_MinGW/src/classes/ts2/c++/tsSignalCore.cpp`。

## 役割

`tsSignalCore` は「シグナル番号ごとに 1 つ」の共有ハンドラ。そのシグナルを待つ複数の
`tsSignal` (`front_list`) にイベントを配る。**シグナルをどう捕まえて、どう配送機構
(fwIO) を起こすか**が POSIX と Windows で全く違う:

- **POSIX**: `sigaction` でハンドラを設定。ハンドラ内から self-pipe に 1 バイト書いて
  `fwIO(select)` を起こす。
- **Windows (MinGW)**: POSIX シグナルが無い。`SetConsoleCtrlHandler` でコンソール制御
  イベントを捕まえ、`PostQueuedCompletionStatus` で `fwIO(IOCP)` を key 付きで起こす。
  写すのは **5 イベント → 2 シグナル**:

      CTRL_C_EVENT / CTRL_BREAK_EVENT                      -> SIGINT
      CTRL_CLOSE_EVENT / CTRL_LOGOFF_EVENT / CTRL_SHUTDOWN_EVENT -> SIGTERM

  他のシグナル番号 (SIGPIPE 等) は「登録はされるが永久に発火しない」= 無害な no-op。

## ★ teardown 戦略が Linux と Windows で真逆な理由 (この移植の肝)

`tsSignalCore` は 2 本のリストを持つ:

- `signal_list` … 生ポインタの走査用リスト (`search_signal` が辿る)。
- `_signal_list` / `_next` … その **所有権を持つ sPtr ミラー**。

**「終了時にこのリストを片付けるか否か」の方針が OS で真逆になる。**

### Linux: あえてリストを壊さない

Linux の `del_signal` は生 `signal_list` しか触らず、所有 sPtr チェーンを外さない
(FIN でも実質リストを畳まない)。**これは意図的**:

- POSIX のシグナルハンドラは **async-signal 文脈**で走り、その中で **mutex を使えない**
  (mutex 系は async-signal-safe でない)。
- プロセス終了処理の最中にシグナルが来ると、**シグナルハンドラ (signal_list を mutex
  無しで読む)** と **tsSignalCore の teardown (リストを mutex 付きで書き換え)** が
  **稀に衝突**する。
- ハンドラ側を mutex で守れない以上、安全策は **「作ったリストを壊さずにプロセスを
  終了する」しか無い**。→ Linux は list を最後まで無傷に保ち、あとはプロセス終了 (OS が
  全メモリを回収) に任せる。

### Windows: きちんと外す (外さないとクラッシュする)

Windows の "シグナルハンドラ" = `console_handler` は **通常の Win32 OS スレッド**で走る
(async-signal 文脈ではない)。→ **mutex を普通に使える**。Linux のような「壊すな」制約が
無く、逆に **きちんと外さないと静的破棄でクラッシュする** (下記バグ 1)。→ Windows は FIN
で両リストから properly に unlink する。

> 教訓: Linux の「リストを壊さない」戦略を Windows にそのままコピーしてはいけない。
> 制約の出所 (signal-handler-in-async-context) が Windows には存在しないため、Linux では
> 必須の回避策が Windows では逆にバグ (lingering → 静的破棄クラッシュ) になる。

## 発見した 2 バグ (2026-07-13 根治済)

### バグ 1: 静的破棄順序クラッシュ

MinGW の `del_signal` が当初 Linux 同様に生 `signal_list` だけ外し、所有 sPtr チェーン
`_signal_list`/`_next` を外していなかった。→ `tsSignalCore` が static `_signal_list` に
握られ **静的破棄時まで生存** → refcount ロック (per-id `sThreadMutex`) の静的破棄が先に
済むと、`tsSignalCore` 解放時の `relref` が **破壊済みロックの virtual `lock()` を
null vtable 経由で呼んでクラッシュ**する。

    クラッシュ連鎖:
    sPtr<tsSignalCore>::_replace
      -> stdObject::relref
        -> sThreadMutexHandle ctor
          -> sThreadMutex::lock()   (virtual)
            -> *(null vtable)         ← execute access to 0x0

**根治**: `del_signal` で **sPtr チェーンからも splice out** する。→ 親 (application) が
**通常の teardown 時 (refcount ロック健在)** に解放でき、静的破棄まで生き残らない。

### バグ 2: IOCP キー空間衝突 → teardown hang

`tsSignalCore` の IOCP 完了キー `key` が独自カウンタ `g_tssig_key` (1 起点) だった。一方
socket / descriptor / ts2System は共有 `ts2io_alloc_key()` (`g_ts2io_fdid`, 1 起点) を
使う。**別カウンタなので値が衝突する** (最初の tsSignal key=1 と最初の socket fdid=1)。
fwIO は IOCP 完了を **key で dispatch** するため、**socket の完了が tsSignalCore の
`io->read` 登録に誤ヒット**してリアクタの登録集合を壊す → **tsSignal と socket が同居する
teardown が実機でハング** (wine は隠す)。実運用の「Windows socket server + SIGINT
ハンドラ」で踏む。

**根治**: `tsSignalCore` も共有 `ts2io_alloc_key()` を使う。プロセス内の全 IOCP キー
(sockets / descriptors / ts2System 子終了 / tsSignalCore) が一意になる。

## 検証

`example/socktest` (回帰テスト):

- **mode3**: `tsSignal(SIGINT)` を作って destroy するだけ (tsSignalCore 単独 teardown)
  → バグ 1 の回帰。
- **mode4**: socket pingpong + `tsSignal(SIGINT)` 同居 → バグ 2 の回帰。

実機 Windows 11 で mode3 / mode4 / mode0-2 / stress N=1000 すべて crash=0 / hang=0 /
bad=0。wine 全 mode clean。Linux 無回帰 (本修正は `arch/posix_MinGW` のみ)。

## 何が検証され、何が検証されていないか

上の socktest の検証が押さえているのは **teardown と fwIO の配送機構**で、**イベントの入口
ではない**。mode3 / mode4 はどちらも `tsSignal(SIGINT)` を *作って壊す* テストで、Ctrl+C を
実際に撃ってはいない。入口については **2026-09-15 に box で別途測った** (下記)。

## Ctrl+C の入口 — 2026-09-15 実測

実機 box (MinGW / gcc 16.1.0) で、`example/interval-timer` を子プロセスとして
`CREATE_NEW_PROCESS_GROUP` で起こし、そのグループにだけ制御イベントを送って測った。

<pre>
                                      修正前 (rc16)        修正後
CTRL_C_EVENT                          届かない (exit=259)   届く (goodbye / exit=0)
CTRL_BREAK_EVENT                      届く                  届く
プロセス内 raise(SIGINT)               即死 (exit=3)         graceful (goodbye / exit=0)
</pre>

いずれも `GenerateConsoleCtrlEvent` は **成功 (1) を返す**。「送れたのに届かない」形なので、
戻り値を見ているだけでは気づけない。

★ **切り分けの結論**: 修正前でも CTRL_BREAK なら `tsSignal` → `filter()` → graceful shutdown
まで通っていた。⇒ **中断機構そのものは動いていて、死んでいたのは CTRL_C の入口だけ**だった。

### 原因 1: 継承された「Ctrl+C を無視」状態

`SetConsoleCtrlHandler(NULL, TRUE)` は「Ctrl+C を無視する」状態を足す操作で、**この状態は
継承される** — 新しいプロセスグループで起こされた子は最初からこれが立っており、親が立てた
場合も子へ渡る。立っている間、イベントは **どのハンドラにも渡る前に握り潰される**。

INI はハンドラを張る **前** に `SetConsoleCtrlHandler(NULL, FALSE)` で解除するようにした。
`CTRL_BREAK_EVENT` はこの無視フラグの対象外で、だから Ctrl+Break だけは動いていた。

⚠ これは「親が意図して Ctrl+C を黙らせている」場合にそれを覆す。**無条件で解除する**判断を
採った。INI が走るのは利用側が `tsSignal` を作って「このシグナルを教えてくれ」と言った後
だけなので、頼んだ相手に届かないほうが驚きが大きい、という理由による。

### 原因 2: CRT の `signal()` が張られていなかった

コンソール経由の Ctrl+C は `console_handler` に来るが、**プロセス内の `raise(SIGINT)`** は
そこを通らない。POSIX 版は `sigaction` を張るのに MinGW 版は何も張っておらず、CRT 既定の
動作でプロセスが即死していた (上表の exit=3)。

`crt_signal_handler` を足し、`console_handler` と同じ `deliver()` を呼ぶようにした。Windows
ではこのハンドラは raise した **スレッド上**で走る (async-signal 文脈ではない) ので、
`console_handler` と同じく IOCP へ post してよい。

### 回帰

同じ box で `socktest` mode0-4 全通過、mode3 / mode4 を 50 回ずつ回して非ゼロ終了 0。

### 教訓

この節が無かったために、「**`.md` の冒頭だけ読んで実装を読まない**」「**`FIRST CUT` の
陳腐化コメントを根拠に検証状況を判断する**」という形で、誤認を含む起票が 1 本出た。
**検証節には「何を確かめたか」と同じ重さで「何を確かめていないか」を書くこと。**
