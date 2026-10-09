

#include	"ts2/c++/stdLimitSemaphore.h"
#include	"ts2/c++/sException.h"
#include	"ts2/c++/sCallSection.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadMutexRecursive.h"
#include	<map>
#include	<list>
#include	<unordered_set>

/**
 * @brief 待ち行列の実体。入場 1 回の費用を待ち行列の長さに依らなくする。/ The wait
 * queue proper: the cost of one entry no longer depends on how many are waiting.
 * @details
 * 旧実装は `stdQueue<tinyState>` の連結リストをそのまま待ち行列にしていたので、
 * 入場 1 回で 3 度リストをたどっていた — 「自分は並んでいるか」(2 度) と
 * 「優先度の位置へ挿す」(1 度)。1 要素進むごとに `sPtr<stdQueueElement>` を
 * コピーする = refMtx 下の addref / relref が走るので、**待ち行列が長いほど
 * 入場が重くなり、全体では 2 乗**になっていた。利用側の planner は枠 21 本に
 * 1000 件を並べるので、反応器のスレッドがこれで張り付いた。
 *
 * ここでは順序を 2 つの入れ物で持つ:
 *   - `bucket` 優先度 (昇順) -> 同点の待ち手の列。先頭が先に入場する。
 *     挿入は O(log n) ・ 先頭の取り出しは O(1)。
 *   - `idx`    「並んでいるか」を O(1) で答える索引。
 *
 * ★ `stdQueue<tinyState>` を継承しているのは、`stdLimitSemaphore` の**ヘッダを
 * 変えずに** (= 利用側の ABI を動かさずに) 中身を差し替えるため。基底の
 * 連結リスト (head / tail) は**使わない**。使うのは `insNeq` と `count` だけで、
 * `count` は待ち行列の長さとして基底と同じ意味に保つ。
 *
 * 挙動は変えていない: 優先度の昇順 ・ 同点は `insNeq` の向き ・
 * `key == MAX_INTEGER64` は旧 `stdQueue_::ins` の末尾追加の速い道と同じく
 * `insNeq` を見ない (ヘッダの契約「enablePriority が倒れているときは見ない」)。
 */
class stdLimitSemaphoreWait : public stdQueue<tinyState> {
public:
	std::map<INTEGER64,std::list<sPtr<tinyState> > >	bucket;
	std::unordered_set<tinyState *>				idx;

	virtual ~stdLimitSemaphoreWait()
	{
		bucket.clear();
		idx.clear();
		count = 0;
	}

	/** @brief 待ち行列へ入れる。O(log n)。/ Enqueue; O(log n). */
	void w_ins(INTEGER64 key,sPtr<tinyState> obj)
	{
	std::list<sPtr<tinyState> > & b = bucket[key];
		if ( insNeq || key == MAX_INTEGER64 )
			b.push_back(obj);
		else	b.push_front(obj);
		idx.insert(obj.__get());
		count ++;
	}
	/** @brief 並んでいるか。O(1) ・ sPtr のコピーをしない。/ Queued? O(1), copies no sPtr. */
	int w_has(sPtr<tinyState> obj)
	{
		return idx.count(obj.__get()) != 0;
	}
	/** @brief 先頭 (= 最も優先度が小さい側の先頭) を外して返す。O(log n)。
	 *  / Dequeue the head (lowest priority value first); O(log n). */
	sPtr<tinyState> w_del()
	{
	sPtr<tinyState> ret;
		if ( bucket.empty() )
			return thNULL;
	std::map<INTEGER64,std::list<sPtr<tinyState> > >::iterator it = bucket.begin();
		ret = it->second.front();
		it->second.pop_front();
		if ( it->second.empty() )
			bucket.erase(it);
		idx.erase(ret.__get());
		count --;
		return ret;
	}
};

/* this->wait はコンストラクタが必ず stdLimitSemaphoreWait を入れる (他に代入する所は
 * 無い)。だからここは静的な下向き変換でよく、dynamic_cast の費用を毎回払わない。
 * その不変条件そのものはコンストラクタで一度だけ dynamic_cast で確かめる。 */
#define WAIT(q)		(static_cast<stdLimitSemaphoreWait*>((q).__get()))

stdLimitSemaphore::stdLimitSemaphore(int lim)
{
	v_limit = lim;
	this->wait = thNEW( stdLimitSemaphoreWait,());
	if ( dynamic_cast<stdLimitSemaphoreWait*>(this->wait.__get()) == 0 )
		stdObject::panic("stdLimitSemaphore: wait queue is not the expected type");
	/* 同じキーで ins したとき、既定 (insNeq=0) は「同キーの前」に入る = 後から来た待ち手が
	 * 先に待っていた側を追い越す。enablePriority を立てると priority() の既定値 10000 が
	 * 全員同じキーになるので、これを立てないと待ち行列が LIFO になり先着が飢える。
	 * insNeq=1 なら「同キーの後ろ」に入るので、優先度が違えば優先度順・同じなら先着順。
	 * enablePriority が偽のときは key=MAX_INTEGER64 の末尾追加経路に入るので参照されない。
	 * 利用側がこの既定を変えたいときは insNeq(0) を呼ぶ。 */
	this->wait->insNeq = 1;
}


stdLimitSemaphore::~stdLimitSemaphore()
{
sPtr<tinyState>  obj;
sPtr<stdQueue<tinyState> >  q;
	q = this->wait;
	this->wait = thNULL;
	for ( ; ; ) {
		obj = WAIT(q)->w_del();
		if ( obj == thNULL )
			break;
		obj->wakeup();
	}
}


void
stdLimitSemaphore::get()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		throw sException(0,EX_ERROR);
	if ( count < v_limit ) {
		count ++;
		return;
	}
	/* 既に並んでいるなら積み直さない (旧実装と同じ)。違うのは、それを
	 * 線形探索ではなく索引で答えること。 */
	if ( !WAIT(this->wait)->w_has(me) ) {
		if ( enablePriority )
			WAIT(this->wait)->w_ins(me->priority(thNULL),me);
		else	WAIT(this->wait)->w_ins(MAX_INTEGER64,me);
	}
	throw sException([this](sPtr<tinyState> caller) {
		/* ★ 破棄中は wait が thNULL になる。デストラクタは wait を外して
		 * から待ち手を起こすので、起こされた側がここへ来る順番が実際に
		 * 作れる。旧実装は無条件に wait を辿っていたので null 参照だった。 */
		if ( this->wait == thNULL )
			return 1;
		if ( WAIT(this->wait)->w_has(caller) )
			return 0;
		return 1;
	});
}


void
stdLimitSemaphore::release()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
sPtr<tinyState>  obj;
	if ( this->wait == thNULL )
		return;
	if ( count <= 0 )
		stdObject::panic("cannot release");
	count --;
	if ( count >= v_limit )
		return;
	obj = WAIT(this->wait)->w_del();
	if ( obj.is_notNull() )
		obj->wakeup();
}

void
stdLimitSemaphore::insNeq(int v)
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		return;
	this->wait->insNeq = ( v != 0 );
}

int
stdLimitSemaphore::insNeq()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	if ( this->wait == thNULL )
		return 1;
	return this->wait->insNeq;
}

int
stdLimitSemaphore::limit()
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
	return v_limit;
}

void
stdLimitSemaphore::limit(int lim)
{
sPtr<tinyState> me;
	me = sCallSection::key->caller();
sThreadMutexHandle __hdr(me->application->mtx);
sPtr<tinyState>  obj;
int _lim;
	if ( lim < 1 )
		return;
	if ( v_limit == lim )
		return;
	if ( v_limit > lim ) {
		v_limit = lim;
		return;
	}
	_lim = v_limit;
	v_limit = lim;
	for ( ; _lim < lim ; _lim ++ ) {
		obj = WAIT(this->wait)->w_del();
		if ( obj == thNULL )
			break;
		obj->wakeup();
	}
}
