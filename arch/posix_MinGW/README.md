# arch/posix_MinGW — MinGW (MSYS2 mingw-w64) overlay

Windows ネイティブ寄り (MinGW) 向けの arch overlay レイヤ。
`CMakeLists.txt` の `arch_layers` は WIN32 (かつ非 CYGWIN) で
このレイヤを base `posix` に重ねる。

設計・方針の出典: tinyState Windows 移植 (Cygwin / MinGW) 検討メモ §5, §7。

## このレイヤに置く予定のファイル (フェーズ順)

`src/{h,classes}/...` 配下に、base `posix` を上書きする形で以下を置く。
現時点ではまだ実ファイルを置いていない (受け皿のみ)。`src/` が無い間、
CMake の overlay ループはこのレイヤを安全に skip する。

| ファイル | 内容 | 出典 |
|---|---|---|
| `classes/ts2/c++/tsSignalCore.cpp` | no-op stub (Windows にシグナル無し) | §5.7 |
| `classes/ts2/c++/fwIOarch.cpp` | IOCP backend (arch 固有半分。共通部は `src/classes/ts2/c++/fwIO.cpp`) | §5.3〜5.5 |
| `classes/ts2/c++/ts2System.cpp` | fork/execvp → CreateProcess | §5.8 |
| `classes/ts2/c++/sObject.cpp` | openpty → 機能無効化 or ConPTY | §7 |
| `classes/ts2/c++/co_tsThreadKill.cpp` | pthread_kill → CancelSynchronousIo | §5.6 |
| `h/std2/m_include.h` | MinGW 用インクルード束ね (winsock2 等) | §4.2 |

## 進め方メモ

- 上位 (アプリ層) は無改変が原則。差し替えは fwIO 内部と arch overlay に閉じる。
- `io->read(state, SOCKET)` / `io->read(state, HANDLE)` の型オーバーロードは
  `#ifdef _WIN32` で共通ソース側に足す (§5.5)。overlay ではなく本体側。
- Cygwin overlay (`arch/posix_Cygwin`) は別系統。混同しない。
