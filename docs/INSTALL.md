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
| `lib/libtinyState2.a`, `libtinyState2Math.a` | 静的ライブラリ |
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

## 6. 開発者向けクロスビルド

日常の移植開発では Linux ホスト上で MinGW クロスコンパイルし wine で実行する
（実機を毎回使わずに済む）フローも使える。これは **公開インストール経路ではなく
開発/CI 用**。詳細は [BUILD_INTERNAL.md](BUILD_INTERNAL.md) §7 を参照。
