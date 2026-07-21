# tinyState ソケット I/O バックエンド — OS 別実装マトリックス

> v0 草案 (2026-07-21)
> **コード由来の事実**（各 arch の実装・実測値）をまとめたもの。実測は共有 Windows 実機
> NucBox7 (Windows 11) / simu01 (Linux) 由来。`要ひさ確認` の断りが無い箇所は実装/計測に基づく。

`ts2IO` 系（`ts2IOdescriptor` / `ts2IOsocket` / `ts2IOsockUDP` …）が、各 I/O API を OS ごとに
**どのカーネル機構で実現しているか**の一覧と、その設計判断（特に Windows の RIO 既定化と自動
フォールバック）の解説。API の契約は各クラスの doxygen を、ここは「その下で何が動くか」を見る。

関連: [CLAUDE.md](../CLAUDE.md) / [COOKBOOK.md](COOKBOOK.md) §6.7（OS コールバック→状態機械の橋渡し）/
`arch/posix_MinGW/src/h/std2/ts_rio.h`（RIO ABI・地雷メモ）。

---

## 1. 実装マトリックス（縦=I/O API・横=OS）

`mingw-native` = 実 Windows（RIO 利用可）、`mingw-wine` = wine（RIO 不可→plain 自動降格）。
両者の差は **datagram の「リング排出（pump）」1軸のみ**。stream（read/write）は RIO 非関与で同一。

| I/O | linux | macos | cygwin | mingw-native | mingw-wine |
|---|---|---|---|---|---|
| **反応器（準備通知）** | `select()` ＋非ブロック fd ＋self-pipe wake | `select()`（arch/posix・kqueue 不使用） | `select()`（errno==0 spurious retry 対応） | IOCP：stream/plain-datagram=`CreateThreadpoolIo`、RIO=RIO_CQ＋`CreateThreadpoolWait`、wake/timer/signal/console=fwIO 自前 IOCP（`CreateIoCompletionPort`+`GetQueuedCompletionStatus`） | 同 native、ただし RIO_CQ 経路なし（datagram も `CreateThreadpoolIo`） |
| **read/write**（stream） | native `::read`/`::write`（O_NONBLOCK・EAGAIN→yield） | native `::read`/`::write` | native `::read`/`::write` | overlapped `ReadFile`/`WriteFile`（record-ring） | 同左（RIO 非関与・wine で動作） |
| **sendto/recvfrom**（datagram 単発） | native `::sendto`/`::recvfrom` | native `::sendto`/`::recvfrom` | native `::sendto`/`::recvfrom` | record-ring → pump が **RIO（`RIOSendEx`/`RIOReceiveEx`）既定**／`TS2_DISABLE_RIO` で plain `WSASendTo`/`WSARecvFrom` | record-ring → pump が **plain `WSASendTo`/`WSARecvFrom`**（RIO 自動降格） |
| **sendmsg/recvmsg**（msghdr scatter-gather ＝`sendmmsg`/`recvmmsg` を vlen=1 で公開） | `::sendmmsg`/`::recvmmsg`（vlen=1） | `::sendmsg_x`/`::recvmsg_x` | `::sendmsg`/`::recvmsg`（per-datagram） | userspace で iov gather → **RIO**（or plain） | userspace で iov gather → **plain `WSASendTo`/`WSARecvFrom`** |
| **sendmmsg/recvmmsg**（バッチ） | **native `::sendmmsg`/`::recvmmsg`**（1 syscall で束＝真バッチ） | **`::sendmsg_x`/`::recvmsg_x`**（Darwin 真バッチ） | `::sendmsg`/`::recvmsg` を **per-datagram ループ**（newlib に mmsg 無し） | record-ring に N 件 → **RIO を N 回**（keep-N-posted パイプライン） | record-ring に N 件 → **plain `WSASendTo`/`WSARecvFrom` を N 回**（単一 posted） |

### 読み方の注意

- **真のバッチ syscall があるのは linux / macos だけ**。cygwin / mingw は「mmsg の形」を
  **per-datagram** で埋めるフォールバック（Windows は RIO で並行度を稼ぐが、カーネルには 1 op ずつ）。
- **`sendmsg`/`recvmsg` という名前のメソッドは無い**が、`sendmmsg`/`recvmmsg` は `struct mmsghdr*`
  （中身は完全な `struct msghdr`）を取る公開メソッドなので、**vlen=1 で呼べばそれが公開の `sendmsg`**。
  非公開なのではなく mmsg に包含されているだけ。
- **MinGW の plain / RIO の差は 1 軸**：`sendto`/`recvfrom`/`sendmmsg`/`recvmmsg` の**呼び出し（API）は
  同一**（同じ record-ring へ produce/consume ＋ park）。違うのは**下流の pump が `WSASendTo` か
  `RIO` か**だけ。`read`/`write`（stream）には RIO は掛からない（datagram 専用）。

---

## 2. Windows datagram パスの設計（RIO 既定 ＋ 自動フォールバック）

### 2.1 なぜ RIO 既定か

RIO（Registered I/O, Windows 8+）は **登録バッファ＋ user-mapped の request/completion queue** で、
per-op のバッファ probe/lock を省き、**keep-N-posted のパイプライン**を張れる。実測（NucBox7・
loopback・UDP・後述）で **連続ストリームは plain の約 5 倍**（plain は record-ring が単一 posted で
直列化、RIO は N 個を同時 in-flight）。**1個ずつの request/response では RIO ≈ plain**（両方とも
完了往復レイテンシ律速）。よって datagram は RIO を既定にした。

以前あった生成フラグ `TS2IO_SOCKET_ENHANCED` と mode コンストラクタは撤去済（2026-07-21）。
**`ts2IOsockUDP` のインタフェースは全 OS 統一**（`(parent, port)` / `(parent, port, conn, len)`）。

### 2.2 フォールバック（plain overlapped）

RIO が使えない環境では **plain overlapped（`WSASendTo`/`WSARecvFrom` ＋ `CreateThreadpoolIo`）へ
自動降格**する。降格条件は次の 3 つ:

1. **Windows 7 / Server 2008R2**（RIO 無し）：`WSASocketW(WSA_FLAG_REGISTERED_IO)` が失敗 →
   生成時に plain ソケット。
2. **wine**（RIO ioctl 未実装）：`WSASocketW` は成功するが、初回 I/O の `ensure_rio()` が RIO 関数
   テーブル取得に失敗 → **エラーにせず `return 1`（降格シグナル）** → 呼び出し側が `enhanced=0` に
   落として plain pump へ。**wine で UDP テストが plain 経路で回り続ける＝開発ゲートを温存**。
3. **環境変数 `TS2_DISABLE_RIO`**（値は任意・存在すれば有効）：明示的に plain を強制。用途＝
   plain との A/B 計測、多数ソケットで RIO 登録バッファの page-pin を避けたい場合、RIO 不具合時の
   安全弁（再コンパイル不要）。

`enhanced` は**caller フラグではなくランタイム検出結果**（`ts2IOsockUDP` INI で 1 を仮置き、
初回 I/O で確定）。`WSA_FLAG_REGISTERED_IO` で作ったソケットは plain overlapped I/O も動くので
降格は安全。実装: `src/classes/ts2/c++/ts2IOsockUDP.cpp`（INI）/ `arch/posix_MinGW/.../ts2IOsocket.cpp`
（`ensure_rio` の戻り値 0=ok / 1=RIO 不可（降格）/ -1=エラー、4 つの I/O メソッドが降格を処理）。

### 2.3 完了配送（RIO）

RIO 完了は **RIO_CQ を RIO_EVENT_COMPLETION（auto-reset EVENT）で signal → `CreateThreadpoolWait`
（モダンスレッドプール・base の `CreateThreadpoolIo` と同系）→ callback で `RIODequeueCompletion`
→ ring 更新 → `RIONotify` 再武装 → `wakeup()`**。`wakeup()` は PQCS でなく **その場で `eventHandler`
をインライン実行**（app-mtx 直列化）なので、再 post も reader 起こしもプールスレッド上で完結し、
fwIO リアクタは datagram 完了経路に入らない。

> 【計測知見】完了通知を legacy `RegisterWaitForSingleObject`（旧プール）→ modern
> `CreateThreadpoolWait`（新プール）に替えると **スループット約 2 倍**（38k→77k pkt/s）。旧プールの
> 通知ディスパッチレイテンシが律速だった。busy-poll（専用スレッドで `RIODequeueCompletion` 空回し）も
> 試したが利得なし（~77k で同等）→ 撤去。残る床は per-datagram 処理／loopback 結合。

### 2.4 寿命 / teardown

- RIO の受信は **keep-N-posted（refio 非保持）**。socket の reactor keep-alive は「active refio 1 個」で、
  `rio_touch()`（各 I/O）で取得＋idle 期限更新、`rio_idle_check()`（pump 内）が「reader/writer 未 park
  かつ idle 経過」で **refio を解放（＝クローズでなく手放し）**。既定 idle は 2ms（`set_rio_idle_us`）。
- destroy / refcount→0 で FIN 連鎖 → `rio_teardown()`（`WaitForThreadpoolWaitCallbacks` で callback を
  drain → `RIODeregisterBuffer` → `RIOCloseCompletionQueue` → `CloseHandle`）を TS_THREAD で。
  never-called の RIO socket は何も建てず auto-destruct（plain 同様）。プロセス終了時は OS が一括回収。

### 2.5 チューニングノブ（Windows RIO のみ・POSIX は保存 no-op）

- **`set_rio_ring_slots(int)`**：RIO リングのスロット数＝posted 受信の上限（既定 4、上限 256）。
  深くすると 1 通知あたりの datagram 数が増え通知コストが償却される。バーストロス緩和にも効く。
- **`set_rio_recv_depth(int)`**：同時 post する受信数の cap（0=リング段数）。
- **`set_rio_idle_us(int)`**：idle keep-alive タイムアウト（µs）。

---

## 3. 実測サマリ（NucBox7 / Windows 11・loopback・UDP）

計測ハーネス `example/riobench`（srv/cli 別プロセス・mmsg blast＋END sentinel）。

- **バーストロス（keep-N-posted の効果）**：RIO 受信 depth 1/2/3/4 = **74.7 / 49.5 / 21.7 / 0.03 %loss**。
  リング深さ 16-32 で loss→0（完了→再 post のドロップ窓を閉じる）。
- **スループット**：既定 RIO **~81k pkt/s** / `TS2_DISABLE_RIO`=plain **~11k pkt/s**（連続ストリーム）。
  mmsg ベクタ長（vbatch）4→32 で +7%（76→81k）で頭打ち。
- **サイズ依存**：64B 82k=42Mbps / 1KB 79k=648Mbps / 1400B 79k=882Mbps。**pps ほぼ一定**＝
  per-datagram 律速で、datagram を大きくするほど帯域（bytes/s）が伸びる。

### なぜ Windows は Linux より遅いか

**同一 `riobench` を Linux native で回すと ~1.2-1.5M pkt/s**（16 倍超）。マシン速度差ではなく**経路差**：
Linux `recvmmsg` は 1 syscall で束を取る**真バッチ**、Windows は**バッチ syscall 不在**で per-datagram
（RIO でも 1 op ずつカーネルを通る）。よって ~80k は帯域でなく **per-datagram 処理律速**。

RIO 本来の利得（登録バッファで per-packet CPU 削減）は **実 NIC 高レート／多接続**で顕在化する。
単一 loopback フローは per-datagram 律速で mmsg 軸の伸び代は小さい。loopback をさらに押すなら
mmsg でなく **per-datagram 処理の削減**（ゼロコピー受信・粗粒度ロック・再 post バッチ）＝別レイヤ。

---

## 4. 参照

- 生成フラグ撤去・RIO 既定化。modern 通知＋計測ノブ追加。スループット計測ハーネス追加。
- RIO ABI と地雷（RIO_BUF addr 長=128・未 post 時 drop・RIONotify 二重武装）：`arch/posix_MinGW/src/h/std2/ts_rio.h`。
- OS コールバック→状態機械の橋渡し：[COOKBOOK.md](COOKBOOK.md) §6.7。
