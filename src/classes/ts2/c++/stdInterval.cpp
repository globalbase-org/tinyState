

#include 	<sys/time.h>
#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/stdFrameWork.h"
#include	"ts2/c++/stdInterval.h"

std::atomic<INTEGER64>
stdInterval::lastAccessTime;

/* 64bit atomic がロックフリーでないターゲットでは libstdc++ が内部のハッシュ
 * ロック表へ落とす。それは自前の mutex より悪いので、黙って遅くなるのではなく
 * ここで止めて気付けるようにする。対応機 (x86-64 / arm64) はいずれもロックフリー。
 * 仮にこれが鳴いたら、そのターゲットだけ mutex 版へ戻すのが正しい手当て。
 * is_always_lock_free は C++17。本体も利用側も -std=gnu++2a で建つ
 * (CMakeLists.txt の INTERFACE_COMPILE_OPTIONS) ので、無条件に書ける。 */
static_assert(std::atomic<INTEGER64>::is_always_lock_free,
	"stdInterval::now() は 64bit のロックフリー atomic を前提にしている");

int
stdInterval::wait(sPtr<tinyState>  THIS,INTEGER64 tm,int type)
{
sPtr<stdFrameWork> fw;
	fw = THIS->application->fw();
	return fw->wait(THIS,tm,type);
}

int
stdInterval::detach(sPtr<tinyState>  THIS)
{
sPtr<stdFrameWork> fw;
	fw = THIS->application->fw();
	return fw->detach(THIS);
}



/* 単調増加のマイクロ秒時刻。
 *
 * 2026-10-02: 単調化の担保をグローバル mutex から CAS へ替えた。返す値の意味は
 * 変えていない (「これまでに返した最大値と今の時刻の、大きい方」)。
 *
 * 理由: tinyState_::eventHandler() が入場時刻 (enter_time) を打つため、now() が
 * **全ディスパッチで必ず通る**経路になった。mutex 版はプロセス共通の 1 本を毎回
 * 取るので、スレッドが増えるとディスパッチごとに直列化点が挟まる。実測 (Linux
 * ・gettimeofday を引いた取り分):
 *
 *     threads      1       4       8      24
 *     mutex      9.0ns  73.4ns  86.0ns  88.8ns   <- スレッド数で悪化する
 *     CAS        0.8ns   1.3ns   1.3ns   3.2ns
 *
 * memory_order は acq_rel/acquire にしてある。relaxed でも単調性は足りるが、
 * mutex 版には付随的に acquire/release の壁があり、それに暗黙に頼っている
 * 呼び出し側が居ても壊さないため。実測コストは relaxed と差が無かった
 * (1 スレッドで 0.4ns)。
 */
INTEGER64
stdInterval::now()
{
struct timeval tm;
INTEGER64 ret;
INTEGER64 prev;
	gettimeofday(&tm,0);
	ret = ((INTEGER64)tm.tv_usec) + ((INTEGER64)tm.tv_sec)*1000000;

	prev = lastAccessTime.load(std::memory_order_acquire);
	for ( ; ; ) {
		/* 時計が戻った / 同じマイクロ秒に収まった → 過去の最大値を返す。 */
		if ( ret <= prev )
			return prev;
		if ( lastAccessTime.compare_exchange_weak(prev,ret,
				std::memory_order_acq_rel,
				std::memory_order_acquire) )
			return ret;
		/* 失敗時 prev には現在値が入っているので、そのまま回り直す。 */
	}
}
