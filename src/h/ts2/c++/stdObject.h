

#ifndef ___stdObject_cpp_H___
#define ___stdObject_cpp_H___
// TS2_DONT_MODIFY

#include	"ts2/c++/ts_types.h"
#include	"ts2/c++/sObject.h"
#include	"ts2/c++/sThreadMutex.h"


#define stdObject_REF_MUTEX_NUM		101

#define REF__NOTICE	std::function<void()>
#define REF_NOTICE	int


/**
 * @brief tinyState フレームワークにおける参照カウント付きオブジェクトの基底クラス。/ Base class with reference counting for all framework objects.
 * @details
 * `sPtr<T>` による参照カウントを実装する。`addref()`/`relref()` で参照数を管理し、
 * 参照数が 0 になると GC スレッドがオブジェクトを破棄する。
 * 生 `new` は使わず `thNEW(T, (args))` でのみ生成すること。
 * `stdObject::start()` / `stdObject::finish()` でフレームワーク全体の GC スレッドを制御する。
 * / Implements reference counting used by `sPtr<T>`. Objects are destroyed by the GC thread
 * when the count reaches zero. Always use `thNEW` — never raw `new`.
 */
class stdObject : public sObject {
public:

       	stdObject();
	virtual ~stdObject();

	void addref();
	void relref();
	int getref();
	static int seq();
	static int
	v_printf(const char * fmt,va_list arg);
	static int
	printf(const char * fmt,...);

	static void start();
	static void finish();
	/** @brief GC が安定（保留中の delete / refEvent キューが空）かを返す。
	 *  / True when the GC has no pending work at the root bucket: no ref==0 objects
	 *  queued for delete and no refEvent (auto-teardown) callbacks pending.  Used by
	 *  tsThread teardown to know refEvent can no longer fire before it frees the pool. */
	static int is_stable();
	/** @brief Sleep until is_stable() holds — 安定になるまで眠って待つ
	 *  @details Woken by the gc thread's broadcast as it goes idle.  Used by teardown to
	 *  wait for stability *outside* the pool mutex; see the comment on the definition. /
	 *  gc スレッドが idle へ入る際の broadcast で起きる。teardown が pool の mutex の
	 *  外で安定を待つために使う。理由は定義側のコメント参照。 */
	static void wait_stable();
protected:
	virtual void refEvent() {};
	void nRefEvent(int obs);
	int nRefEvent();
private:

	int		ref;
	int		refEvent_num;
	stdObject *	next;
	unsigned	in_list:1;
	static void		gc(int);
	static void *		gc_thread(void*);

	static int8_t	finish_flag;
	static int8_t	start_flag;

	static sThreadMutex	refMtx[stdObject_REF_MUTEX_NUM];
	static int8_t		refFlags[stdObject_REF_MUTEX_NUM];
	static stdObject *	refList[stdObject_REF_MUTEX_NUM];
	static stdObject *	refEventHead[stdObject_REF_MUTEX_NUM];
	static stdObject *	refEventTail[stdObject_REF_MUTEX_NUM];
	static sThreadCond	refCond;
};




#endif


