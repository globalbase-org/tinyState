

#include	"ts2/c++/sThreadStack.h"

#include	<pthread.h>

/* 既定 (Linux/glibc)。glibc の拡張なので非標準だが、g++ は glibc 向けには
 * _GNU_SOURCE を常に定義するので宣言は見える。
 *
 * ★ この層は Cygwin では**使われない** (posix_Cygwin が同名ファイルで上書きする)。
 * Cygwin に pthread_getattr_np は無く、_WIN32 も定義されないので、ここに
 * #ifdef を足して振り分けようとすると「posix の既定実装に落ちる」事故になる。
 * 振り分けは層で行う。 */
INTEGER64
ts2_thread_stack_size()
{
pthread_attr_t	a;
size_t		sz = 0;
	if ( pthread_getattr_np(pthread_self(),&a) != 0 )
		return 0;
	if ( pthread_attr_getstacksize(&a,&sz) != 0 )
		sz = 0;
	pthread_attr_destroy(&a);
	return (INTEGER64)sz;
}

/* フレームワークが作る スレッドに与えるスタック長。4 機とも同じ 8 MB。
 * 層ごとに持たせてあるのは、この機だけ変えたくなったときに**ここだけ**
 * 書き換えれば済むようにするため (共通側に #ifdef を置かない規約)。 */
INTEGER64
ts2_worker_stack_size()
{
	return (INTEGER64)8 * 1024 * 1024;
}
