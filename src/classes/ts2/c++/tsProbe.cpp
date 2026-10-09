/*
 * 撤収プローブ — 「撤収中に誰が仕事を積みに来たか」
 *
 * 既存の表明は捨てられる側 (printParent) の名前しか出さず、窓 A (ins() が成功して
 * しまう窓) に至っては一行も出ない。ここでは **積みに来た側** を呼び出しスタックごと
 * 固定して記録する。
 *
 * このファイルを独立した TU にしてあるのは、MinGW のスタック取得に <windows.h> が
 * 要るため。共通コード (tsThread.cpp / tinyState.cpp) に windows.h を持ち込まずに
 * 済ませる。tinyState のクラスではないので codegen (tscpp2) は素通りする。
 *
 * 使い方と環境変数は src/h/ts2/c++/tsProbe.h の doxygen を参照。
 */

#if defined(_WIN32) && !defined(__CYGWIN__)
/* WIN32_LEAN_AND_MEAN で legacy <winsock.h> を締め出す (std2/includes.h が引く
 * <winsock2.h> を唯一の winsock にしておく)。fwIOarch.cpp と同じ作法。 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600		/* CaptureStackBackTrace の宣言に要る */
#endif
#include	<windows.h>
#define TS_PROBE_BT_WIN		1
#else
#include	<unistd.h>
/* ★ プラットフォーム名で決め打たず、ヘッダの有無で判定する。
 * 最初は `__GLIBC__ || __APPLE__ || __CYGWIN__` と書いていたが、
 * **Cygwin に execinfo.h は無い** (backtrace 自体が提供されない)。
 * 「あるはず」で並べた名前は、その環境で初めて建てたときに落ちる。 */
#if defined(__has_include)
#if __has_include(<execinfo.h>)
#include	<execinfo.h>
#define TS_PROBE_BT_POSIX	1
#endif
#endif
#endif

#include	<stdio.h>
#include	<stdarg.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<stdint.h>
#include	<pthread.h>

#include	"ts2/c++/sRptr.h"
#include	"_ts2/c++/tinyState_.h"
#include	"ts2/c++/tsProbe.h"


#define TS_PROBE_TAG		"TS2-PROBE"
#define TS_EVH_TAG		"TS2-EVH"
#define TS_PROBE_MAXDEPTH	64
#define TS_PROBE_CHAINMAX	32
#define TS_PROBE_BUFSIZE	8192


struct tsProbeCfg {
	const char *	path;		/* 0 = 無効 */
	int		to_stderr;
	int		do_abort;
	int		max;		/* 0 = 無制限 */
	int		depth;
	int		force;		/* 較正用: 撤収前の通常の ins() も記録する */
};

/* 記録の直列化。プローブは自分の外のロックを一切保持せずにここへ来ること
 * (getStateName() は相手の lm を取るので、文字列の組み立てはこの mutex の外で
 * 終わらせてある)。 */
static pthread_mutex_t	probe_mtx = PTHREAD_MUTEX_INITIALIZER;
static int		probe_seq;
static int		probe_dropped;


static int
probe_env_int(const char * name,int def)
{
const char * v = ::getenv(name);
	if ( v == 0 || *v == 0 )
		return def;
	return (int)::strtol(v,0,0);
}

static tsProbeCfg
probe_cfg_init()
{
tsProbeCfg c;
	::memset(&c,0,sizeof(c));
	c.path = ::getenv("TS2_TEARDOWN_PROBE");
	if ( c.path && *c.path == 0 )
		c.path = 0;
	if ( c.path == 0 )
		return c;
	if ( ::strcmp(c.path,"-") == 0 )
		c.to_stderr = 1;
	c.do_abort = probe_env_int("TS2_TEARDOWN_PROBE_ABORT",0);
	c.max      = probe_env_int("TS2_TEARDOWN_PROBE_MAX",64);
	c.depth    = probe_env_int("TS2_TEARDOWN_PROBE_DEPTH",24);
	c.force    = probe_env_int("TS2_TEARDOWN_PROBE_FORCE",0);
	if ( c.depth < 1 )
		c.depth = 1;
	if ( c.depth > TS_PROBE_MAXDEPTH )
		c.depth = TS_PROBE_MAXDEPTH;
	if ( c.max < 0 )
		c.max = 0;
	return c;
}

/* 関数内 static の初期化はスレッド安全 (C++11 magic statics)。環境変数は一度だけ読む。 */
static const tsProbeCfg &
probe_cfg()
{
static const tsProbeCfg c = probe_cfg_init();
	return c;
}

int
tsProbeTeardown_enabled()
{
const tsProbeCfg & c = probe_cfg();
	if ( c.path == 0 )
		return 0;
	return c.force ? 2 : 1;
}

static unsigned long long
probe_tid()
{
pthread_t	t = pthread_self();
unsigned long long v = 0;
	/* pthread_t の実体はポインタだったり整数だったり構造体だったりする。
	 * 値の意味を問わず、他の行と突き合わせられる識別子であればよい。 */
	::memcpy(&v,&t,sizeof(t) < sizeof(v) ? sizeof(t) : sizeof(v));
	return v;
}

static unsigned long
probe_pid()
{
#ifdef TS_PROBE_BT_WIN
	return (unsigned long)::GetCurrentProcessId();
#else
	return (unsigned long)::getpid();
#endif
}


/* 追記型の1行アペンダ。溢れたら黙って切り詰める (記録が無くなるより切れた方がまし)。 */
static void probe_add(char * buf,int size,int * pos,const char * fmt,...)
	__attribute__((format(printf,4,5)));

static void
probe_add(char * buf,int size,int * pos,const char * fmt,...)
{
va_list	ap;
int	n;
	if ( *pos >= size - 1 )
		return;
	va_start(ap,fmt);
	n = ::vsnprintf(buf + *pos,(size_t)(size - *pos),fmt,ap);
	va_end(ap);
	if ( n < 0 )
		return;
	*pos += n;
	if ( *pos > size - 1 )
		*pos = size - 1;
}

/* 親の連鎖。printParent() と同じ形だが stdString を使わない。
 * 撤収の最中に gc オブジェクトを新たに作らずに済ませたいのと、出力先が stdout では
 * ないため。getStateName() は相手の lm を取るので、呼び出し側のロックは外れていること。 */
static void
probe_chain(char * buf,int size,int * pos,sPtr<tinyState> job)
{
sPtr<tinyState>	pp;
int		i;
	probe_add(buf,size,pos,"%s[",job->getClass());
	pp = job->parent;
	for ( i = 0 ; pp.is_notNull() && i < TS_PROBE_CHAINMAX ; pp = pp->parent, i ++ )
		probe_add(buf,size,pos,"((%s*)%p)<%s>,",
			pp->getClass(),(void*)pp.__get(),pp->getStateName());
	if ( pp.is_notNull() )
		probe_add(buf,size,pos,"...,");
	probe_add(buf,size,pos,"TOP]");
}


/* プローブ自身の 2 段 (probe_stack と tsProbeTeardown) は要らないので飛ばす。
 * 呼び出し側 — ins() なり eventHandler なり — が 1 段目に来るようにしておくと、
 * 記録を読むときに段数を数え直さずに済む。 */
#define TS_PROBE_SKIP	2

static int
probe_stack(void ** frame,int depth)
{
#if defined(TS_PROBE_BT_WIN)
	return (int)::CaptureStackBackTrace(TS_PROBE_SKIP,(ULONG)depth,frame,0);
#elif defined(TS_PROBE_BT_POSIX)
void *	raw[TS_PROBE_MAXDEPTH + TS_PROBE_SKIP];
int	n = ::backtrace(raw,depth + TS_PROBE_SKIP);
	if ( n <= TS_PROBE_SKIP )
		return 0;
	n -= TS_PROBE_SKIP;
	::memcpy(frame,raw + TS_PROBE_SKIP,(size_t)n * sizeof(void *));
	return n;
#else
	(void)frame;
	(void)depth;
	return 0;
#endif
}

static void
probe_frames(char * buf,int size,int * pos,void ** frame,int n)
{
int	i;
	if ( n <= 0 ) {
		probe_add(buf,size,pos,TS_PROBE_TAG "   stk : (unavailable on this platform)\n");
		return;
	}
	probe_add(buf,size,pos,TS_PROBE_TAG "   stk :");
	for ( i = 0 ; i < n ; i ++ )
		probe_add(buf,size,pos," %p",frame[i]);
	probe_add(buf,size,pos,"\n");

#if defined(TS_PROBE_BT_WIN)
	/* dbghelp は使わない (Sym* の初期化はプロセス状態を触るし、撤収中に呼ぶには重い)。
	 * 代わりに「モジュール+オフセット」を出しておけば addr2line で後から解ける。 */
	for ( i = 0 ; i < n ; i ++ ) {
	HMODULE	hm = 0;
	char	mod[MAX_PATH];
	const char * base;
		if ( ::GetModuleHandleExA(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
				GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR)frame[i],&hm) == 0 || hm == 0 ) {
			probe_add(buf,size,pos,TS_PROBE_TAG "   frm : ?? [%p]\n",frame[i]);
			continue;
		}
		mod[0] = 0;
		::GetModuleFileNameA(hm,mod,(DWORD)sizeof(mod));
		base = ::strrchr(mod,'\\');
		base = base ? base + 1 : mod;
		probe_add(buf,size,pos,TS_PROBE_TAG "   frm : %s+0x%llx [%p]\n",
			base,
			(unsigned long long)((uintptr_t)frame[i] - (uintptr_t)hm),
			frame[i]);
	}
#elif defined(TS_PROBE_BT_POSIX)
	{
	char ** sym = ::backtrace_symbols(frame,n);
		if ( sym == 0 )
			return;
		for ( i = 0 ; i < n ; i ++ )
			probe_add(buf,size,pos,TS_PROBE_TAG "   frm : %s\n",sym[i]);
		::free(sym);		/* backtrace_symbols の契約: 配列本体だけを free */
	}
#endif
}


static void
probe_emit(const tsProbeCfg & c,const char * rec,int len)
{
FILE *	fp;
	if ( c.to_stderr ) {
		::fwrite(rec,1,(size_t)len,stderr);
		::fflush(stderr);
		return;
	}
	/* 都度 open の追記。ctest は同じパスへ多数のプロセスから書き込む。1 レコードを
	 * 1 回の fwrite で出せば O_APPEND のおかげで行が混ざらずに済む。 */
	fp = ::fopen(c.path,"a");
	if ( fp == 0 )
		return;
	::fwrite(rec,1,(size_t)len,fp);
	::fclose(fp);
}


void
tsProbeTeardown(sPtr<tinyState> job,const char * window,const char * pool)
{
const tsProbeCfg & c = probe_cfg();
char		rec[TS_PROBE_BUFSIZE];
void *		frame[TS_PROBE_MAXDEPTH];
int		pos = 0;
int		nframe;
int		seq = 0;
int		over = 0;
int		dropped_now = 0;

	if ( c.path == 0 )
		return;

	nframe = probe_stack(frame,c.depth);

	{
	int er = ::pthread_mutex_lock(&probe_mtx);
		if ( er )
			return;
		if ( c.max && probe_seq >= c.max ) {
			over = 1;
			dropped_now = ++ probe_dropped;
		}
		else
			seq = ++ probe_seq;
		::pthread_mutex_unlock(&probe_mtx);
	}
	if ( over ) {
		/* 上限の告知は 1 回だけ。ここで毎回書くと上限の意味が無くなる。 */
		if ( dropped_now == 1 ) {
			pos = 0;
			probe_add(rec,sizeof(rec),&pos,
				TS_PROBE_TAG " PROBE suppressed pid=%lu"
				" — hit TS2_TEARDOWN_PROBE_MAX=%d\n",
				probe_pid(),c.max);
			probe_emit(c,rec,pos);
		}
		return;
	}

	probe_add(rec,sizeof(rec),&pos,
		TS_PROBE_TAG " PROBE seq=%d window=%s pid=%lu tid=0x%llx t=%lld\n",
		seq,window,probe_pid(),probe_tid(),(long long)::time(0));
	probe_add(rec,sizeof(rec),&pos,
		TS_PROBE_TAG "   pool: %s\n",pool ? pool : "(none)");
	if ( job.is_notNull() ) {
		probe_add(rec,sizeof(rec),&pos,
			TS_PROBE_TAG "   job : %s %p <%s>\n",
			job->getClass(),(void*)job.__get(),job->getStateName());
		probe_add(rec,sizeof(rec),&pos,TS_PROBE_TAG "   own : ");
		probe_chain(rec,sizeof(rec),&pos,job);
		probe_add(rec,sizeof(rec),&pos,"\n");
	}
	else
		probe_add(rec,sizeof(rec),&pos,TS_PROBE_TAG "   job : (null)\n");
	probe_frames(rec,sizeof(rec),&pos,frame,nframe);
	probe_add(rec,sizeof(rec),&pos,TS_PROBE_TAG " PROBE end seq=%d\n",seq);

	probe_emit(c,rec,pos);

	if ( c.do_abort )
		::abort();
}


/*******************************************
	深すぎる入れ子を gc へ逃がした記録
********************************************/

/* 延べ件数。環境変数とは無関係に常に数える。POD なのでデストラクタは登録されない
 * (tinyState は破棄される static を 1 つも作らない — sImmortal.h / ctest の
 * tinyState_no_static_dtor)。 */
static volatile INTEGER64	evh_escapes;
static volatile INTEGER64	evh_delivers;
static volatile int		evh_lines;
static volatile int		evh_dlines;
/* ★ 「打ち切った」と言ったかのフラグは **逃がし側と配送側で別に持つ**。
 * 行数カウンタ (evh_lines / evh_dlines) を分けておきながらここを共有すると、
 * 先に打ち切った片方が言った時点でもう片方が**黙って切る**ことになり、
 * 行を数えた人が件数を取り違える。実際に配送側が黙っていて、利用側が逃がし数を
 * 1 桁取り違えた。数える側から見た不変条件は「打ち切ったら必ず言う」。 */
static volatile int		evh_said_cut;
static volatile int		evh_said_dcut;

struct tsEvhCfg {
	int	on;	/* TS2_EVH_PROBE が設定されている */
	int	max;	/* 行数の上限。0 = 無制限 */
};

static tsEvhCfg
evh_cfg_init()
{
tsEvhCfg c;
const char * v = ::getenv("TS2_EVH_PROBE");
	c.on  = (v && *v) ? 1 : 0;
	c.max = probe_env_int("TS2_EVH_PROBE_MAX",64);
	if ( c.max < 0 )
		c.max = 0;
	return c;
}

static const tsEvhCfg &
evh_cfg()
{
static const tsEvhCfg c = evh_cfg_init();
	return c;
}

INTEGER64
tsProbeEvhEscapeCount()
{
	return evh_escapes;
}

INTEGER64
tsProbeEvhDeliverCount()
{
	return evh_delivers;
}

/* ★ ここは逃がしの経路から、**lm を持たずに**呼ばれる (tinyState.cpp の深さガードは
 * lm を取る前に居る)。getClass() は相手のロックを取らないので安全。
 * 相手の状態名は引かない — getStateName() は lm を取るので、呼び出し規約が変わる。 */
INTEGER64
tsProbeEvhEscape(sPtr<tinyState> job,int evtype,int depth,int limit)
{
const tsEvhCfg & c = evh_cfg();
INTEGER64 seq;
	seq = __sync_add_and_fetch(&evh_escapes,1);
	if ( !c.on )
		return seq;
	if ( c.max ) {
		if ( __sync_add_and_fetch(&evh_lines,1) > c.max ) {
			if ( __sync_val_compare_and_swap(&evh_said_cut,0,1) == 0 )
				::fprintf(stderr,"%s escape (以降は打ち切り: %d 行)\n",
						TS_EVH_TAG,c.max);
			return seq;
		}
	}
	::fprintf(stderr,"%s escape seq=%lld class=%s ev=0x%x depth=%d limit=%d tid=%llu\n",
			TS_EVH_TAG,(long long)seq,
			job.is_notNull() ? job->getClass() : "(null)",
			(unsigned)evtype,depth,limit,probe_tid());
	::fflush(stderr);
	return seq;
}

/* 配送側。逃がし側の seq と 1 対 1 に対応する。
 *
 * ★ 行数の打ち切りは逃がし側と**別に**数える (同じ上限を別々に当てる)。
 * 同じカウンタを共有すると、揃っているかを数えたい当人が打ち切りで数を崩す。
 * `TS2_EVH_PROBE_MAX=0` にすれば両方とも全件出る。
 *
 * ★★ 打ち切ったら**必ず 1 度言う** (逃がし側と対称)。行の数は件数ではないので、
 * 黙って切ると「64 件だった」と読まれる。件数が要るだけなら
 * `tsProbeEvhEscapeCount()` / `tsProbeEvhDeliverCount()` を使う方が確実
 * (環境変数と無関係に常に数えていて、打ち切りの影響を受けない)。 */
void
tsProbeEvhDeliver(sPtr<tinyState> job,int evtype,INTEGER64 seq)
{
const tsEvhCfg & c = evh_cfg();
	__sync_add_and_fetch(&evh_delivers,1);
	if ( !c.on )
		return;
	if ( c.max ) {
		if ( __sync_add_and_fetch(&evh_dlines,1) > c.max ) {
			if ( __sync_val_compare_and_swap(&evh_said_dcut,0,1) == 0 )
				::fprintf(stderr,"%s deliver (以降は打ち切り: %d 行)\n",
						TS_EVH_TAG,c.max);
			return;
		}
	}
	::fprintf(stderr,"%s deliver seq=%lld class=%s ev=0x%x tid=%llu\n",
			TS_EVH_TAG,(long long)seq,
			job.is_notNull() ? job->getClass() : "(null)",
			(unsigned)evtype,probe_tid());
	::fflush(stderr);
}
