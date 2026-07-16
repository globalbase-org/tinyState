# tinyState COOKBOOK

> v0 草案 (2026-05-25)

「したいこと → コード」形式のリファレンス。各パターンは最小コード例 + なぜそうなるか で構成。

まず [docs/MENTAL_MODEL.md](MENTAL_MODEL.md) と [CLAUDE.md](../CLAUDE.md) を読んでから使うこと。

---

## 使用クラス ステータス早見表

| カテゴリ | クラス | 状態 |
|---|---|---|
| 非 stdObject 系 | `sArray`, `sBufferHandle`, `sCallSection`, `sEndian`, `sException`, `sObject`, `sThreadCond`, `sThreadMutexHandle`, `sThreadMutexRecursive` | ✓ OK |
| stdObject 系 | `stdObject`, `stdArray`, `stdBuffer`, `stdBalancedTree`, `stdEvent`, `stdInterval`, `stdMutex`, `stdQueue`, `stdQueueString`, `stdRx`, `stdString`, `stdStringTree`, `stdHalfOrderQueue`, `stdHalfOrderQueueTS`, `stdLimitSemaphore`, `stdSemaphore` | ✓ OK |
| tinyState 基幹 | `tinyState`, `tsApplication`, `tsCallBuffer`, `tsCallList`, `tsFinishChecker`, `tsGC`, `tsSignal`, `tsThread` | ✓ OK |
| IO 系 | `ts2IO`, `ts2IOdevNull`, `ts2IOdescriptor`, `ts2IOraw`, `ts2IOsocket`, `ts2IOsockTCPconnect`, `ts2IOsockTCPserver`, `ts2IOsockUDP`, `ts2IOsockUnixServer`, `ts2IOunixClient` | ✓ OK |
| プロセス生成 | `ts2System` | ✓ OK |
| フレームワーク | `stdFrameWork`, `fwIO`, `stdEventHandle` | ✓ OK (内部利用) |

補足:
- `tsGC`, `tsThread`, `tsSignalCore`, `co_tsThreadKill` はユーザが直接 `thNEW` しない (内部生成 / 内部専用の子クラス)
- `stdEventHandle` は内部利用 (`listen` 登録のハンドル)
- `ts2IOsockServer` / `s2IOstd` はソケット・IO の内部ベースクラス、`ts2IOwinConsole` は Windows 専用 (`posix_MinGW` overlay)
- `stdMutex` は 2026-07-11 に sException ベースへ刷新済み (旧「要メンテ」区分は解消。詳細 [CLAUDE.md](../CLAUDE.md))
- **要メンテ区分は現在なし。** 廃棄・削除済みクラス (CURL/OpenSSL 群, `stdQue`(C++版), `tsFindFiles`, `tsInsensitive`, レガシー C ツリー) は再利用しないこと → [CLAUDE.md「廃止予定 / メンテ必要」](../CLAUDE.md)

---

## 0. tinyState クラスファイル(.cpp)を作る。

まず、tscpp2 コマンドを使って、クラスファイルを作る。

```bash
tscpp2 new <PATH3/classname> <PATH4> [<PATH5/baseclass> [<PATH6/PARENT>]]
```

PATH3はcppファイルを保存する先のディレクトリパス、classname.cpp というファイルが生成されます。絶対パスか、カレントワーキングディレクトリからの相対パスを指定する。
PATH4はヘッダファイルの保存場所　
PATH5/baseclass は、classnameクラスのベースクラスとなるクラスのヘッダファイルのある位置。
PATH6/PARENT は、parentクラスのヘッダファイルのある位置。
ただし、PATH4, PATH5, PATH6はヘッダのルート位置からの相対パス。

PATH5/baseclass, PATH6/PARENTを省略するとデフォルトは、ts2/c++/tinyState が指定されたものとなる。
PARENT = '-' とすると、PARENT=baseclass となる。

```cpp
#include	"PATH6/PARENT.h"
#include	"_ts2/c++/classname_.h"

// インスタンスとして使われる、クラスのインクルードファイル
#include	"...A.h";   // テンプレートに後から追加
#include	"...B.h";   // テンプレートに後から追加

CLASS_TINYSTATE(ts2/c++/tsHoge,ts2/c++/ts2IO)


#if 0

TS_BEGIN_IMPLEMENT
// _ts2/c++/classname_.h 収容情報
// 実装クラスの宣言
// 状態遷移関数の宣言
// 実装クラスのpublic のインスタンスはインタフェースクラスのpublic から参照される。

// 実装クラスprivate, protectedで使うインスタンスのクラスはここで宣言しておくといい。
class B;   // テンプレートに後から追加

class TS_THISCLASS : public TS_BASECLASS {
public:
	tsHoge_(
		sPtr<PARENT> parent);

	// コンストラクタを追加することができる。テンプレートに後から追加
        tsHoge_(
		sPtr<PARENT> parent,
		int arg1);
	
	sRptr<PARENT,tinyState>		parent;

	class A aa;   // テンプレートに後から追加
private:
protected:
	TS_DEFARGS

	class B bb;   // テンプレートに後から追加
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// _ts2/c++/classname_pb.h 収容情報
// インタフェースクラスの宣言

// predefine
#include	"ts2/c++/sRptr.h"
class tsApplication;

// 実装クラス・公開クラスpublic で使うクラスはここで宣言しておくといい。
class A;   // テンプレートに後から追加

TS_END_INTERFACE

#endif


tsHoge_::tsHoge_(TS_ARGS0)
        : baseclass_(parent),
	  parent(tinyState_::parent)
{
    TS_CPARGS0
}

// コンストラクタを追加した場合は、実態も追加。ただし、引数はTS_ARGSx xをカウントアップ。
// TS_ARGSx はtscpp2 が実装クラスの宣言を舐めて、コンストラクタの数だけ自動的に生成する。
tsHoge_::tsHoge_(TS_ARGS1)
        : baseclass_(parent),
	  parent(tinyState_::parent)
{
    TS_CPARGS1
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/



/*******************************************
	STATE MACHINE
********************************************/

// これ移行に、TS_STATE, TS_THREAD による、状態遷移関数本体の定義。

```



## 0.5. main 関数からの起動

`tsApplication` を作り、その初期化完了後のラムダ内で最初のアプリ tinyState を起動する。

```cpp
#include "ts2/c++/tsApplication.h"
#include "myapp/c++/MyApplication.h"

int main(int argc, char ** argv)
{
    // デバッグトレースが必要なときはここで有効化 (通常はコメントアウト)
    // stdFrameWork::trace_all = "CAL";
    // stdFrameWork::trace_bit = FWTR_ALL;

    setlocale(LC_NUMERIC, "");

    thNEW(tsApplication, (thNULL, [argc, argv](sPtr<tsApplication> app) {
        thNEW(MyApplication, (app, argc, argv));
    }));
}
```

ポイント:
- `tsApplication` の parent は `thNULL` (ルートなので親なし)
- ラムダは tsApplication の初期化完了後に呼ばれる。`app` の sPtr が安全に使える
- 最初のアプリ tinyState (`MyApplication`) の parent は `app` (tsApplication) を渡す
- 以降のすべての tinyState は `application->mtx` を参照できるようになる

実例: `example/2026-03-02_dualMode-flowcontrol/src/main/main.cpp`

### ビルドフロー

`.cpp` を変更するだけでよい。次のビルド時に自動で tscpp2 が走り `_ts2/c++/` 以下のヘッダを再生成してからコンパイルする。手動で tscpp2 を叩く必要はない。

内部的には `.cpp` ごとにスタンプファイルを生成して再実行を制御している。CMake (`cmake/AddTinyStateLibrary.cmake`) が `add_custom_command` で tscpp2 を呼び、`STLCPP file <src> --baseheader=<base> --header=_ts2` 相当のコマンドを実行する。

---

## 1. タイマで待つ

`sTimer::start` + タイムアウト待ち状態。時間単位は<b>マイクロ秒</b>。

メンバ変数として `sTimer timer;` を宣言しておく。

```cpp
// ヘッダ (tscpp2 管理の宣言ブロック内)
sTimer timer;

// 5 秒待ってから次の処理へ進む例
TS_STATE(ACT_START) {
    timer.start(ifThis, 5 * 1000 * 1000);   // 5 秒 (単位: マイクロ秒)
    return ACT_WAIT_TIMER;
}

TS_STATE(ACT_WAIT_TIMER) {
    if ( !timer.is_expire(ifThis) )
        return 0;                            // まだ時間が来ていない
    return rDO | ACT_NEXT;
}
```

タイマをキャンセルしたい場合:

```cpp
timer.stop(ifThis);
```

`FIN_START` でも `timer.stop(ifThis)` を呼ぶとパフォーマンスが上がる (CLAUDE.md Graceful shutdown 参照)。

実例: `src/classes/ts2/c++/tsFinishChecker.cpp`

---

## 2. I/O で待つ (read / write)

`ts2IO*` 派生クラスの `read()` / `write()` は EAGAIN 発生時に自動で `throw sException` して
現在の状態関数を yield にする。ユーザコードは<b>同期 read を書いているように見えて、実は非同期</b>。

### TCP 接続して読み書きする

```cpp
// メンバ変数
sPtr<ts2IOsockTCPconnect> conn;
struct sockaddr_in resolve;
char buf[4096];

TS_STATE(ACT_CONNECT) {
    conn = thNEW(ts2IOsockTCPconnect, (ifThis, &resolve, "example.com", 8080));
    conn->listen(ifThis, TSE_ASSERT);    // 接続確立 (or エラー) で TSE_ASSERT が届く
    return ACT_WAIT_CONNECT;
}

TS_STATE(ACT_WAIT_CONNECT) {
    if ( ev->type != TSE_ASSERT ) return 0;
    if ( ev->source != conn )    return 0;
    if ( conn->state == TS2IO_ERROR )
        return rDO | FIN_START;          // 接続失敗
    return rDO | ACT_READ;
}

TS_STATE(ACT_READ) {
    int n = conn->read(buf, sizeof(buf));
    // EAGAIN のとき sException が自動投入 → このステートが yield される
    // fd が ready になったら自動的にここから再開
    if ( n <= 0 )
        return rDO | FIN_START;          // EOF またはエラー
    // buf[0..n-1] を処理する
    return rDO | ACT_READ;               // 続けて読む
}
```

書き込みも同様。`write()` は EAGAIN 時に自動 yield。

```cpp
TS_STATE(ACT_WRITE) {
    conn->write_c(data, length);         // write_c = 指定バイト数を書き切る版
    return rDO | ACT_READ;
}
```

`conn->state` の値:

| 値 | 意味 |
|---|---|
| `TS2IO_ACTIVE` | 接続中・使用可能 |
| `TS2IO_ERROR` | エラー発生 |
| `TS2IO_CLOSE` | クローズ済み |

> <b>注意</b>: `ts2IOsockTCPconnect` の DNS 解決と `connect(2)` は TS_THREAD で行われるので、
> `ACT_CONNECT` からすぐに read しようとしても接続前に EAGAIN が来る。
> 必ず `TSE_ASSERT` で接続完了を待ってから read/write に進むこと。

### TCP / Unix サーバで accept する (ts2IOsockServer)

`ts2IOsockTCPserver`(AF_INET) と `ts2IOsockUnixServer`(AF_UNIX) は共通ベース `ts2IOsockServer` の薄い派生。生成の引数だけ違い、あとは同じ。`accept()` が接続到着まで yield し、accepted 端を `ts2IO` として返す。以降は上の read_c/write_c と同じ。実例は `example/srvtest`(mode0=tcp / mode1=unix)。

```cpp
// メンバ: sPtr<ts2IOsockServer> server; sPtr<ts2IO> accepted;
//         struct sockaddr peer; int errp;
TS_STATE(INI_START) {
    server = server.d_cast(thNEW(ts2IOsockTCPserver, (ifThis, port)));   // TCP
    // server = server.d_cast(thNEW(ts2IOsockUnixServer, (ifThis, path))); // AF_UNIX
    return ACT_WAIT_READY;                        // bind/listen 完了を待つ
}
TS_STATE(ACT_WAIT_READY) {
    if ( ev->type != TSE_WAKEUP ) return 0;       // サーバ準備完了通知
    if ( server->err != 0 ) {                     // server->errpos にどこで失敗したか
        return rDO | ACT_CLEANUP;
    }
    return rDO | ACT_ACCEPT;
}
TS_STATE(ACT_ACCEPT) {
    accepted = server->accept(&errp, &peer, ifThis);   // ← 接続が来るまで yield
    if ( accepted == thNULL ) return rDO | ACT_CLEANUP; // errp に理由
    return rDO | ACT_SERVE;                        // accepted を read_c/write_c
}
```

> クライアント側は TCP なら `ts2IOsockTCPconnect`、AF_UNIX なら `ts2IOunixClient`。
> 複数接続をさばくなら、`accept()` で得た `accepted` を子ワーカ tinyState に渡し、
> `ACT_ACCEPT` へ `rDO` で戻ってループする(1 accept = 1 状態)。

### UDP データグラムを送受信する (ts2IOsockUDP)

コネクションレス。`sendto` / `recvfrom` を「1 状態 = 1 データグラム操作・ev 非依存」で書く(read_c/write_c と同じ再走ルール、[CLAUDE.md 鉄則 5](../CLAUDE.md))。実例は `example/udptest`。

```cpp
// メンバ: sPtr<ts2IOsockUDP> udp; struct sockaddr from; int flen;
//         char rbuf[8]; struct sockaddr_in dest;
TS_STATE(INI_START) {
    udp = thNEW(ts2IOsockUDP, (ifThis, port));    // INADDR_ANY:port に bind
    return ACT_WAIT_READY;
}
TS_STATE(ACT_WAIT_READY) {
    if ( ev->type != TSE_WAKEUP ) return 0;       // bind 完了
    if ( udp->err != 0 ) return rDO | ACT_CLEANUP;
    return rDO | ACT_SEND;
}
TS_STATE(ACT_SEND) {                              // 宛先は明示的に組む
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port   = htons(peer_port);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    udp->sendto("PING", 4, 0, (struct sockaddr*)&dest, sizeof(dest));
    return rDO | ACT_RECV;
}
TS_STATE(ACT_RECV) {
    flen = sizeof(from);
    int n = udp->recvfrom(0, rbuf, 4, 0, &from, &flen);  // ← 到着まで yield
    // 返信するなら recvfrom がくれた from へ sendto
    return rDO | ACT_DONE;
}
```

> **`recvfrom` の送信元 `from` と長さ `flen` はメンバにする** — yield をまたいで生存
> する必要がある(stack ローカルは再走で別アドレス化。addr-lifetime 契約)。
> INADDR_ANY に bind するので `getsockname` は `0.0.0.0:port`。バッチ受信
> (`recvmmsg` 相当)は `example/mmsgtest` 参照。

<hr>

## 3. 子オブジェクトを起動して結果を待つ

`thNEW` + `listen` + `TSE_RETURN` 受け

```cpp
TS_STATE(ACT_START) {
    child = thNEW(ChildClass, (ifThis, args));
    child->listen(ifThis, TSE_DESTROY);   // 忘れると FIN_START が永久 yield
    return ACT_WAIT_RETURN;
}

TS_STATE(ACT_WAIT_RETURN) {
    if ( ev->type != TSE_RETURN ) return 0;
    if ( ev->source != child )   return 0;
    // 結果を取り出して次へ
    return rDO | ACT_NEXT;
}
```

### TSE_RETURN 受け取りの便利マクロ (tinyState.h)

```cpp
// TSE_RETURN でなければ yield (return 0) する
R_TEST

// R_TEST + ev->msg_obj を x に d_cast して代入
R_SET(x)
```

展開後:
```cpp
#define R_TEST      if ( ev->type != TSE_RETURN ) return 0;
#define R_SET(x)    R_TEST  (x) = (x).d_cast(ev->msg_obj);
```

子が 1 つで source チェックが不要な場合は `R_TEST` / `R_SET` を使うと簡潔に書ける:

```cpp
TS_STATE(ACT_WAIT_RETURN) {
    R_SET(result)          // TSE_RETURN でなければ yield、result に msg_obj を取り出す
    return rDO | ACT_NEXT;
}
```

> `R_REF` / `S_SET` は v1 互換マクロ。新規コードでは使わないこと (廃棄予定)。

<hr>

## 3.5. 子プロセスを起動して出力を読む (ts2System)

`ts2System` は外部プロセスを tinyState として包む。stdout/stdin/stderr をそれぞれ `ts2IO*` で受け取れる(不要なものは `(sPtr<ts2IO>*)0`)。子の出力は `read` で読み、終了は子 tinyState からの `TSE_RETURN` で判定する(§3 の子待ちと同じ形)。実例は `example/systest`。

```cpp
// 実装ブロックで #include ts2/c++/ts2System.h, ts2/c++/ts2IO.h
// メンバ: int retPid; sPtr<ts2IO> rfd; sPtr<tinyState> sys; char rbuf[256];
TS_STATE(ACT_START) {
    // 引数: (parent, &pid, コマンド文字列, &stdout, &stdin, &stderr)
    sys = thNEW(ts2System, (ifThis, &retPid, "#cmd /c echo hello",
                &rfd, (sPtr<ts2IO>*)0, (sPtr<ts2IO>*)0));
    if ( retPid < 0 ) return rDO | FIN_START;      // 起動失敗
    return rDO | ACT_READ;
}
TS_STATE(ACT_READ) {
    int n = rfd->read(rbuf, sizeof(rbuf)-1);       // 子 stdout を読む
    if ( n > 0 ) { rbuf[n] = 0; ::printf("%s", rbuf); return rDO | ACT_READ; }
    return rDO | ACT_WAIT_EXIT;                     // n<=0 は EOF
}
TS_STATE(ACT_WAIT_EXIT) {
    if ( ev->type != TSE_RETURN ) return 0;         // 子 tinyState からの終了通知
    if ( ev->source != sys )      return 0;
    return rDO | FIN_START;                         // 子プロセス終了
}
```

> コマンド文字列の書式(先頭 `#` のシェル指定など)は `ts2System.h` / `systest` を参照。
> 終了時の `rfd` / `sys` の解放順序に注意 — Windows では descriptor teardown を fwIO
> 生存中に走らせる必要がある(`systest` の `SYSTEST_LINGER` 節のコメント参照)。

<hr>

## 4. 複数子の結果を集約

`tsCallList` パターン — 複数の子を並行起動して、全員の TSE_RETURN を集めてから次へ進む。

<b>ポイント</b>: 子を `thNEW` するとき、parent に `cl` (tsCallList) を渡す。子は自分の親 (= cl) へ
TSE_RETURN を送るので、tsCallList が全員分を集約できる。

```cpp
// メンバ変数
sPtr<tsCallList> cl;

TS_STATE(ACT_START) {
    cl = thNEW(tsCallList, (ifThis));           // parent = 自分

    cl->call(thNEW(WorkerA, (cl, arg_a)));      // parent = cl  ← ここが重要
    cl->call(thNEW(WorkerB, (cl, arg_b)));
    cl->finish();                               // 登録完了を通知

    return ACT_WAIT_ALL;
}

TS_STATE(ACT_WAIT_ALL) {
    if ( ev->type != TSE_RETURN ) return 0;
    if ( ev->source != cl )       return 0;
    // 全員の結果が ev->msg_obj (stdArray<sPtr<stdEvent>>) に入っている

    sPtr<ResultA> ra;
    sPtr<ResultB> rb;
    CL_SET(0, ra);    // 0 番目 (wa) の TSE_RETURN の msg_obj を ra に取り出す
    CL_SET(1, rb);    // 1 番目 (wb) の TSE_RETURN の msg_obj を rb に取り出す

    return rDO | ACT_NEXT;
}
```

`CL_SET(n, var)` マクロ: tsCallList.cpp の `TS_BEGIN_INTERFACE` 内で定義。
`ev->msg_obj` の stdArray の n 番目要素の `msg_obj` を `var` に d_cast して代入する。

> <b>注意1</b>: `cl->finish()` を呼び忘れると tsCallList が永久に待ち続ける。
> 全子を登録し終えたら必ず `finish()` を呼ぶこと。
>
> <b>注意2 (ハマりポイント)</b>: `cl->call()` の呼び出し順と `CL_SET(n)` の n は対応している。
> 登録順を間違えると結果が入れ替わったまま動いてしまい、型が合えばコンパイルエラーにもならない。
>
> ```cpp
> cl->call(thNEW(WorkerA, (cl, arg_a)));   // → CL_SET(0, ra) で取る
> cl->call(thNEW(WorkerB, (cl, arg_b)));   // → CL_SET(1, rb) で取る
> // ✗ 順番を逆に登録すると CL_SET(0) に rb が入る
> ```

実例: `src/classes/ts2/c++/tsCallList.cpp`

---

## 5. TSE_RETURN を無視する

`tsCallBuffer` パターン

```cpp
    thNEW(childProc,(thNEW(tsCallBuffer,(ifThis))->ignore(),...));
```

TS_RETURN を返すchildProc を生成するが、TS_RETURNを実際に受けるクラスは、tsCallBuffer であり、これは受け取ったあと何もせず、終了する（無視する）。childProcを呼び出した親も、childProcを以降、かまうことなく、別の仕事をする。

通常通り、
```cpp
    thNEW(childProc,(ifThis,...));
```
として、childProc を呼び出すも、構うことなく、別の仕事をしてもいいが、そのあと、childProc2 を呼び出し、そのTSE_RETURNは補足したい場合。つまり、

```cpp
    thNEW(childProc,(ifThis,...));
    ....
    thNEW(childProc2,(ifThis,...));
    return rDO|ACT_RET;
}
TS_STATE(ACT_RET)
{
    R_TEST
    ....
}
```

では、R_TESTでは、childProc2のTSE_RETURNを待っているはずだが、場合によっては、childProcのTSE_RETURNを補足してしまうかもしれない。

```cpp
    thNEW(childProc,(thNEW(tsCallBuffer,(ifThis))->ignore(),...));
    ....
    thNEW(childProc2,(ifThis,...));
    return rDO|ACT_RET;
}
TS_STATE(ACT_RET)
{
    R_TEST
    ....
}
```

と書いておけば、R_TEST時は、かならず、childProc2 のTSE_RETURNである。

> <b><code>sig_int = thNULL</code> ではなく <code>sig_int->destroy()</code> を使う理由</b>
>
> `tsSignal` は生成時に `tsSignalCore::front_list` (内部リスト) に登録されるため、
> `thNEW` 直後の refcount は 2 以上になっている (自分の sPtr + リストの参照)。
> `sig_int = thNULL` にしても refcount は 1 に減るだけで 0 にならず、オブジェクトが生き続ける。
> `->destroy()` を呼ぶとリストから外れ、refcount が 0 になって gc_thread が回収できる。
>
> このように「自分以外が refcount を持っているクラス」は `= thNULL` だけでは解放できない。
> そのようなクラスは `->destroy()` の明示呼び出しが必要。

<hr>

## 6. 長時間処理を別ワーカに逃がす

TS_STATE → TS_THREAD 昇格。マクロ名を変えるだけ。

```cpp
// 変更前
TS_STATE(ACT_HEAVY) { /* 長い処理 */ }

// 変更後 (mtx が一時 unlock になる点に注意)
TS_THREAD(ACT_HEAVY) { /* 長い処理 */ }
```

TS_THREAD 内で外部ロックを持ったまま `other->eventHandler` を呼ぶとデッドロック (CLAUDE.md 鉄則 3 参照)。

---

## 6.5. セマフォで SM 間の同時実行数を制御する

`stdSemaphore` / `stdLimitSemaphore` — 同時実行数の制限と待ち行列

`get()` は取得できないとき `sException` を投げる。これにより <b>TS_STATE ではノンブロッキング yield、TS_THREAD + <code>THR_CATCH</code> ではブロッキング待ち</b>として同じ関数が動く。

### TS_STATE で使う（ノンブロッキング）

```cpp
TS_STATE(ACT_WAIT_SEM) {
    sem.get();              // 取れなければ sException → SM が yield、release() で再起動
    return rDO | ACT_NEXT;
}

TS_STATE(ACT_DONE) {
    sem.release();
    return rDO | ACT_NEXT;
}
```

### TS_THREAD で使う（ブロッキング）

```cpp
TS_THREAD(ACT_WORK) {
    THR_CATCH(void, ) ([&]() {
        sem.get();          // 取れなければ thrInfo->wait() でスレッドをブロック
    });
    // ... クリティカルセクション ...
    sem.release();
}
```

`THR_CATCH` は sException ベースの API をブロッキング呼び出しに変換するラッパーマクロ。
release() による正常起床か SIGPIPE 等によるスプリアス起床かを `is_ready` ラムダで区別し、スプリアスなら `errno = EINTR` で返る。

### stdSemaphore vs stdLimitSemaphore

| | `stdSemaphore` | `stdLimitSemaphore` |
|---|---|---|
| 初期カウント | コンストラクタで指定 | — |
| 上限 | 固定 (初期値が上限) | `limit(n)` で動的変更可 |
| 優先度付き待ち | `enablePriority` フラグあり | なし |

---

## 6.6. 読み書きロックで排他する (stdMutex)

複数リーダ同時可・ライタ排他の RW ロック。`stdMutex` を 1 つ作って共有したいワーカ全員に渡す。`read_lock()` / `write_lock()` は**ブロックしない** — 取れなければ `sException(EX_STAY)` を投げて yield し、wakeup で状態関数を先頭から再走する(I/O と同じ再走モデル)。だから「1 状態 = ロック取得」で ev 非依存に書く。実例は `example/rwlock-demo`。

```cpp
// 共有ロックを作って渡す (親):
rw = thNEW(stdMutex, ());
thNEW(hwReader, (ifThis, rw, id, work_ms));   // reader に渡す
thNEW(hwWriter, (ifThis, rw, id, work_ms));   // writer に渡す

// reader 側:
TS_STATE(ACT_ACQUIRE) {
    _rw->read_lock();                 // writer 保持中なら sException で yield、wakeup で再走
    // 複数 reader が同時に保持できる。_rw->flag_count() で現在の保持数
    return rDO | ACT_WORK;
}
TS_THREAD(ACT_WORK) {                 // 保持したまま作業(mtx を解放 → reader 同士は重なれる)
    ::usleep(_work_ms * 1000);
    return rDO | ACT_RELEASE;
}
TS_STATE(ACT_RELEASE) {
    _rw->release();                   // 解放
    return rDO | FIN_START;
}

// writer 側: read_lock を write_lock に変えるだけ。単独保持(排他)。
TS_STATE(ACT_ACQUIRE) {
    _rw->write_lock();
    return rDO | ACT_WORK;
}
```

> `stdMutex` は 2026-07 に sException ベースへ刷新済み(旧「state 引数を取り返す」方式は
> 廃止)。状態遷移の同期には `stdMutex`、スレッド同期には `sThreadMutex` を使う
> ([CLAUDE.md 禁止リスト](../CLAUDE.md))。

---

## 7. 外部シグナルを捕捉する

`tsSignal` — OS シグナルを stdEvent に変換

```cpp
// 典型例: SIGINT (Ctrl+C) を受けて FIN_START へ遷移

// 実装クラス定義内
public:
	virtual sPtr<stdEvent> filter(sPtr<stdEvent> ev);
protected:
	unsigned sigint_flag:1;
	sPtr<tsSignal>	sig_int;

// インスタンス関数本体定義
// 状態がいかなる状態であろうと、イベントを捕捉できる関数filter.
// この関数を使う場合は、必ず、
//    return TS_BASECLASS::filter(ev);
// で終わること。どの継承クラスでも捕捉が可能なようにするため。
//
// ★ ev == thNULL チェックが必須 ★
// tinyState の初期化時に filter(thNULL) が呼ばれ、
//   - 例外を投げずに返す → filter_lock = 1 → 以降のイベントが filter を通る
//   - 例外を投げる       → filter_lock = 0 → filter が無効になり TSE_SIGNAL が届かない
// thNULL のまま ev->type を参照するとクラッシュするので return ev で早期リターンする。
sPtr<stdEvent>
classname_::filter(sPtr<stdEvent> ev)
{
   if ( ev == thNULL )
      return ev;                // ← 必須: filter_lock を有効にするため例外を投げない
   if ( ev->type == TSE_SIGNAL ) {
      // シグナルイベントを捕捉。
      if ( ev->msg_int == SIGINT )
      	 sigint_flag = 1;
   }
   return TS_BASECLASS::filter(ev);
}

// 初期化
TS_STATE(INI_START)
{
	sig_int = thNEW(tsSignal,(ifThis,SIGINT));
	...
	
}
....
TS_STATE(ACT_WAIT)
{
   // 終了処理への分岐が可能なポイントで、
   if ( sigint_flag )
      return rDO|FIN_START;
   ....
}


// 終了処理内で
TS_STATE(FIN_START)
{
   ....
   sig_int->destroy();  // ← thNULL ではダメ。理由は下記参照
   ....
}


```

### tsSignal の 3 パターン

`tsSignal` オブジェクトが生きている間は `signal_handler_active` がインストールされ、
シグナルが `tsSignalCore` 経由でアプリに届く。
`tsSignal` が死ぬと `tsSignalCore` は `SIG_DFL`（デフォルト動作）に戻す。

| パターン | やること | 効果 |
|---|---|---|
| <b>tsSignal を作らない</b> | 何もしない | シグナルのデフォルト動作（SIGINT → terminate 等） |
| <b>tsSignal を作り、無視する</b> | filter で捕捉してイベントを捨てる | シグナルを受けても何もしない（握り潰す） |
| <b>tsSignal を作り、介入する</b> | filter で捕捉し、フラグを立てて graceful shutdown へ | Ctrl+C でクリーンに終了、など |

<b>SIGPIPE 吸収（パターン 2 の典型例）</b>

TS_THREAD を持つ SM は `destroy()` 時に `THR_KILL(SIGPIPE)` を受ける（worker thread 中断のため）。
main や top-level SM で `tsSignal(SIGPIPE)` を作っておくだけでデフォルト terminate を防げる:

```cpp
// hwMain::INI_START
sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
// 何も filter しない — SIGPIPE を握り潰すだけ

// hwMain::FIN_START
sig_pipe->destroy();
```

<b>設計上の原則</b>

- `tsSignal` の *存在* がシグナル介入の ON/OFF を決める
- 介入をやめたい（デフォルト動作に戻したい）なら `sig_xxx->destroy()` するだけでよい
- `tsSignal` が生きている間は `tsSignalCore` が `signal_handler_active` を維持し続ける

---

## 8. Graceful shutdown

FIN_START パターン (CLAUDE.md 推奨イディオム参照)

```cpp
TS_STATE(FIN_START) {
    if ( child && !C_TEST(child->state(), C_ZOM) )
        return 0;                    // 子がまだ生きている (非 null かつ非 ZOM)
    io->detach(ifThis);
    timer.stop(ifThis);
    return rDO | FIN_TINYSTATE_START;
}
```

---

## 9. 親の破棄を待ってから動き始める (ジョブのシーケンシャライズ)

`parent->listen(ifThis, TSE_DESTROY)` パターン

```cpp
TS_STATE(INI_START) {
    parent->listen(ifThis, TSE_DESTROY);
    return rDO | INI_WAIT;
}

TS_STATE(INI_WAIT) {
    if ( !C_TEST(parent->state(), C_ZOM) )
        return 0;
    return rDO | ACT_START;
}
```

---

## 10. メンバ変数の初期化場所

コンストラクタは引数のセットアップのみ。初期化は `INI_START` で行う。

```cpp
// ✗ コンストラクタで初期化しない
MyClass::MyClass(sPtr<tinyState> p, int x) {
    val = x * 2;   // ← NG
}

// ✓ INI_START で初期化する
TS_STATE(INI_START) {
    val = args.x * 2;
    return rDO | ACT_START;
}
```

### 上級者向け: TS_PRIVATE でメンバ変数を使用箇所の近くに宣言する

通常はクラス宣言ブロックにまとめて書くメンバ変数を、`TS_PRIVATE(...)` を使うと
<b>使う状態関数のすぐ近く</b>に書ける。`tscpp2` がファイルを走査して拾い上げてくれる。

```cpp
TS_PRIVATE(sPtr<tsCallList> cl;)

TS_STATE(ACT_START) {
    cl = thNEW(tsCallList, (ifThis));
    cl->call(thNEW(WorkerA, (cl, arg_a)));
    cl->finish();
    return ACT_WAIT_ALL;
}
```

`ts2Parallel` の lambda に `[this]` を渡すときも同様に使える:

```cpp
TS_PRIVATE(int counter;)

TS_STATE(ACT_START) {
    counter = 0;
    thNEW(ts2Parallel,(ifThis, 0,
        [this](sPtr<ts2Parallel> me, sPtr<stdEvent> ev) -> int {
            if (me->is_destroyed()) return 1;   // cancel 時: 後始末して抜ける (必須契約)
            ::printf("worker %d\n", counter++);
            if (counter < 10)
                me->spawn();   // 兄弟を chain 生成
            return 1;
        }));
    return ACT_WAIT;
}
TS_STATE(ACT_WAIT) {
    R_TEST   // TSE_RETURN (全兄弟完了) まで待つ
    ...
}
```

> <b>注意</b>: `TS_PRIVATE` は本物のメンバ変数宣言に展開されるため、
> 同じ宣言を 2 箇所に書くとコンパイルエラー (重複メンバ) になる。
> リファクタリングで状態関数をコピーするときに踏みやすいので注意。

<hr>

## 10.5. ts2Parallel でワーカーごとのローカル変数を持つ (sPicoState パターン)

### 問題: lambda の value capture は全ワーカーで共有される

`[this]` キャプチャではメンバ変数は共有できる (それが目的)。しかし
「このワーカーが何番目か」のような<b>ワーカー固有のローカル状態</b>を
lambda の value capture に入れても、各ワーカーへのコピーに <b><code>org</code> 保証</b> がある
ため、全ワーカーは同じ初期値から始まることが保証される (後述)。

ただし lambda 内で `return 0` による yield (sException 再起動) を使うと、
再起動時に value capture の値が<b>リセット</b>されてしまう。
変数を yield をまたいで保持するには `sPicoState` を使う。

### 解決: sPicoState — lambda 内マイクロコルーチン

`sPicoState.h` が提供するマクロ群で、lambda body の中に
「状態を保持したまま yield できるコルーチン」を埋め込める。

```cpp
#include "ts2/c++/sPicoState.h"

TS_STATE(ACT_START)
{
TS_PRIVATE(int counter;)
TS_PRIVATE(sPtr<stdLimitSemaphore> sem;)
    counter = 0;
    sem = thNEW(stdLimitSemaphore,(4));

    struct {
        PS_PRESET          // int __state; int __recursive; を展開
        int my_counter;    // ワーカー固有のローカル変数
    } pico_state = {};     // ← POD なので = {} で明示的にゼロ初期化が必須

    thNEW(ts2Parallel,(ifThis, 0,
        [this, pico_state](sPtr<ts2Parallel> me, sPtr<stdEvent> ev) mutable -> int {
        //                                                           ^^^^^^
        //                  value capture した pico_state を変更するために mutable 必須
            if (me->is_destroyed()) return 1;   // cancel 時: 後始末して抜ける (必須契約)
            PS_STATEMENT(pico_state,
                PS_DEF(my_counter);    // my_counter を pico_state.my_counter への参照に束縛
            ,
            PS_STATE(psINI)
                my_counter = counter++;
                if ( counter < 10 )
                    me->spawn();   // 兄弟を chain 生成
            PS_STATE(psDO)
                sem->get();            // 取れなければ sException → yield、再開で続行
                ::printf("  worker %d\n", my_counter);
                sem->release();
                PS_RETURN(1);          // 完了 (__state を psINI にリセットして return 1)
            )
            return 0;
        }));
    return ACT_WAIT;
}

TS_STATE(ACT_WAIT)
{
    R_TEST
    ::printf("all %d workers done\n", counter);
    return rDO | FIN_START;
}
```

### ts2Parallel の org 保証

`ts2Parallel` は呼び出し元から受け取った `fn` を <b><code>org</code> フィールドに保存</b>する。
全ワーカー (root も兄弟も) は `org` からコピーを取るため、
<b>いつ・どのワーカーから兄弟が spawn されても、初期状態は常に同じ</b>。

```
  hwMain が渡した fn  →  org (保存)
                           ├─ worker_0 の _fn = org のコピー  (__state=0, __recursive=0)
                           ├─ worker_1 の _fn = org のコピー  (__state=0, __recursive=0)
                           └─ worker_N の _fn = org のコピー  (__state=0, __recursive=0)
```

`worker_0` が body を何ステップか実行した後に `worker_1` を spawn しても、
`worker_1` は `worker_0` の途中状態ではなく `org` (= 初期状態) を受け取る。
実装の詳細 (spawn タイミング、実行順) がユーザーの body 初期状態に影響しない。

### sPicoState マクロ早見表

| マクロ | 展開内容・用途 |
|---|---|
| `PS_PRESET` | `int __state; int __recursive;` を struct 内に展開 |
| `PS_STATEMENT(name, x, y)` | 再入ガード + state dispatch ブロック本体。`x` に `PS_DEF`、`y` に `PS_STATE` を書く |
| `PS_DEF(var)` | `typeof(name.var)& var(name.var);` — struct メンバへの参照を作る |
| `PS_STATE(label)` | `case label:` ジャンプラベル |
| `PS_RETURN(val)` | `__state = psINI; return val;` — 完了して psINI に戻す |
| `psINI` / `psDO` | 0 / 1 の定数。PS_STATE の label として使う |

### 注意点

- <b><code>mutable</code> が必須</b>: value capture した `pico_state` を変更するため lambda に `mutable` が必要。なければ `const` 扱いとなりコンパイルエラー。
- <b><code>= {}</code> ゼロ初期化が必須</b>: `PS_PRESET` の展開は POD の `int` フィールド。`= {}` なしでは `__recursive` がゴミ値になり "cannot recursive call" panic が起きる。
- <b><code>return 0;</code> を PS_STATEMENT の後に</b>: PS_STATEMENT ブロックを抜けたとき non-void 関数が値を返さない場合のコンパイル警告を防ぐ (上記例では PS_RETURN が return するためだが、コンパイラは静的に判断できない)。
- **同じ `pico_state` を持つ関数を再帰的に呼んではいけない**: `PS_STATEMENT` 実行中 (`__recursive == 1`) に、同じ `pico_state` を参照する関数が再び呼ばれるとその場で panic する。tinyState のように状態遷移の不可分性を保証する機構はなく、`__recursive` はそのためのガード変数である。lambda の body から、自身を直接または間接に呼び出す設計は NG。

実例: `example/parallel-demo/src/classes/hw/c++/hwMain.cpp`

### spawn() / cancel() API

#### spawn() — 兄弟ワーカーの生成

```cpp
me->spawn();                // type・body を root から継承して兄弟を生成
me->spawn(type);            // type を指定 (0=TS_STATE, 1=TS_THREAD)
```

`thNEW(ts2Parallel,(me, type))` の糖衣構文。

#### cancel() — グループ全体を中断

```cpp
me->cancel();   // 全兄弟 + root を destroy し、呼び出し元へ TSE_RETURN を返す
```

どのワーカーの body からでも呼べる。内部動作:

1. `_root->cancel()` へ転送
2. `_cancelling` フラグで二重実行をガード
3. `_root->_next` チェーンを走査して全兄弟を `destroy()`
4. root 自身も `destroy()`
5. 兄弟が順次 FIN_START を経て死ぬと root の FIN_START が通り、呼び出し元へ TSE_RETURN

**注意: `cancel()` の直後は必ず `return` すること。**

```cpp
PS_STATE(psDO)
    if ( is_error() ) {
        me->cancel();
        PS_RETURN(1);   // ← cancel() の後は必ず即 return
    }
    // 通常処理...
    PS_RETURN(1);
```

#### 必須契約: body 冒頭で `is_destroyed()` を見て後始末する

<b>すべての body は冒頭で <code>me->is_destroyed()</code> を確認し、真なら後始末をして必ず非 0 を返して抜けること。</b>

```cpp
thNEW(ts2Parallel,(ifThis, 0,
    [this](sPtr<ts2Parallel> me, sPtr<stdEvent> ev) -> int {
        if ( me->is_destroyed() ) {
            // アプリ依存の後始末 (確保したリソースの解放など)
            return 1;            // ← 非 0 を返して FIN へ。0 を返してはいけない
        }
        // 通常処理...
        return 1;
    }));
```

理由 (コードの挙動):
- `cancel()` は生き残っている兄弟を <code>destroy()</code> するだけ。<code>destroy()</code> は <code>_state</code> を FIN に飛ばさず、<code>destroy_flag</code> を立てて <b>現在の状態 (=body) を 1 回だけ再実行させる</b> (`TSE_INVOKE` + `THR_KILL(SIGPIPE)`)。
- ts2Parallel の `ACT_START` / `ACT_START_THR` は <b><code>is_destroyed()</code> を見ずに body を呼ぶ</b>。これは意図的な設計 — 回収すべきリソースはアプリ依存なので、後始末を body に委ねるため。
- したがって <b>body 側で検出しないと</b>:
  - 処理を続行した body が <code>me->spawn()</code> すると、cancel から逃げる<b>孤児ワーカー</b>が生まれる
  - destroyed 中に <code>return 0</code> (suspend) を返すと、<code>destroy()</code> の起こしは 1 回きりなので<b>永久に finalize しない</b>

> `me->is_destroyed()` は body が自分自身の状態関数内で走る (`me == ifThis`) ための<b>自己確認</b>で、正当な使い方 (外部オブジェクトへの `a->is_destroyed()` とは別物。セクション「sPtr 系」参照)。

---

## 11. イベントを別オブジェクトに送る

`other->eventHandler(thNEW(stdEvent, ...))` で任意のイベントを送れる。

```cpp
// 結果を親に返す (子 → 親への TSE_RETURN)
parent->eventHandler(thNEW(stdEvent, (TSE_RETURN, ifThis, result)));

// 相手を起こすだけ (内容なし)
other->eventHandler(thNEW(stdEvent, (TSE_INVOKE, ifThis, thNULL)));

// 任意のイベントタイプを送る
other->eventHandler(thNEW(stdEvent, (TSE_UPDATED, ifThis, thNULL)));
```

`stdEvent` のコンストラクタ: `(イベントタイプ, 送信元, msg_obj)`

- 送信元は必ず `ifThis` (実装クラスの内部ポインタ `thThis` は渡さない — CLAUDE.md 参照)
- `msg_obj` に任意の `sPtr<stdObject>` を載せられる。不要なら `thNULL`

> <b>TS_THREAD 内での注意</b>: app-mutex を保持したまま `other->eventHandler` を呼ぶとデッドロック。
> TS_THREAD 内で外部ロックを持っている場合は `application->gc->exe` でスタックをパージしてから送ること (CLAUDE.md 鉄則 3 参照)。

<hr>

## 12. 派生クラスの INI/FIN 連鎖

classA → classB → classC という単一継承チェーンで、各クラスが初期化・終了処理を連鎖させるパターン。

### INI (初期化順: A → B → C)

各クラスが「自分の名前付き入口ステート」を定義し、派生クラスがそれをオーバーライドして自分の初期化を挿入する。

```cpp
// 親クラス classA
TS_STATE(INI_START) {
    // classA の初期化
    return rDO | INI_classA_START;  // ← 派生クラスへの引き渡しゲート
}
TS_STATE(INI_classA_START) {
    return rDO | ACT_START;         // ← classB がここをオーバーライドする
}

// 子クラス classB (classA を継承)
TS_STATE(INI_classA_START) {        // classA のゲートをオーバーライド
    // classB の初期化
    return rDO | INI_classB_START;
}
TS_STATE(INI_classB_START) {
    return rDO | ACT_START;         // ← classC がここをオーバーライドする
}

// 孫クラス classC (classB を継承)
TS_STATE(INI_classB_START) {        // classB のゲートをオーバーライド
    // classC の初期化
    return rDO | INI_classC_START;
}
TS_STATE(INI_classC_START) {
    return rDO | ACT_START;
}
```

実行順: `INI_START`(A) → `INI_classA_START`(B) → `INI_classB_START`(C) → `ACT_START`

### FIN (終了順: C → B → A、INI の逆順)

FIN は「最も派生したクラスが先に後片付けする」必要があるため、INI と逆の構造になる。
派生クラスが `FIN_START` (classA のゲート) をオーバーライドして先頭に割り込む。

```cpp
// 親クラス classA
TS_STATE(FIN_START) {
    return rDO | FIN_classA_START;  // ← 派生クラスへの引き渡しゲート
}
TS_STATE(FIN_classA_START) {
    // classA の後片付け
    return rDO | FIN_TINYSTATE_START;
}

// 子クラス classB (classA を継承)
TS_STATE(FIN_START) {               // classA のゲートをオーバーライド
    // classB の後片付け
    return rDO | FIN_classB_START;
}
TS_STATE(FIN_classB_START) {
    return rDO | FIN_classA_START;  // classA の後片付けへ引き継ぐ
}

// 孫クラス classC (classB を継承)
TS_STATE(FIN_START) {               // classB のゲートをオーバーライド
    // classC の後片付け
    return rDO | FIN_classC_START;
}
TS_STATE(FIN_classC_START) {
    return rDO | FIN_classB_START;  // classB の後片付けへ引き継ぐ
}
```

実行順: `FIN_START`(C) → `FIN_classC_START` → `FIN_classB_START`(B) → `FIN_classA_START`(A) → `FIN_TINYSTATE_START`

---

## 12.5. 文字列を正規表現でマッチ・抽出する (stdString / stdRx)

`stdString` にパターン `stdRx` をかけてマッチ・部分抽出する。`rxVar(n)` で n 番目の部分文字列を取る(`$0` = マッチ全体、`$1..` = キャプチャ群)。実例は `example/regextest`。

```cpp
#include "ts2/c++/stdString.h"
#include "ts2/c++/stdRx.h"

sPtr<stdString> s   = thNEW(stdString, ("word rest"));
sPtr<stdRx>     re  = thNEW(stdRx, ("^([^ \t]+)"));   // POSIX ERE
sPtr<stdString> hit = s->rx("mv", re);                 // マッチ実行
if ( hit.is_notNull() && s->rxVar(1).is_notNull() ) {
    const char * whole = s->rxVar(0)->get_str();       // $0 = マッチ全体
    const char * g1    = s->rxVar(1)->get_str();       // $1 = 1番目のキャプチャ群 → "word"
}
```

> **POSIX ERE (leftmost-longest)** であることに注意。`a|ab` は入力 `"ab"` に対して
> `"ab"`(長い方)にマッチする — `std::regex` の leftmost-first(`"a"` を返す)とは違う。
> MinGW でも vendored TRE により同じ挙動になる。
> `rx()` のモード文字列(`"mv"` 等)の詳細は `stdRx.h` を参照。非マッチ時 `hit` は null、
> `rxVar(n)` も null チェックしてから `get_str()`。

---

## 13. tinyState オブジェクトの死に方

tinyState オブジェクトが死ぬパターンは主に 3 つ。

### パターン A: ->destroy() で外部から殺す

他のオブジェクトが sPtr を持っていても強制的に死ねる。
`is_destroyed()` が true になり、次の状態遷移で `FIN_START` へ向かう。

```cpp
child->destroy();   // child を強制終了させる。sPtr を持っていても死ぬ
```

### パターン B: 役割を終えて自ら死ぬ

処理が完了したら `FIN_TINYSTATE_START` へ遷移して自分で終了する。
親や他のオブジェクトが sPtr を持っていても死ねる (A と同じ理由)。

```cpp
TS_STATE(FIN_START) {
    // リソース解放...
    return rDO | FIN_TINYSTATE_START;   // ← ここで自律的に死ぬ
}
```

### パターン C: refcount = 0 で死ぬ

誰も sPtr を持たなくなったとき gc_thread が回収する。<b>設計が正しければ</b>最もクリーンな死に方。

```cpp
{
    sPtr<ChildClass> child = thNEW(ChildClass, (ifThis));
    // このスコープを抜けると child の sPtr が消え、他に参照がなければ refcount=0 → 死ぬ
}
```

<b>注意点</b>:
- `sPtr` はループ参照を検出しない。A→B→A のような参照ループが残ると refcount が 0 にならず、永遠に生き続ける
- tsSignal のように内部リストに登録されるクラスは、`= thNULL` だけでは refcount が 0 にならない (セクション 7 参照)
- 思わぬ待ちキューに繋がれていると死ねないケースがある

<b>パターン C を安全に使うには</b>: childClass の設計段階で「誰が参照を持つか」を明確にし、ループ参照や外部リスト登録がないことを確認する。うまく設計できれば子オブジェクトが自動的にクリーンになり、最も気持ちよい。

### ハマりポイント: グローバル / static な sPtr を持たせない

<b>ファイルスコープ・関数 static・グローバル変数に <code>sPtr&lt;T&gt;</code> / <code>sArray&lt;sPtr&lt;T&gt;&gt;</code> を置いてはいけない。</b>

理由は <code>sPtr</code> の解放経路にある。<code>sPtr</code> の refcount が 0 になったオブジェクトの実デストラクタ (<code>delete</code>) は、専用の <b><code>gc_thread</code></b> (`stdObject.cpp` の `gc_thread` / `gc`) が、<b>スレッドのスタックトップ・フレームワークロックを一切持たない安全な状態</b>で実行するよう設計されている。

グローバル / static の <code>sPtr</code> は、この正規経路を完全にバイパスする。プログラム終了時、libc の <b><code>exit()</code> が <code>__cxa_atexit</code> 経由で main スレッド上でデストラクタを走らせる</b>ため、

- gc_thread ではない (= 想定外のスレッド)
- フレームワーク (app / fwIO / gc_thread / `stdObject::refMtx`) が既に撤収・破棄され始めている
- 他の worker スレッドがまだ生存していることもある

という不正なコンテキストで <code>~sPtr → relref → delete</code> が走り、<code>refMtx</code> / refcount の use-after-free で <b>終了時に SEGV する</b> (実例: `static sArray<sPtr<pigData>> g_asyncExports` を exit に破棄させて 100% クラッシュ)。

<b>対策 (いずれか)</b>:

1. <b>そもそもグローバル / static の <code>sPtr</code> を作らない</b> (第一選択)。
2. やむを得ず持つ場合は、<b>アプリ終了前に確実に <code>x = thNULL;</code></b> を書き込み、まだ app/gc_thread が生きているうちに gc_thread 経由で解放させる。
3. <b>推奨パターン</b>: アプリ起動時の「実態元祖 tinyState」(例 `app`) の public メンバにグローバル状態を集約し、各 tinyState は `app` を指す <code>sPtr</code> 経由で <code>app-&gt;hoge</code> と参照する。元祖が死ぬとき配下が連鎖的に gc_thread で安全に解放され、libc の exit 破棄が一切発生しない。

```cpp
// ✗ ファイルスコープ static sPtr。exit で main スレッドが破棄 → SEGV
static sArray<sPtr<pigData> > g_asyncExports;

// ✓ 実態元祖 tinyState に集約し app 経由で参照
class MyApp : public tinyState {
public:
    sArray<sPtr<pigData> > asyncExports;   // app の寿命に従い gc_thread で解放
};
//   利用側:  app->asyncExports.push(promise);
```

> これは tinyState フレームワーク側のバグではなく <b>利用側コードのハマりポイント</b>。グローバル <code>sPtr</code> は「終了時クラッシュ」として顕在化しやすい。

---

## 14. Doxygen コメントの書き方 (EN/JA 2-block スタイル)

tinyState の Doxygen コメントは <b>前半英語・後半日本語の 2-block 方式</b>を採用する。
1 行ごとに `/` や `//` で英和を混ぜるのではなく、英語ブロックを通し、区切り行を置いてから日本語ブロックを通す。

### 基本構造

```cpp
/**
 * @brief One-line English summary. / 1 行日本語サマリ。
 * @details
 * Full English description here.<br>
 * Second line of English.<br>
 * <br>
 * @code{.cpp}
 * // English code example
 * thNEW(MyClass, (parent, 0));
 * @endcode
 *
 * @note English note if needed.
 *
 * ---
 *
 * 日本語での説明をここに書く。<br>
 * 2 行目の日本語。<br>
 * <br>
 * @code{.cpp}
 * // 日本語側にも同じコード例を再掲
 * thNEW(MyClass, (parent, 0));
 * @endcode
 *
 * @note 日本語の補足がある場合はここ。
 */
```

### ルール

| 項目 | 方針 |
|---|---|
| `@brief` | `英語。 / 日本語。` の形で 1 行に両方書く |
| `@details` | 前半に英語ブロック、`---` で区切り、後半に日本語ブロック |
| コードブロック | `@code{.cpp}` / `@endcode` で囲む。両言語ブロックに同じ例を<b>繰り返す</b> |
| 改行 | `@details` 内のテキスト行末に `<br>` を付ける (doxygen は改行を無視するため) |
| 段落区切り | `*` だけの行 (空行) を挟む |
| `---` | 英語ブロックと日本語ブロックの境界を示す区切り行 (`* ---`) |
| `@note` / `@see` | 英語・日本語の各ブロック末に個別に置く |

### よくある落とし穴

<b>改行が潰れる問題</b>: `@details` 内の普通のテキスト行は doxygen にパラグラフとして結合される。
改行を保持したいときは行末に `<br>` を付ける。

```
* type == 0: run in TS_STATE<br>
* type == 1: run in TS_THREAD<br>
```

<b>コードが 1 行になる問題</b>: コード例を普通テキストとして書くと改行が消える。
必ず `@code{.cpp}` / `@endcode` で囲む。

<b><code>@code{.cpp}</code> vs <code>```cpp</code></b>: doxygen の `@details` 内ではバッククォートのコードフェンスは使えない。
`@code{.cpp}` / `@endcode` を使うこと。

### 実例 (ts2Parallel クラスより抜粋)

```cpp
/**
 * @brief Fork/join parallel execution framework. / fork-join 型並行実行フレームワーク。
 * @details
 * Spawn workers with `thNEW(ts2Parallel,(parent, type, fn))`.<br>
 * Each worker runs `body()` then signals completion.<br>
 * When all siblings finish, a TSE_RETURN is sent to the original parent.<br>
 * <br>
 * @code{.cpp}
 * TS_STATE(ACT_START) {
 *     thNEW(ts2Parallel,(ifThis, 0, [](sPtr<ts2Parallel> me, sPtr<stdEvent>) {
 *         ::printf("hello from worker\n");
 *         return 1;
 *     }));
 *     return ACT_WAIT;
 * }
 * TS_STATE(ACT_WAIT) {
 *     R_TEST
 *     return rDO | FIN_START;
 * }
 * @endcode
 *
 * ---
 *
 * `thNEW(ts2Parallel,(parent, type, fn))` でワーカーを生成する。<br>
 * 各ワーカーは `body()` を実行し、完了を通知する。<br>
 * 全兄弟が完了すると、元の parent に TSE_RETURN が送られる。<br>
 * <br>
 * @code{.cpp}
 * TS_STATE(ACT_START) {
 *     thNEW(ts2Parallel,(ifThis, 0, [](sPtr<ts2Parallel> me, sPtr<stdEvent>) {
 *         ::printf("hello from worker\n");
 *         return 1;
 *     }));
 *     return ACT_WAIT;
 * }
 * TS_STATE(ACT_WAIT) {
 *     R_TEST
 *     return rDO | FIN_START;
 * }
 * @endcode
 */
```

---

## Open issues

- [x] セクション 12「派生クラスの INI/FIN 連鎖」にコード例を追加 — §12 に実装済み (2026-07-14)
- [x] `ts2DevNull.cpp` → `ts2IOdevNull.cpp` リネーム — 2026-07-11 完了
- [x] 要メンテクラスの修繕計画 — `stdMutex` 刷新済み・CURL/OpenSSL 群は削除済み。要メンテ区分は解消 (早見表反映済み)
