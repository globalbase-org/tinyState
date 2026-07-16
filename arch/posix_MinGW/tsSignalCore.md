# tsSignalCore — Windows (MinGW) 実装メモ

出典/関連: tinyState Windows 移植 検討メモ。回帰テスト: `example/socktest`
mode3 / mode4。対象コード: `arch/posix_MinGW/src/classes/ts2/c++/tsSignalCore.cpp`。

## 役割

`tsSignalCore` は「シグナル番号ごとに 1 つ」の共有ハンドラ。そのシグナルを待つ複数の
`tsSignal` (`front_list`) にイベントを配る。**シグナルをどう捕まえて、どう配送機構
(fwIO) を起こすか**が POSIX と Windows で全く違う:

- **POSIX**: `sigaction` でハンドラを設定。ハンドラ内から self-pipe に 1 バイト書いて
  `fwIO(select)` を起こす。
- **Windows (MinGW)**: POSIX シグナルが無い。`SetConsoleCtrlHandler` で Ctrl+C
  (`CTRL_C_EVENT`) / ウィンドウ閉じ (`CTRL_CLOSE_EVENT`) を捕まえ、
  `PostQueuedCompletionStatus` で `fwIO(IOCP)` を key 付きで起こす。マップするのは
  SIGINT / SIGTERM のみ。他のシグナル番号 (SIGPIPE 等) は「登録はされるが永久に発火
  しない」= 無害な no-op。

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
