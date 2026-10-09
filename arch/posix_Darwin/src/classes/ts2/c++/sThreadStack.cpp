

#include	"ts2/c++/sThreadStack.h"

#include	<pthread.h>

/* macOS。pthread_getattr_np は無いが、こちらは失敗を返さない (値か 0)。
 *
 * ★ この機は main と worker で 16 倍違う (実測 main 8169KB / 既定 worker 519KB)。
 * 既定の worker が最も浅い機なので、閾値を「聞いて決める」効果がいちばん大きい。
 */
INTEGER64
ts2_thread_stack_size()
{
	return (INTEGER64)pthread_get_stacksize_np(pthread_self());
}

/* フレームワークが作る スレッドに与えるスタック長。4 機とも同じ 8 MB。
 * 層ごとに持たせてあるのは、この機だけ変えたくなったときに**ここだけ**
 * 書き換えれば済むようにするため (共通側に #ifdef を置かない規約)。 */
INTEGER64
ts2_worker_stack_size()
{
	return (INTEGER64)8 * 1024 * 1024;
}
