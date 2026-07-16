
# tinyState — ビルド内部構造

> v0 草案 (2026-06-05)

<hr>

## 1. ディレクトリ構成

```
tinyState/
├── src/
│   ├── classes/<domain>/c++/Foo.cpp   ← ソース（実装 + TS_BEGIN_IMPLEMENT）
│   └── h/<domain>/c++/Foo.h           ← 公開ヘッダ（薄いラッパ）
├── CMakeLists.txt                     ← ルートの CMake エントリポイント
├── cmake/
│   ├── AddTinyStateLibrary.cmake
│   ├── AddTinyStateExample.cmake
│   ├── tinyStateConfig.cmake
│   ├── toolchain-mingw.cmake
│   └── config.h.in
├── ts2/                               ← ts2 ライブラリの CMake サブツリー
├── math2/                             ← math2 ライブラリの CMake サブツリー
├── utils/
│   └── tscpp2                         ← コードジェネレータ (Perl スクリプト)
├── build/                             ← CMake ビルドディレクトリ
│   ├── _ts2/c++/
│   │   ├── Foo_.h                     ← tscpp2 生成: 実装ヘッダ（非公開）
│   │   └── Foo_pb.h                   ← tscpp2 生成: 公開インタフェースヘッダ
│   ├── stamps/                        ← tscpp2 の再実行判定用 stamp ファイル
│   └── lib/                           ← コンパイル済みアーカイブ (.a)
├── docs/
├── Doxyfile
└── CLAUDE.md
```

<hr>

## 2. CMake ビルド手順

```sh
cmake -B build .
cmake --build build
```

（リポジトリ root で実行する。root が source dir。）

デフォルトビルドタイプは `Debug`。Release ビルドは:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

codegen のみ実行（コンパイルせず）:

```sh
cmake --build build --target tinyState2_codegen
```

<hr>

## 3. ビルドフロー

```
[フェーズ 1: コード生成]
src/classes/Foo.cpp  →  tscpp2  →  build/_ts2/c++/Foo_.h
                                    build/_ts2/c++/Foo_pb.h

[フェーズ 2: コンパイル]
src/classes/Foo.cpp  +  build/_ts2/c++/  →  コンパイル  →  build/lib/*.a
```

フェーズ 1 が完了してから (= 生成ヘッダが揃ってから) フェーズ 2 が始まる。
CMake では `<lib>_codegen` ターゲットが 1 を担い、ライブラリターゲットが 2 を担う。

<hr>

## 4. tscpp2 — コードジェネレータ

`utils/tscpp2` は Perl スクリプト。各 `.cpp` ファイルを解析して 2 つのヘッダを生成する。

### 生成物の役割

| ファイル | 内容 | インクルード元 |
|---|---|---|
| `Foo_.h` | 実装クラス `Foo_` の定義、状態名マクロ | `Foo.cpp` のみ |
| `Foo_pb.h` | 公開クラス `Foo` の定義（Doxygen コメント注入済み） | `src/h/.../Foo.h` 経由で外部公開 |

### CMake からの呼び出し方 (`AddTinyStateLibrary.cmake`)

```cmake
add_custom_command(
    OUTPUT ${stamp_file}
    COMMAND ${STLCPP}
        file
        ${src}
        --baseheader=${CMAKE_BINARY_DIR}
        --header=_ts2
    ...
)
```

`--baseheader` と `--header` の組み合わせが出力パスを決める。

```
出力パス = ${--baseheader}/${--header}/c++/<classname>_pb.h
         = build/_ts2/c++/Foo_pb.h
```

`STLCPP` は `CMakeLists.txt` で `utils/tscpp2` に設定されている。

### 手動実行

```sh
tscpp2 file src/classes/<domain>/c++/Foo.cpp \
    --baseheader=build \
    --header=_ts2
```

**注意**: `--baseheader` は絶対パスまたは CMake 実行ディレクトリからの相対パス。
`--baseheader=ts2/c++` のように短縮すると誤ったパスに出力される。

### tscpp2 が解析する主なブロック

```
#if 0
TS_BEGIN_IMPLEMENT
    // ← ここを scan_implement / scan_doc_comments が読む
    // 内部クラス Foo_ の宣言、TS_DEFARGS など
TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
    // ← interface block: Foo_pb.h の #include 群
TS_END_INTERFACE
#endif
```

`TS_BEGIN_IMPLEMENT...TS_END_IMPLEMENT` のコンテンツから:
- `scan_implement` → 関数一覧・引数型を抽出 → `_pb.h` の宣言生成に使う
- `scan_doc_comments` → `/** */` コメントを抽出 → 対応宣言の直前に注入

Doxygen コメントの書き方は `docs/DOXYGEN.md` 参照。

### `TS_DEFARGS` マクロ

コンストラクタ引数をメンバ変数として保存するボイラープレートを自動展開する。

```cpp
class TS_THISCLASS : public TS_BASECLASS {
public:
    Foo_(sPtr<tinyState> parent, int type = -1, FooFn fn = {});
    int org_type;
    FooFn org;
    TS_DEFARGS   // ← tscpp2 が "int org_type; FooFn org;" に展開
};
```

コンストラクタの引数名と同名のメンバ変数宣言を自動挿入する。

<hr>

## 5. ヘッダのインクルード経路

```
#include "ts2/c++/Foo.h"          ← アプリが include するエントリポイント
        ↓
src/h/ts2/c++/Foo.h
        ↓ #include "_ts2/c++/Foo_pb.h"
build/_ts2/c++/Foo_pb.h  ← tscpp2 生成、公開インタフェース
```

`src/h/` は `target_include_directories` の `PUBLIC` に、
`build/` は `PRIVATE` に含まれている。
外部ライブラリ利用側は `src/h/` のみが見える。

<hr>

## 6. Doxygen ビルドとデプロイ

Doxyfile の主要設定:

| 項目 | 値 |
|---|---|
| `INPUT` | `src/h`, `build/_ts2`, `docs`, `CLAUDE.md` |
| `OUTPUT_DIRECTORY` | `build/doxygen` |
| `EXCLUDE_PATTERNS` | `*/_ts2/*/*_.h` (実装ヘッダを除外、`_pb.h` は含む) |

ビルド:

```sh
doxygen Doxyfile
# → build/doxygen/html/ に生成
```

デプロイ（**メンテナ向け・GLOBALBASE 内部運用**。一般利用者には不要——ローカルビルドで完結する）:

```sh
doxygen Doxyfile && \
rsync -avz --delete build/doxygen/html/ \
    globalbaseProject:/home/git/public/releases/tinyState/
```

deploy 先 SSH alias `globalbaseProject` はメンテナの `~/.ssh/config` に定義。
Doxygen コメントの書き方は `docs/DOXYGEN.md` 参照。

<hr>

## 7. Windows 移植の開発ビルド（クロス + wine）

> これは **開発 / CI 用の内部フロー**。エンドユーザ向けの正規インストール手順は
> [INSTALL.md](INSTALL.md)（Windows 上で MSYS2 / Cygwin でセルフホストビルド）を参照。
> ここに書くのは「Linux ホストで MinGW クロスコンパイルし wine で実行する」開発者用の
> 近道で、実機を毎回使わずに移植を回すためのもの。

### ツールチェーン

- クロスコンパイラ: `x86_64-w64-mingw32-g++-posix`（POSIX スレッドモデル = winpthreads）
- toolchain ファイル: `cmake/toolchain-mingw.cmake`（`CMAKE_SYSTEM_NAME=Windows`）

### フレームワークをクロスビルド

```sh
cmake -B build-mg . \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-mingw.cmake
cmake --build build-mg -j
cmake --install build-mg --prefix /tmp/ts_mg        # 消費用の staging prefix
```

> `arch/*` を編集したら **必ず再 configure** すること（overlay は hardlink で、
> ソースを編集すると inode が変わりリンクが腐る）。

### example をクロスビルドして wine 実行

正規の `find_package` フローがクロスでも通る（`WIN32` は toolchain 側で真になる）:

```sh
cmake -S example/socktest -B /tmp/socktest_mg \
      -DCMAKE_TOOLCHAIN_FILE=$PWD/cmake/toolchain-mingw.cmake \
      -DCMAKE_PREFIX_PATH=/tmp/ts_mg
cmake --build /tmp/socktest_mg -j
WINEPREFIX=$HOME/.wine-ts WINEDEBUG=-all wine /tmp/socktest_mg/socktest.exe
```

### 注意点

- **wine は互換レイヤ**。状態機械の並行バグを隠す一方、存在しない終了時 crash を
  見せることがある（IOCP プロセス破棄の wine 内部バグ）。**並行性・終了処理の
  最終検証は実機必須**。
- `/tmp/build_*.sh` に手書きのクロスビルドスクリプトが残っている場合があるが、
  正規は上記 CMake フロー。スクリプトは揮発的な dev 補助。

<hr>

## 8. バージョニング & リリース

### 単一ソースは `project(VERSION)`

バージョン番号の正は **トップ `CMakeLists.txt` の `project()` だけ**。他の場所には手で書かない。

```cmake
project(tinyState VERSION 2.0.0 LANGUAGES C CXX)
```

ここから 2 系統に流れる:

1. **`tinyState_config.h`**（`cmake/config.h.in` を `configure_file` で生成、`include/std2/` に install）
   - `TS_VERSION` … `"2.0.0"`（文字列）
   - `TS_VERSION_MAJOR` / `_MINOR` / `_PATCH` … `2` / `0` / `0`（整数、条件コンパイル用）
   - `TS_REVISION` … git 由来リビジョン（下記）
2. **CMake パッケージ版数ファイル** `tinyStateConfigVersion.cmake`（`write_basic_package_version_file`、`COMPATIBILITY SameMajorVersion`）
   - これで消費側が `find_package(tinyState 2.0 REQUIRED)` と版数指定でき、`2.x` は満たすが `3.x` は弾く

### `TS_REVISION` — git provenance（`TS_VERSION` とは別物）

`TS_VERSION` が「人間が宣言する semver」なのに対し、`TS_REVISION` は **正確な出所**。configure 時に取得する:

```cmake
git describe --tags --always --dirty --long
#   例: v2.0.0-3-gabc1234        (タグから3コミット進んだ地点)
#       v2.0.0-0-gabc1234        (タグ commit ちょうど)
#       abc1234-dirty            (タグ皆無/未コミット変更あり)
#       unknown                  (git チェックアウト外 = リリース tarball)
```

- **configure 時のスナップショット**。`cmake -B build .` 以降のコミット/編集は、**再 configure するまで反映されない**。
- 旧 SVN 前提の `utils/version.pl` はこの仕組みに置き換えて削除済み。

### タグ運用（プレリリース rc 方式）

タグは「リリース地点を固定する点」。ブランチ（開発の線）とは独立で、`git describe` は HEAD の祖先を辿って最寄りの annotated タグを拾う。

```sh
# rc を刻む → 確定
git tag -a v2.0.0-rc1 -m "tinyState 2.0.0 release candidate 1"
git push origin v2.0.0-rc1          # タグは通常 push で飛ばない。明示が要る
# ... 検証 ...
git tag -a v2.0.0     -m "tinyState 2.0.0"
git push origin v2.0.0

# タグを打ったら TS_REVISION を更新（再 configure で git describe を取り直す）
cmake -B build .
#   → build/tinyState_config.h の TS_REVISION が "v2.0.0-0-g...." に変わる
```

- **annotated タグ（`-a`）を使う**。`git describe` はデフォルト annotated しか見ない（`--tags` で lightweight も拾えるが、リリースには annotated が定石）。
- タグ名は慣習で先頭 `v`（`v2.0.0`）。ただし `TS_REVISION` は describe の出力そのままなので `"v..."` と `v` 付きになる。`TS_VERSION`（`v` なし）と揃えたければタグも `v` なしにする——どちらでも動く。
- **打った点を後から動かさない**。公開後の付け替えは取得済みの相手と食い違う。基本は打ち直しでなく次版（`v2.0.1` 等）へ進める。
- feature ブランチの途中コミットに打ち、後で squash/rebase merge するとその commit が消えてタグが**迷子**になる。恒久履歴に残る commit（merge/ff で取り込んだ側）に打つこと。

### Doxygen への反映

`tinyState_config.h` は Doxyfile の `INPUT` に入っており、`TS_VERSION` / `TS_REVISION` はマクロとして doc 化される（`tinyState_config.h` ページ + マクロ索引）。タグ後に HP を更新するなら、再 configure → `doxygen Doxyfile` → §6 の rsync deploy の順。
