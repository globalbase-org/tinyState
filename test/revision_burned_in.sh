#!/bin/sh
# 版の文字列が **成果物に焼き込まれている** こと、かつ **全 TU が引くヘッダには
# 入っていない** ことを検査する。
#
# なぜ 2 つを 1 本で見るか: この 2 つは**同じ変更の裏表**で、片方だけ戻すと静かに
# 劣化する。
#   ・版がヘッダ (std2/tinyState_config.h) に戻ると、install するたびに mtime だけで
#     下流の全 TU が焼き直る (利用側の実測で 884 TU ・ Linux 約 6.5 分 ・
#     Windows(MSYS2) は建て直し約 60 分 + フル検定約 42 分)。
#   ・版を専用の TU に移すと、**誰からも参照されないので静的リンクで .o ごと落ちる**。
#     「.a に在る」は「exe に入った」を意味しない (example/whole-archive-test と同じ盲点)。
#     だから ts2_revision() は **常に引かれる TU (tinyState.cpp)** に居なければならない。
#
# 正本: docs/BUILD_INTERNAL.md §8
#
# 使い方: revision_burned_in.sh <nm> <lib> <config_h> <revision_h> <workdir>
#
# ⚠ 検査自身が「何も測れていない」まま緑になる事故を避けるため、**毎回較正してから測る**。
#   較正に失敗したらその場で落ちる (0 件を報告する前に、当たる例を 1 つ通す)。
#   ★ 道具が在ることから始める: box の MINGW64 の既定 PATH には nm も strings も無く、
#     版マーカーの数が全部 0 に見えた実例がある。`strings` は使わない (box に無い)。

NM="$1"; LIB="$2"; CFG="$3"; REVH="$4"; WORK="$5"
[ -n "$NM" ] && [ -n "$LIB" ] && [ -n "$CFG" ] && [ -n "$REVH" ] && [ -n "$WORK" ] || {
	echo "FAIL usage: $0 <nm> <lib> <config_h> <revision_h> <workdir>"; exit 2; }
for f in "$LIB" "$CFG" "$REVH"; do
	[ -f "$f" ] || { echo "FAIL not found: $f"; exit 2; }
done
mkdir -p "$WORK" || exit 2

# ---- 道具が在るか (較正より手前) ----------------------------------------
"$NM" --version >/dev/null 2>&1 || { echo "FAIL tool not usable: $NM"; exit 2; }

# ---- 版の文字列を取り出す -----------------------------------------------
REV=`sed -n 's/^#define[ 	][ 	]*TS_REVISION[ 	][ 	]*"\(.*\)"[ 	]*$/\1/p' "$REVH"`
[ -n "$REV" ] || { echo "FAIL could not read TS_REVISION from $REVH"; exit 2; }
echo "revision = [$REV]"

# ---- 較正 1: この版文字列で grep が当たること ---------------------------
# (当たらない道具/文字化けで「config.h に無い」が自明に真になるのを防ぐ)
grep -F "$REV" "$REVH" >/dev/null 2>&1 || {
	echo "FAIL calibration: grep cannot find [$REV] even in $REVH"; exit 2; }

# ---- 検査 1: 全 TU が引く config ヘッダに版が入っていないこと -----------
if grep -F "$REV" "$CFG" >/dev/null 2>&1; then
	echo "FAIL the revision string is back in $CFG"
	echo "     -> install するたびに下流の全 TU が mtime で焼き直る。"
	echo "        版は std2/tinyState_revision.h に置くこと (docs/BUILD_INTERNAL.md §8)。"
	exit 1
fi
echo "ok   the revision is not in the universally-included config header"

# ---- 較正 2: nm の問い方が効くこと -------------------------------------
"$NM" -A "$LIB" > "$WORK/syms.txt" 2>/dev/null || {
	echo "FAIL could not read symbols from $LIB with $NM"; exit 2; }
[ -s "$WORK/syms.txt" ] || { echo "FAIL no symbols read from $LIB"; exit 2; }
# 在るはずの無い名前では 0 件になること (= 何にでも当たる問い方ではない)
if grep 'ts2_revision_this_symbol_does_not_exist' "$WORK/syms.txt" >/dev/null 2>&1; then
	echo "FAIL calibration: the symbol query matches a name that cannot exist"; exit 2
fi

# ---- 検査 2: ts2_revision() が定義されていること ------------------------
# マングル名は ABI 依存なので素の名前で照合する (demangle を要求しない)。
grep 'ts2_revision' "$WORK/syms.txt" > "$WORK/hit.txt" 2>/dev/null
[ -s "$WORK/hit.txt" ] || {
	echo "FAIL ts2_revision() is not in $LIB"
	echo "     -> 版の文字列が成果物から消えている。ts2/c++/ts2Revision.h と"
	echo "        src/classes/ts2/c++/tinyState.cpp を見ること。"
	exit 1; }
echo "ok   ts2_revision() is defined in the library"

# ---- 検査 3: それが「常に引かれる TU」に居ること ------------------------
# 静的ライブラリのときだけ意味がある (共有では .so 全体が 1 つの像)。
case "$LIB" in
*.a)
	# nm -A の出力は "<lib>:<member>:<addr> T <sym>" なので、TU は末尾から 2 つ目。
	TU=`awk -F: '/ [TtRrDdBb] /{print $(NF-1)}' "$WORK/hit.txt" | sort -u | head -1`
	case "$TU" in
	# ★ 拡張子は道具で違う: POSIX の ninja/make は .o ・ **MinGW は .obj**。
	#   .o だけで照合して MinGW で**偽の赤**を出した (box 実機で捕まえた)。
	*tinyState.cpp.o|*tinyState.cpp.obj)
		echo "ok   it lives in an always-linked TU ($TU)" ;;
	"")
		echo "FAIL could not tell which TU defines ts2_revision (nm -A format?)"
		exit 2 ;;
	*)
		echo "FAIL ts2_revision() moved to [$TU]"
		echo "     -> 参照されない .o は静的リンクで落ちるので、消費側の exe に"
		echo "        版が入らなくなる。常に引かれる TU (tinyState.cpp) に戻すこと。"
		exit 1 ;;
	esac ;;
*)
	echo "skip TU locality check: $LIB is not a static archive (shared build)" ;;
esac

echo "PASS revision is burned into the artifact and out of the shared header"
exit 0
