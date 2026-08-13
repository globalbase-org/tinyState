# tinyState — インストール手順 (Linux / Windows)

> v0 (2026-07-15)

tinyState を **git clone した状態から自前でビルド＆インストール**する手順。
Linux と、Windows の 2 つのツールチェーン（**Cygwin** / **MSYS2 (MinGW-w64)**）を
同じ CMake フローで扱う。クロスコンパイルは不要——各環境上でセルフホストビルドする。

すべての環境で流れは同じ:

```
1. 依存を入れる               (g++ / cmake / make / perl)
2. フレームワークをビルド      cmake -B build . && cmake --build build
3. インストール                cmake --install build --prefix <PREFIX>
4. アプリ/example をビルド      cmake -S example/<name> -B ... -DCMAKE_PREFIX_PATH=<PREFIX>
```

`<PREFIX>` は任意の場所（例: `/usr/local`、`$HOME/tinystate`、`C:/tinystate`）。
インストール後の prefix は **relocatable**（丸ごと移動/コピー/scp しても動く）。

---

## 0. 何がインストールされるか

`cmake --install` は `<PREFIX>` 配下に:

| 場所 | 内容 |
|---|---|
| `include/` | 共通ヘッダ + OS 依存ヘッダ(arch overlay) + 生成ヘッダ `_ts2/` |
| `include/std2/tinyState_config.h` | ビルド構成ヘッダ（`TS_VERSION` / `TS_REVISION` 等。→ [BUILD_INTERNAL §8](BUILD_INTERNAL.md)） |
| `lib/libtinyState2.a`, `libtinyState2Math.a` | 静的ライブラリ（`TINYSTATE_BUILD_SHARED=ON` なら代わりに `.so` / `.dylib` / `.dll`。→ [§5-1](#5-1-ビルドオプション)） |
| `bin/tscpp2` ほか | コードジェネレータ (Perl) |
| `lib/cmake/tinyState/` | `find_package(tinyState)` 用パッケージ設定（版数ファイル込みで `find_package(tinyState 2.0)` の版数指定も可） |

アプリ側は `find_package(tinyState REQUIRED)` + `add_tinystate_example()` で
これを消費する（プラットフォーム別のリンク——Windows の winsock、Linux の
pthread/rt——は自動で付く）。

---

## 1. Linux (リファレンス)

### 依存 (Debian / Ubuntu 系)

```sh
sudo apt install build-essential cmake perl
```

（Red Hat 系なら `gcc-c++ cmake make perl`。curl/openssl 依存は無い。）

### ビルド & インストール

```sh
cmake -B build .
cmake --build build -j
sudo cmake --install build                 # → /usr/local
#   もしくは prefix を指定:
# cmake --install build --prefix $HOME/tinystate
```

### example をビルド & 実行

```sh
cmake -S example/hello-world -B example/hello-world/build     # /usr/local なら PREFIX 指定不要
cmake --build example/hello-world/build -j
./example/hello-world/build/hello-world
```

prefix を `/usr/local` 以外にした場合は
`-DCMAKE_PREFIX_PATH=$HOME/tinystate` を configure に付ける。

---

## 2. Windows — Cygwin (native)

Cygwin は POSIX 互換レイヤ。tinyState は Cygwin 上ではほぼ Linux と同じ経路で
ビルドできる（`cygwin1.dll` に依存する PE を生成）。

### 依存 (Cygwin `setup-x86_64.exe` で選択)

```
gcc-g++   cmake   make   perl
```

コマンドラインからは:

```sh
setup-x86_64.exe -q -P gcc-g++,cmake,make,perl
```

### ビルド & インストール（Cygwin ターミナルで）

```sh
cmake -B build .
cmake --build build -j
cmake --install build --prefix /usr/local
```

### example

```sh
cmake -S example/socktest -B example/socktest/build -DCMAKE_PREFIX_PATH=/usr/local
cmake --build example/socktest/build -j
./example/socktest/build/socktest
```

> Cygwin は `WIN32` ではなく POSIX として検出される（`CYGWIN`）。arch overlay は
> `posix_Cygwin` 層が選ばれ、リンクは pthread ベース（winsock ではない）。

---

## 3. Windows — MSYS2 / MinGW-w64 (native PE)

MSYS2 の MinGW-w64 は **cygwin1.dll に依存しない素のネイティブ PE** を生成する
（配布向け）。**必ず「MSYS2 MINGW64」シェル**を使うこと（"MSYS" シェルではない）——
これで CMake が `WIN32` を検出し、IOCP ベースの Windows 実装 (arch overlay =
`posix_MinGW`) が選ばれる。

### 依存 (MINGW64 シェルで pacman)

```sh
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  perl
```

> `perl` は tscpp2（コードジェネレータ）に必須。ninja の代わりに
> `mingw-w64-x86_64-make` + `-G "MinGW Makefiles"` でもよい。

### ビルド & インストール（MINGW64 シェルで）

```sh
cmake -G Ninja -B build .
cmake --build build
cmake --install build --prefix /mingw64        # または $HOME/tinystate 等
```

> `/mingw64` は MSYS2 の MinGW64 prefix。そこに入れると PATH に載るので tscpp2 も
> そのまま使える。別 prefix にした場合は example configure に
> `-DCMAKE_PREFIX_PATH=<prefix>` を、実行前に `<prefix>/bin` を PATH に追加。

### example

```sh
cmake -G Ninja -S example/socktest -B example/socktest/build -DCMAKE_PREFIX_PATH=/mingw64
cmake --build example/socktest/build
./example/socktest/build/socktest.exe
```

生成物は `-static -static-libgcc -static-libstdc++` でリンクされるため、
`.exe` 単体を MinGW/MSYS2 の無い Windows へコピーしても動く（実行時 DLL 不要）。

---

## 4. example 一覧

| example | 内容 |
|---|---|
| `hello-world` | 最小の状態機械 |
| `parallel-demo` / `semaphore-demo` / `rwlock-demo` | tsThread / セマフォ / RW ロック |
| `interval-timer` | interval タイマ + 並行 π 計算（Ctrl+C で終了） |
| `naming-conventions` | 命名接頭辞（ss/std/ts）のデモ |
| `socktest` / `udptest` / `unixtest` | TCP / UDP / AF_UNIX ループバック |
| `nettest` | マシン間 TCP（`nettest server <port>` / `nettest client <host> <port>`） |
| `srvtest` | サーバ階層（`srvtest tcp <port>` / `srvtest unix <path>`） |
| `mmsgtest` | recvmmsg/sendmmsg スループット |
| `systest` | ts2System 子プロセス生成 |

各 example は `cmake -S example/<name> -B <build> -DCMAKE_PREFIX_PATH=<PREFIX>` で
個別にビルドできる。

---

## 5. 自作アプリの CMakeLists

example と同じ 3 行で済む:

```cmake
cmake_minimum_required(VERSION 3.16)
project(myapp LANGUAGES CXX)
find_package(tinyState REQUIRED)

add_tinystate_example(NAME myapp
  SOURCES
    src/main/main.cpp
    src/classes/hw/c++/hwMyClass.cpp)   # /classes/ 配下は tscpp2 codegen 対象
```

`add_tinystate_example` は `SOURCES` のうち `/classes/` を含むパスを tscpp2 で
コード生成し、残り（`main.cpp` 等）はそのままコンパイルする。コンパイルフラグ
（`-std=gnu++2a` 等）・include・リンクは `tinyState::tinyState2` から継承される。

---

## 5-1. ビルドオプション

| オプション | 既定 | 内容 |
|---|---|---|
| `TINYSTATE_BUILD_PIC` | `ON` | ライブラリを PIC (`-fPIC`) でビルドする。`.a` のままだが、consumer が共有ライブラリ (dlopen されるモジュール等) へ埋め込める。非 PIC だと ELF 上で `R_X86_64_32` / `TPOFF32` の再配置が残り `-shared` リンクが失敗する。x86-64 での実行時コストは実質無視できる |
| `TINYSTATE_BUILD_SHARED` | `OFF` | ライブラリを共有ライブラリ (`.so` / `.dylib` / `.dll`) としてビルドする。`OFF` なら従来どおり静的 `.a` |

```sh
cmake -B build -DTINYSTATE_BUILD_SHARED=ON .
```

### 共有ライブラリビルドの注意

* `libtinyState2Math` は **GMP / MPFR に実リンク**する。静的ビルドではこれらの
  シンボルは未解決のまま `.a` に残り、アプリ側が解決していた。共有ビルドでは
  ライブラリ自身が `DT_NEEDED` を持つので、**ビルドマシンに開発パッケージが必要**
  (Debian: `libgmp-dev` `libmpfr-dev` / Homebrew: `gmp` `mpfr` /
  MSYS2: `mingw-w64-x86_64-gmp` `mingw-w64-x86_64-mpfr` / Cygwin: `libgmp-devel` `libmpfr-devel`)。
  ただし **できた `.dylib` / `.so` を使わなければ実行時に GMP/MPFR は要らない**。
* **macOS では universal ビルドにできない**。本プロジェクトは既定で
  `arm64;x86_64` を作るが、Homebrew の GMP/MPFR は単一アーキテクチャなので
  x86_64 スライスがリンクできない。共有ビルド時は明示的に単一アーキを指定する:

  ```sh
  cmake -B build -DTINYSTATE_BUILD_SHARED=ON -DCMAKE_OSX_ARCHITECTURES=arm64 .
  ```

  静的ビルドはリンクが起きないので従来どおり universal のままでよい。

### `find_package` から共有版を選ぶ

パッケージ設定は **実際に install されているものを見て**ターゲットを決めるので、
「静的だけ」「共有だけ」「両方」のどの install でも `find_package(tinyState)` は成立する。

| ターゲット | 指すもの |
|---|---|
| `tinyState::tinyState2` | 静的があれば静的、無ければ共有（**既定はこれまでどおり静的**） |
| `tinyState::tinyState2Shared` | 共有（共有が install されている場合のみ定義される） |
| `tinyState::tinyState2Math` / `…MathShared` | 同上 |

両方 install した prefix で共有版を明示的に使いたい場合:

```cmake
find_package(tinyState REQUIRED)
target_link_libraries(myapp PRIVATE tinyState::tinyState2Shared)
```

プロセス内で tinyState の実体と静的状態を確実に 1 組にしたい構成（複数の `.so` が
それぞれ tinyState を内包すると実体が複数になる）では、共有版を選ぶ意味がある。

### whole-archive 検証

`.a` は参照を解決しないため、どの `.o` も引き込まれない限り未定義参照は表に
出ない。これを検出するターゲットが `example/whole-archive-test` で、ライブラリを
全取り込み (`--whole-archive` / `-force_load`) して共有ライブラリを作る。
通常ビルドには含まれないので明示的に指定する:

```sh
cmake --build build --target whole_archive_test_so
```

ビルドが通ること自体が検証結果。tinyState 本体を丸ごと共有ライブラリへ
取り込みたい利用者は、まずこれが通ることを確認するとよい。

---

## 6. 開発者向けクロスビルド

日常の移植開発では Linux ホスト上で MinGW クロスコンパイルし wine で実行する
（実機を毎回使わずに済む）フローも使える。これは **公開インストール経路ではなく
開発/CI 用**。詳細は [BUILD_INTERNAL.md](BUILD_INTERNAL.md) §7 を参照。
