
# tinyState — Doxygen コメントの書き方

> v0 草案 (2026-06-05)

<hr>

## 1. パイプライン概要

tinyState のクラス定義は tscpp2 を経由して公開ヘッダ (`_pb.h`) が自動生成される。
Doxygen はこの **生成ヘッダ** を読む。

```
src/classes/<domain>/c++/Foo.cpp
        ↓ tscpp2 (ビルド時 or 手動)
build/_ts2/c++/Foo_pb.h   ← Doxygen が読む
```

`Doxyfile` の `INPUT` に `build/_ts2` が含まれているため、
**Doxygen コメントは `_pb.h` に存在しなければ反映されない。**

`src/h/<domain>/c++/Foo.h` は `_ts2/c++/Foo_pb.h` を include するだけの
薄いラッパであり、ここにコメントを書いても Doxygen には届かない。

`_pb.h` は自動生成ファイルなので直接編集しない。
コメントは `.cpp` の `TS_BEGIN_IMPLEMENT` ブロック内に書く — tscpp2 が注入する。

<hr>

## 2. クラスレベルコメント

`TS_BEGIN_IMPLEMENT` 内の `class TS_THISCLASS` 直前に `/** */` ブロックを置く。

```cpp
#if 0
TS_BEGIN_IMPLEMENT

/**
 * @brief One-liner summary of the class.
 * @details
 * Longer description, usage examples, etc.
 *
 * @code{.cpp}
 * thNEW(Foo, (parent, args));
 * @endcode
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
    Foo_(sPtr<tinyState> parent);
    ...
};

TS_END_IMPLEMENT
#endif
```

tscpp2 の `scan_doc_comments` がこのコメントを `class_comment` として拾い、
`_pb.h` の生成クラス定義の直前に注入する。

<hr>

## 3. メソッドレベルコメント

クラス本体内の対象メソッド宣言の **直前** に `/** */` ブロックを置く。

```cpp
class TS_THISCLASS : public TS_BASECLASS {
public:
    Foo_(sPtr<tinyState> parent);
    /**
     * @brief Short description.
     * @details
     * Longer description.
     * @param type 0 = TS_STATE, 1 = TS_THREAD, -1 = inherit from root.
     */
    void spawn(int type = -1);
    /**
     * @brief Cancel all workers.
     * @details
     * Safe to call from any worker body.
     * Always return immediately after cancel().
     */
    void cancel();
};
```

tscpp2 は「`/** */` の直後に来る `identifier(` 行」でメソッド名を確定し、
`_pb.h` の対応する宣言の直前に挿入する。

### コメントと宣言の間に空行を挟まない

```cpp
/** @brief ok */
void spawn(int type = -1);      // ✓ 直前に置く

/** @brief このコメントは届かない */

void spawn(int type = -1);      // ✗ 空行があると pending がクリアされる
```

`scan_doc_comments` は空行 (`^\s*$`) を見ると pending を捨てずに無視するが、
空白でない行で `identifier(` パターンにマッチしない行が来ると pending がクリアされる。
安全のため **コメントと宣言は隣接させる**。

<hr>

## 4. コードブロック (@code / @endcode)

Doxygen の `@code{.cpp}` / `@endcode` はそのまま使える。
tscpp2 はコメントブロックを素通しするため、Doxygen 拡張タグはすべて有効。

```cpp
/**
 * @code{.cpp}
 * thNEW(Foo, (parent, 0,
 *     [](sPtr<Foo> me, sPtr<stdEvent> ev) -> int {
 *         return 1;
 *     }));
 * @endcode
 */
```

<hr>

## 5. tscpp2 の手動実行と動作確認

```sh
# リポジトリルートで
tscpp2 file src/classes/<domain>/c++/Foo.cpp \
    --baseheader=build \
    --header=_ts2
```

出力先: `build/_ts2/c++/Foo_pb.h`

その後 Doxygen を回す。

```sh
doxygen Doxyfile
```

ブラウザで `build/doxygen/html/index.html` を開いて確認。

CMake ビルドでも `<lib>_codegen` ターゲットが同じ引数で tscpp2 を呼ぶ
(詳細は `docs/BUILD_INTERNAL.md` 参照)。

<hr>

## 6. Doxygen Markdown の既知バグ

Doxygen の Markdown パーサに由来するバグで、このプロジェクトのドキュメントで実際に踏んだもの。

### blockquote の直後に水平線マーカーを書かない

```markdown
> 注釈文

---       ← ✗ </blockquote> がエスケープされてページに出力される
```

```markdown
> 注釈文

<hr>      ← ✓ HTML タグを直接使う
```

原因: `---` は Setext 見出し (`<h1>`) の Markdown 記法。
blockquote の直後に来ると、閉じタグ `</blockquote>` が本文に漏れる。

### 日本語に隣接する bold 記法は効かない

```markdown
**これは bold です**。         ← ✗ Doxygen の語境界判定が日本語を非サポート
<b>これは bold です</b>。      ← ✓
```

コード名を bold にするときも同様。

```markdown
**`destroy()`** を呼ぶ              ← ✗
<b><code>destroy()</code></b> を呼ぶ  ← ✓
```

英単語のみの場合は `**bold**` で動作する。

### 見出し (##) の中に backtick を書かない

```markdown
## `destroy()` を呼ぶと SIGPIPE が飛ぶ  ← ✗ <tt> が HTML エスケープされて出力
## destroy() を呼ぶと SIGPIPE が飛ぶ    ← ✓
```

backtick はインライン本文では `<code>` に変換されて正しく動くが、
見出しコンテキストでは `<tt>` への変換が二重エスケープされる。

<hr>

## 7. デプロイ

> **メンテナ向け（GLOBALBASE 内部運用）**。以下は公開 doc サイトへの反映手順で、
> GLOBALBASE のインフラ（SSH alias・deploy 先）に依存する。一般の利用者・
> contributor には不要——ローカルで `doxygen Doxyfile` を実行すれば
> `build/doxygen/html/` に HTML が生成される。

```sh
doxygen Doxyfile && \
sed -i 's/if (rootBase=="index" || rootBase=="pages" || rootBase=="search")/if (rootBase || n)/' \
    build/doxygen/html/navtree.js && \
rsync -avz --delete build/doxygen/html/ \
    globalbaseProject:/home/git/public/releases/tinyState/
```

deploy 先の SSH alias `globalbaseProject` はメンテナの `~/.ssh/config` に定義済み。

### sed パッチの理由

Doxygen 1.9.8 の `navtree.js` は `index.html`（メインページ）でのみサイドバーのツリーノードを
自動展開する。他のページでは選択ノードを展開しないため、セクションが折り畳まれたままになる。
sed で条件を書き換えて全ページで展開するようにしている。`doxygen Doxyfile` を実行するたびに
`navtree.js` が再生成されるため、常に sed パッチが必要。
