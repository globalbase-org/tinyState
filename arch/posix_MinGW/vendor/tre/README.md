# Vendored TRE (MinGW-only POSIX regex)

MinGW-w64 ships no POSIX `<regex.h>`. tinyState の `stdRx` / `stdString` は POSIX
C regex API (`regcomp`/`regexec`) を使うため、**MinGW ターゲットに限り** TRE を
bundle して供給する。Linux / macOS / Cygwin は system regex を使う (この dir は
不使用)。

## なぜ TRE か
- **本物の POSIX ERE**: leftmost-longest。旧 std::regex シム (leftmost-first) が
  取りこぼしていた意味論 (例 `a|ab` on `ab` → `ab`) を正しく返す。
- **線形時間マッチ** (バックトラック爆発 = ReDoS なし)。
- POSIX API を素で提供する **drop-in** (`stdRx` 無改修)。
- **BSD-2-clause** ライセンス (LICENSE 参照)。musl libc の regex も TRE 派生。

## 出所 / バージョン
- Upstream: TRE 0.9.0 (Ville Laurikari)。Debian `tre_0.9.0.orig.tar.gz` より。
- 構成: `configure --disable-approx --disable-wchar --disable-multibyte`
  (narrow char / POSIX byte regex のみ = stdRx の用途)。生成された `config.h` /
  `tre-config.h` を同梱。

## 同梱物
- `*.c` (11): lean POSIX matcher (approx / wchar 系は除外)。
- `*.h`: TRE 内部ヘッダ + 公開 `tre.h`。
- `config.h` / `tre-config.h`: MinGW 向け生成物。
- `regex.h`: POSIX 名を `tre_*` にマップする薄いラッパ (stdRx が `<regex.h>` で拾う)。

## ローカルパッチ
- `tre-internal.h` の `ALIGN` マクロ: `(long)ptr` → `(intptr_t)(ptr)`
  (Windows は LLP64 で `long`=32bit・ポインタ=64bit。切り詰め警告/整列バグを回避)。

## ビルド
CMake が `if(WIN32 AND NOT CYGWIN)` で `*.c` を OBJECT library `tre_mingw` として
`-DHAVE_CONFIG_H` 付きでコンパイルし、`tinyState2` に取り込む。詳細はルート
`CMakeLists.txt` の arch overlay 節直後を参照。

## 更新手順
upstream 更新時は同じ configure フラグで再生成し、`ALIGN` パッチを再適用する。
