#!/bin/sh
# tinyState2 が **静的デストラクタを 1 つも登録していない** ことを検査する。
#
# なぜ: 静的オブジェクトのデストラクタは構築の逆順で走るが、翻訳単位をまたぐ順序は
# 未規定。tinyState は静的同士に依存があり (tsSignalCore_::_signal_list の dtor が
# stdObject::relref() を呼び、それが stdObject::refMtx[] を取る)、順序が外れると
# 破棄済みのオブジェクトを触って 0 番地へ飛ぶ。abort() 経路ではローダの DLL detach が
# 直接デストラクタを走らせるので、アプリ側から順序に介入する手段が無い。
# ⇒ 残る手は「登録させない」だけ。新しい静的が増えたらここで落とす。
#
# 直し方: src/h/ts2/c++/sImmortal.h の sImmortal<T> / sImmortalArray<T,N> で包む。
#         それが無理なら test/no_static_dtor_allow.txt に **シンボル粒度**で登録し、
#         「なぜプロセスに 1 つでよいか」を書く (「消せない」ではなく)。
#
# 使い方: no_static_dtor.sh <nm> <cxx> <lib> <allowlist> <workdir>
#
# ⚠ 道具の差で黙って素通りする事故を避けるため、**毎回 canary で較正**してから測る。
#   grep -P (BSD grep に無い) と GNU 拡張の sed は使わない。
#   ★ 較正は「**道具が在ること**」から始める。box の MINGW64 の既定 PATH には nm も
#     strings も無く、版マーカーの数が全部 0 に見えた実例がある。
#     「コマンド不在 → 非 0 → 差がある」と読み違える形も同日に出ている。
#
# ⚠ **対象は __cxa_atexit / atexit / _onexit だけ**。`__cxa_thread_atexit`
#   (thread_local のデストラクタ登録) は **意図して見ていない**。あれはスレッド寿命の話で、
#   プロセス終了時の破棄順が未規定という本題とは別物。現に 1 本残っているが
#   (sCallSection の thread_local holder)、その経路は ~sCallSection の `list=0` と
#   sObject::operator delete の refLock (生の pthread_mutex_t = 破棄されない POD) だけで、
#   静的オブジェクトにも relref にも到達しない。**黙って除外せず、ここに書いておく。**

NM="$1"; CXX="$2"; LIB="$3"; ALLOW="$4"; WORK="$5"
[ -n "$NM" ] && [ -n "$CXX" ] && [ -n "$LIB" ] && [ -n "$WORK" ] || {
	echo "FAIL usage: $0 <nm> <cxx> <lib> <allowlist> <workdir>"; exit 2; }
[ -f "$LIB" ] || { echo "FAIL library not found: $LIB"; exit 2; }

mkdir -p "$WORK" || exit 2

# ---- 道具が在るか (較正より手前) ----------------------------------------
for tool in "$NM" "$CXX"; do
	"$tool" --version >/dev/null 2>&1 || {
		echo "FAIL tool not usable: $tool"
		echo "     ★ 不在のコマンドは非 0 を返すだけなので、検定の中で使うと"
		echo "       「測った結果」と区別がつかない。ここで先に落とす。"
		exit 2; }
done

# 登録シンボルは ABI で名前が違う。ELF=__cxa_atexit / MinGW=atexit / Mach-O=___cxa_atexit。
# ★ 「どれか 1 つでも当たれば可」にはしない。較正で **この環境で当たるべきものが
#    当たったか** を確かめてから本番を測る。
scan() {	# scan <objfile-or-archive> -> 登録している TU 名を 1 行ずつ
	# nm -A の 1 行は  <archive>:<member>:<sym>  または  <object>:<sym>。
	# ★ Windows のドライブ文字 (C:) でもフィールドが増えるだけなので、
	#   「最後から 2 番目のフィールド」を取るのが唯一正しい取り方。
	#   (sed で ':.*' を消す書き方だと archive のパスを拾ってしまう — 実際に踏んだ)
	"$NM" -A "$1" 2>/dev/null |
		grep -E ' U (___cxa_atexit|__cxa_atexit|atexit|_onexit)$' |
		awk -F: '{ print $(NF-1) }' |
		sort -u
}

# ---- 較正 ---------------------------------------------------------------
# 拾うべきもの / 拾ってはいけないものを両方置き、両側を検定する。
# 期待値は **仕様から** 書く (デストラクタを持つ静的は登録される / POD は登録されない)。
cat > "$WORK"/canary_hit.cpp <<'EOF'
struct thing_with_dtor { thing_with_dtor(); ~thing_with_dtor(); int x; };
thing_with_dtor::thing_with_dtor() { x = 1; }
thing_with_dtor::~thing_with_dtor() { x = 0; }
static thing_with_dtor must_hit_plain;
struct holder { static thing_with_dtor must_hit_member; };
thing_with_dtor holder::must_hit_member;
int canary_hit_use() { return must_hit_plain.x + holder::must_hit_member.x; }
EOF
cat > "$WORK"/canary_miss.cpp <<'EOF'
struct pod { int x; };
static pod no_hit_pod;
static int no_hit_int = 3;
struct trivial_dtor { trivial_dtor(); int x; };	/* ctor はあるが dtor は trivial */
trivial_dtor::trivial_dtor() { x = 2; }
static trivial_dtor no_hit_trivial;
int canary_miss_use() { return no_hit_pod.x + no_hit_int + no_hit_trivial.x; }
EOF

for c in hit miss; do
	"$CXX" -c "$WORK/canary_$c.cpp" -o "$WORK/canary_$c.o" 2>"$WORK/canary_$c.err" || {
		echo "FAIL calibration: could not compile canary_$c"; cat "$WORK/canary_$c.err"; exit 2; }
done

if [ -z "`scan "$WORK/canary_hit.o"`" ]; then
	echo "FAIL calibration: the detector did NOT flag canary_hit.o."
	echo "     この検査は何も測れていない (nm=$NM のシンボル名が想定と違う可能性)。"
	echo "     参考: `"$NM" "$WORK/canary_hit.o" 2>/dev/null | grep -i atexit`"
	exit 2
fi
if [ -n "`scan "$WORK/canary_miss.o"`" ]; then
	echo "FAIL calibration: the detector flagged canary_miss.o (偽陽性)."
	exit 2
fi
echo "calibration OK (hit を拾い / miss を拾わない ことを確認)"

# ---- 本番 ---------------------------------------------------------------
FOUND=`scan "$LIB"`
RC=0
for tu in $FOUND; do
	if [ -f "$ALLOW" ] && grep -q "^$tu\([ 	]\|$\)" "$ALLOW" 2>/dev/null; then
		echo "allowed: $tu"
		continue
	fi
	echo "FAIL $tu が静的デストラクタを登録しています (atexit)"
	RC=1
done

if [ $RC -eq 0 ]; then
	echo "OK tinyState2 は静的デストラクタを 1 つも登録していません"
else
	echo ""
	echo "直し方: その静的を sImmortal<T> / sImmortalArray<T,N> で包む"
	echo "        (src/h/ts2/c++/sImmortal.h)。包めない理由があるなら"
	echo "        $ALLOW に TU 名と **なぜプロセスに 1 つでよいか** を書く。"
fi
exit $RC
