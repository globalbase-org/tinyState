

#include        <unistd.h>
#include        <sys/types.h>
#include        <sys/wait.h>
#include        <errno.h>
#include        <signal.h>
#if defined(__linux__)
#include        <sys/syscall.h>		/* SYS_close_range (5.9+)。無ければ従来ループへ落ちる */
#endif
#include        <termios.h>

#include	"_ts2/c++/ts2System_.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/ts2IOdescriptor.h"
#include	"ts2/c++/tsSignal.h"
#include	"ts2/c++/ts2IOdevNull.h"

CLASS_TINYSTATE(ts2/c++/ts2System,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

/**
 * @brief サブプロセスを起動・監視する tinyState。/ Launch and monitor a subprocess.
 * @details
 * コンストラクタで `fork`/`exec` してプロセスを起動し、終了を検出したら
 * 親に `TSE_RETURN` を送る。`ev->msg_int` に `waitpid` の生 `status` が入る
 * (`WEXITSTATUS` / `WIFSIGNALED` 等で評価すること)。Windows(MinGW) も同じ形に正規化して
 * 載せるので利用側に OS 判定は要らない。詳細は @ref ts2system_status。
 *
 * **commandLine の形式 / commandLine format:**
 * | プレフィックス | 起動方法 |
 * |---|---|
 * | なし (通常文字列) | `sh -c commandLine` 経由。シェル展開・パイプ・リダイレクト可 |
 * | `'#'` 始まり | `#cmd arg1 arg2` → `execvp` で直接起動。sh を挟まない |
 *
 * **プロセスグループ kill / Process-group kill:**
 * 子プロセスは常に `setpgid(0,0)` で独立したプロセスグループに置かれる。
 * `destroy()` 時は `kill(-pgid, SIG)` でグループ全体へ送るので、`sh -c` 経由の
 * **孫プロセスにも届く**。
 *
 * ⚠ 「届く」ことと「終了する」ことは別である。以下は保証していない:
 * - **SIGTERM を捕まえて終了しない**子/孫は死なない。捕捉も無視も block もできないのは
 *   SIGKILL だけなので、終了を保証したいなら `DM_CONT_KILL` を使うこと。
 * - 子/孫が自分で `setsid()` / `setpgid(0,0)` を呼んで**グループから出た**場合は、
 *   どのシグナルでも届かない (デーモン化する子はこれをやる)。
 * - `TSE_RETURN` は `waitpid` した **直接の子** の status である。`sh -c` 経由なら
 *   それは sh の終了であって、**孫の終了を表さない**。sh が死んで孫が生き残った状態でも
 *   `TSE_RETURN` は返る。
 *
 * / kill(-pgid, SIG) reaches grandchildren too, but reaching is not dying: a child or
 * grandchild that catches SIGTERM survives (use `DM_CONT_KILL` if termination must be
 * guaranteed), one that called `setsid()` has left the group and receives nothing, and
 * `TSE_RETURN` carries the status of the *direct* child -- with `sh -c` that is sh's
 * exit, not the grandchild's.
 *
 * **I/O / Pipes:**
 * `rfd` に子の stdout、`wfd` に子の stdin、`efd` に子の stderr を受け取る。
 * 不要なものは `nullptr` を渡す。`rfd`/`efd` は自動的に `ts2IOdevNull` へ排水される。
 *
 * @code{.cpp}
 * // 例: sh -c 経由で起動し stdout を読む
 * TS_STATE(ACT_START) {
 * TS_PRIVATE(int retPid; sPtr<ts2IO> rfd;)
 *     sys = thNEW(ts2System,(ifThis, &retPid,
 *         "ls -l /tmp", &rfd, nullptr, nullptr));
 *     return ACT_WAIT;
 * }
 * TS_STATE(ACT_WAIT) {
 *     if (ev->type != TSE_RETURN || ev->source != sys) return 0;
 *     int exit_code = WEXITSTATUS(ev->msg_int);
 *     return rDO|ACT_NEXT;
 * }
 *
 * // 例: # プレフィックスで直接 exec (sh なし)
 * sys = thNEW(ts2System,(ifThis, &retPid,
 *     "#/usr/bin/myapp --flag value", &rfd, nullptr, &wfd));
 *
 * // 例: 強制終了
 * sys->destroy(DM_CONT_KILL);   // SIGKILL → 1 秒後に再送し続ける
 * @endcode
 *
 * ---
 *
 * コンストラクタで `fork`/`exec` してプロセスを起動。終了時に親へ `TSE_RETURN` を送る。
 * `ev->msg_int` は `waitpid` の生 `status`。`WEXITSTATUS(ev->msg_int)` で終了コードを得る。
 *
 * @see ts2system_status — 終了 status の形 (Windows(MinGW) を含む) / exit status shape
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief サブプロセスを起動する。/ Launch a subprocess.
	 * @param parent
	 *   親 tinyState。TSE_RETURN の送り先になる。/ Parent tinyState; receives TSE_RETURN on exit.
	 *
	 * @param retp
	 *   起動した子プロセスの PID を書き込むポインタ。`fork` 失敗など起動エラー時は負値。
	 *   PID は通常 `destroy()` の呼び出しには不要 (ts2System が内部で保持する)。
	 *   外部から直接 `kill` したい場合や `waitpid` を自前で呼ぶ用途向け。<br>
	 *   / Receives the child PID. Negative on launch error. Normally not needed for destroy()
	 *   since ts2System tracks it internally; useful if you need to kill or waitpid manually.
	 *
	 * @param commandLine
	 *   コマンド文字列。先頭文字で起動方法が変わる:<br>
	 *   - 通常文字列 → `sh -c commandLine` で起動。シェル展開・パイプ・リダイレクト・
	 *     環境変数展開が使える。ただし sh が fork した実プロセスは孫になる。<br>
	 *   - `'#'` 始まり → `#cmd arg1 arg2` のように空白区切りで渡すと `execvp` で直接起動。
	 *     sh を挟まないので ret が実プロセスの PID になり、孫が生まれない。<br>
	 *   / Command string. Leading char selects launch mode:<br>
	 *   - normal string → `sh -c commandLine`. Shell expansion/pipes work; real process is a grandchild.<br>
	 *   - `'#'`-prefix → `#cmd arg1 arg2` → direct `execvp`. No sh; ret is the real process PID.
	 *
	 * @param rfd
	 *   子の **stdout** (親から見て読む側) を受け取る `sPtr<ts2IO>` へのポインタ。<br>
	 *   `nullptr` を渡すと stdout は内部の `ts2IOdevNull` へ自動排水される(詰まらない)。<br>
	 *   受け取った `ts2IO` は使い終わったら `->destroy()` すること。<br>
	 *   / Pointer to receive child **stdout** (parent-readable) as `sPtr<ts2IO>`.
	 *   Pass `nullptr` to auto-drain via `ts2IOdevNull`. Destroy the `ts2IO` when done.
	 *
	 * @param efd
	 *   子の **stderr** (親から見て読む側) を受け取る `sPtr<ts2IO>` へのポインタ。<br>
	 *   `nullptr` を渡すと stderr は内部の `ts2IOdevNull` へ自動排水される。<br>
	 *   / Pointer to receive child **stderr** as `sPtr<ts2IO>`. `nullptr` → auto-drained.
	 *
	 * @param wfd
	 *   子の **stdin** (親から見て書く側) を受け取る `sPtr<ts2IO>` へのポインタ。<br>
	 *   `nullptr` を渡すとパイプの書き込み端を即 close する (子の stdin は EOF になる)。<br>
	 *   / Pointer to receive child **stdin** (parent-writable) as `sPtr<ts2IO>`.
	 *   `nullptr` → write end closed immediately (child gets EOF on stdin).
	 *
	 * @param dmode
	 *   下位バイト (`DM_CMD`) に kill モード、上位バイト (`DM_FLAG`) にフラグを OR で指定:<br>
	 *   - `DM_NORMAL` (0, デフォルト): `destroy()` 時に SIGTERM を 1 回送って終了を待つ。<br>
	 *   - `DM_CONT_TERM` (1): `destroy()` 時に SIGTERM を送り 1 秒ごとに再送し続ける。<br>
	 *   - `DM_CONT_KILL` (3): `destroy()` 時に SIGKILL を送り 1 秒ごとに再送し続ける。<br>
	 *   - `DM_TTY` (0x100): 子の stdio を PTY で接続する (端末エミュレータ向け)。<br>
	 *   - `DM_APPLY` (0x200): 呼び出し元が fd を事前に用意して渡す (高度な用途)。<br>
	 *   kill は常にプロセスグループ全体 (`kill(-pgid, SIG)`) を対象にするので、
	 *   `sh -c` 経由の孫プロセスにも**届く**。ただし終了までは保証しない
	 *   (捕まえて死なない子・グループから出た子には効かない)。詳細はクラスの説明を参照。<br>
	 *   / Low byte (`DM_CMD`) selects kill mode; high byte (`DM_FLAG`) are ORed flags.
	 *   Kill always targets the whole process group (`kill(-pgid, SIG)`), so it reaches
	 *   grandchildren; it does not guarantee they exit -- see the class description.
	 */
	ts2System_(
		sPtr<tinyState> parent,
		int *		retp,
		const char *	commandLine,
		sPtr<ts2IO> * rfd=0,
		sPtr<ts2IO> * efd=0,
		sPtr<ts2IO> * wfd=0,
		int	dmode=0);
	/** @brief stdString でコマンドを渡すオーバーロード。/ Overload accepting stdString commandLine. */
	ts2System_(
		sPtr<tinyState> parent,
		int *		retp,
		sPtr<stdString> commandLine2,
		sPtr<ts2IO> * rfd=0,
		sPtr<ts2IO> * efd=0,
		sPtr<ts2IO> * wfd=0,
		int	dmode=0);

	/** @brief SIGTERM でプロセスグループを kill してから終了を待つ。/ Kill pgroup with SIGTERM and wait. */
	virtual void destroy();
	/** @brief 指定モードでプロセスグループを kill する。/ Kill pgroup in specified mode.
	 * @param mode
	 *   - `DM_CONT_TERM`: SIGTERM を送り、1 秒ごとに再送し続ける。<br>
	 *   - `DM_CONT_KILL`: SIGKILL を送り、1 秒ごとに再送し続ける (確実に殺したい場合)。<br>
	 *   - `DM_NORMAL` (0): SIGTERM を 1 回送って終了を待つ (デフォルト destroy() と同じ)。
	 *     **1 回しか送らない**ので、捕まえて死なない子はそのまま生き残り、待ちは戻らない。<br>
	 *   プロセスグループ全体が対象なので孫プロセスにも**届く** (終了の保証は上記のとおり
	 *   `DM_CONT_KILL` のみ)。<br>
	 *   / `DM_CONT_TERM`: SIGTERM every 1 s. `DM_CONT_KILL`: SIGKILL every 1 s.
	 *   `DM_NORMAL`: a *single* SIGTERM -- a child that catches it and does not exit stays
	 *   alive and the wait never returns.  Targets the entire process group.
	 */
	virtual void destroy(int mode);

	/** @brief プロセスの終了を待たずに TSE_RETURN を親へ送る。/ Send TSE_RETURN without waiting for exit.
	 * @details
	 * プロセス自体は kill せずバックグラウンドで走り続けさせる。
	 * `ts2System` の監視から外れるだけ。
	 * / Does not kill the process; it continues running in the background.
	 * Just detaches ts2System from monitoring it.
	 */
	void detach();
private:
protected:
	void newProcess(sPtr<tinyState> p);
	virtual sPtr<stdEvent>  filter(sPtr<stdEvent>  ev);

	TS_DEFARGS

	static void signal_handler_empty(int sig);
	int do_exec(
		const char *command,
		int *fd_r,
		int *fd_w,
		int *fd_err,
		int dmode);


	unsigned 	detach_flag:1;
	unsigned	kill_flag:1;
	int status;

	INTEGER64	kill_timer;
	int		destroy_mode;
	int		ret;

	sPtr<ts2IO>	d_rfd;
	sPtr<ts2IO>	d_efd;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IO;
class tsSignal;
TS_END_INTERFACE

#endif


ts2System_::ts2System_(TS_ARGS0)
        : tinyState_(parent)
{
    TS_CPARGS0
	newProcess(parent);
}

ts2System_::ts2System_(TS_ARGS1)
        : tinyState_(parent)
{
    TS_CPARGS1
	commandLine = commandLine2->get_str();
	newProcess(parent);
}

void
ts2System_::newProcess(sPtr<tinyState> p)
{
int _fd[3];
sPtr<ts2IO> iofd[3];
int d_iofd[3];
	if ( wfd == 0 && efd == 0 )
		dmode |= DM_TTY;
	_fd[0] = _fd[1] = _fd[2] = -1;
	d_iofd[0] = d_iofd[1] = d_iofd[2] = 0;
	this->ret = this->do_exec(commandLine,
		&_fd[0],
		&_fd[1],
		&_fd[2],
		dmode);
	*retp = ret;
	if ( ret < 0 )
		return;
int i,j;
	for ( i = 0 ; i < 3 ; i ++ ) {
		for ( j = 0 ; j < i ; j ++ )
			if ( _fd[i] == _fd[j] )
				break;
		if ( j == i ) {
			iofd[i] = thNEW(ts2IOdescriptor,(p,_fd[i]));
			d_iofd[i] = 1;
		}
		else	iofd[i] = iofd[j];
	}
	if ( rfd ) {
		*rfd = iofd[0];
		for ( j = 0 ; j < 3 ; j ++ )
			if ( iofd[0] == iofd[j] )
				iofd[j] = thNULL;
	}
	if ( wfd ) {
		*wfd = iofd[1];
		for ( j = 0 ; j < 3 ; j ++ )
			if ( iofd[1] == iofd[j] )
				iofd[j] = thNULL;
	}
	if ( efd ) {
		*efd = iofd[2];
		for ( j = 0 ; j < 3 ; j ++ )
			if ( iofd[2] == iofd[j] )
				iofd[j] = thNULL;
	}
	if ( iofd[0] != thNULL ) {
		d_rfd = iofd[0];
		i = 0;
		for ( j = 0 ; j < 3 ; j ++ )
			if ( i != j && iofd[i] == iofd[j] )
				iofd[j] = thNULL;
	}
	if ( iofd[2] != thNULL ) {
		d_efd = iofd[2];
		i = 2;
		for ( j = 0 ; j < 3 ; j ++ )
			if ( i != j && iofd[i] == iofd[j] )
				iofd[j] = thNULL;
	}
	if ( iofd[1] != thNULL )
		iofd[1]->destroy();
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/

sPtr<stdEvent> 
ts2System_::filter(sPtr<stdEvent>  ev)
{
  	if ( ev == thNULL ) return ev;
	if ( is_destroyed() ) 
		if ( kill_flag == 0 ) {
			kill_flag = 1;
			switch ( destroy_mode ) {
			case DM_CONT_TERM:
			        kill(-ret,SIGTERM);
				kill_timer = stdInterval::now();
				stdInterval::wait(ifThis,1000*1000,TSE_TIMER);
				break;
			case DM_CONT_KILL:
			        kill(-ret,SIGKILL);
				kill_timer = stdInterval::now();
				stdInterval::wait(ifThis,1000*1000,TSE_TIMER);
				break;
			default:
			        kill(-ret,SIGTERM);
				break;
			}
		}
	return TS_BASECLASS::filter(ev);
}


void
ts2System_::detach()
{
	detach_flag = 1;
	wakeup();
}

void
ts2System_::destroy()
{
	TS_BASECLASS::destroy();
}

void
ts2System_::destroy(int mode)
{
	destroy_mode = mode;
	TS_BASECLASS::destroy();
	wakeup();
}

void
ts2System_::signal_handler_empty(int sig)
{
	::signal(sig,signal_handler_empty);
}

#define R	(0)
#define W	(1)

int
ts2System_::do_exec(
	const char *command,
	int *fd_r,
	int *fd_w,
	int *fd_err,
	int dmode)
{
int     pipe_c2p[2],pipe_p2c[2];
int 	pipe_err[2];
int     pid;
sigset_t saved_sigmask;		/* fork の前に止めた mask の退避先。fork 失敗時も親で戻す */

//	_fd_r = _fd_w = _fd_err = -1;

        /* Create two of pipes. */
	if ( dmode & DM_TTY ) {
	int master,slave;
	char name[100];
	struct termios ios={0};
		if ( soOPENPTY(&master,&slave,name,0,0) < 0 )
			return(-5);
		tcgetattr(master,&ios);
		ios.c_iflag=0;
		ios.c_oflag=0;
		ios.c_lflag &= 0;     /* non-canonical */
		ios.c_cc[VMIN ]=0; /* non-block-mode */
		ios.c_cc[VTIME]=0;
		ios.c_cflag |= CS8;

		tcsetattr(slave,TCSANOW,&ios);
		tcflush(slave,TCIOFLUSH);
		pipe_p2c[W] = master;
		pipe_p2c[R] = slave;
		pipe_c2p[W] = slave;
		pipe_c2p[R] = master;
		pipe_err[W] = slave;
		pipe_err[R] = master;
	}
	else if ( dmode & DM_APPLY ) {
		pipe_p2c[W] = -1;
		pipe_p2c[R] = *fd_r;
		pipe_c2p[W] = *fd_w;
		pipe_c2p[R] = -1;
		pipe_err[W] = *fd_err;
		pipe_err[R] = -1;
	}
	else {
	        if ( soPIPE(pipe_c2p) < 0 ){
        	        return(-1);
	        }
        	if ( soPIPE(pipe_p2c) < 0 ){
		  	soCLOSE(pipe_c2p[R]);
        	        soCLOSE(pipe_c2p[W]);
	                return(-2);
        	}
		if ( soPIPE(pipe_err) < 0 ) {
        	        soCLOSE(pipe_c2p[R]);
	                soCLOSE(pipe_c2p[W]);
        	        soCLOSE(pipe_p2c[R]);
	                soCLOSE(pipe_p2c[W]);
        	        return(-3);
		}
	}

sPtr<stdArray<const char*> > args_cmd;
const char ** args_ptr;
sPtr<stdArray<sPtr<stdString> > > args;	/* MUST stay in scope until exec: args_cmd->ary[]
					   below hold raw char* borrowed from these stdString
					   buffers (via get_str()).  When this was declared
					   inside the `#`-block it was destroyed at the block
					   end — before fork/exec — freeing the strings and
					   leaving args_cmd->ary[] dangling.  Linux happened to
					   read the not-yet-reused memory intact; on Cygwin the
					   concurrent framework threads reuse the freed heap, so
					   argv[0] came back garbage and execvp failed with
					   ENOENT ~65% of the time (lost child I/O). */
	if ( command[0] == '#' ) {
	sPtr<stdString>  cmd,  v,  res;
	sPtr<stdRx>  rx1, rx2,  rx3;
	int i;
		args = thNEW( stdArray<sPtr<stdString> >,(0));
		cmd = thNEW( stdString,(&command[1]));
		rx1 = thNEW( stdRx,("^[ \t]*"));
		rx2 = thNEW( stdRx,("^\"(([^\\\\\"]*(\\\\|\\\\\"|()))*)\""));
		rx3 = thNEW( stdRx,("^([^ \t]+)"));

		for ( i = 0 ; ; i ++ ) {
			if ( cmd->cmp("") == 0 )
				break;
			cmd->rx("s",rx1,"");
			if ( cmd->rx("s",rx2,"").is_notNull() ) {
				v = cmd->rxVar(1);
			}
			else if ( cmd->rx("s",rx3,"").is_notNull() ) {
				v = cmd->rxVar(1);
			}
			else {
				break;
			}
			args->length(i+1);
			args->ary[i] = v;
		}
		args_cmd = thNEW( stdArray<const char*>,(args->length()+1));
		for ( i = 0 ; i < args->length() ; i ++ )
			args_cmd->ary[i] = args->ary[i]->get_str();
		args_cmd->ary[i] = 0;
		args_ptr = &args_cmd->ary[0];
	}

        /* Invoke processs */
	/* ★ fork の **前に** シグナルを止める。子側で戻すだけでは、fork が子で返ってから
	 * 下のリセットの 1 命令目までがまだ親のハンドラで、窓が残る (狭いだけ)。
	 * ここで止めておくと、その区間は **配送そのものが起こり得ない** —
	 * 届いたシグナルは pending のまま待ち、子がハンドラを既定へ戻して mask を解いた
	 * 時点で配送されて既定動作で死ぬ。⇒ 窓が狭くなるのではなく **無くなる**。
	 * ★ 親側は「配送が µs 遅れる」だけで、**取りこぼしはしない** (pending は消えない)。
	 *   止めるのは fork するスレッドの mask だけなので (pthread_sigmask)、プロセス宛の
	 *   シグナルは止めていない他スレッドが受ける。
	 * ⚠ SIGKILL / SIGSTOP は止められないが **捕捉もできない** = 食うハンドラが存在しない。
	 * ⚠ pending なシグナルは子に継承されない ⇒ 子を誤って撃つことはない。 */
	{
	sigset_t block_all;
		sigfillset(&block_all);
		pthread_sigmask(SIG_BLOCK,&block_all,&saved_sigmask);
	}
        if ( (pid = fork()) < 0 ){
		pthread_sigmask(SIG_SETMASK,&saved_sigmask,NULL);
                soCLOSE(pipe_c2p[R]);
		soCLOSE(pipe_c2p[W]);
		if ( !(dmode & DM_TTY) ) {
	                soCLOSE(pipe_p2c[R]);
        	        soCLOSE(pipe_p2c[W]);
                	soCLOSE(pipe_err[R]);
	                soCLOSE(pipe_err[W]);
		}
                return(-4);
        }
        if ( pid == 0 ) {      /* I'm child */
	int rc,fd;
		/* ★ fork から exec までの子は **親のシグナル設定を引き継いだ親のコピー**である。
		 * その窓の間に届いたシグナルは *親のハンドラ* に食われ、exec でハンドラが既定へ
		 * 戻る頃にはもう残っていない。⇒ destroy() の kill(-pgid,SIGTERM) を撃ったのに
		 * 子が死なず、呼び手は kill の rc=0 を見て成功と信じる。実際に踏んだ利用側では、
		 * 撃った側が子の終了を待ち続け、子の寿命ぶん座り込んだ。
		 * ⚠ 窓は狭くない。下の close ループは getdtablesize() 回まわるので、
		 *   ulimit -n が大きい環境では **数十 ms** に達する (実測 524288 本で約 40ms)。
		 * ⇒ 子に入ったら **何よりも先に** 捕捉中のシグナルを既定へ戻し、mask を解く。
		 * ★ SIG_IGN は「exec を越えて継承される」ことが POSIX の仕様で、それに依存する
		 *   呼び手 (バックグラウンドジョブの SIGINT 等) が居るので **触らない**。
		 *   食われる原因はハンドラの方なので、これで足りる。 */
		{
		int sg;
		sigset_t unblock_all;
			for ( sg = 1 ; sg < NSIG ; sg ++ ) {
			struct sigaction cur;
				if ( sg == SIGKILL || sg == SIGSTOP )
					continue;
				/* ⚠ この continue は load-bearing。glibc の内部シグナル
				 * (SIGCANCEL=32 / SIGSETXID=33 ・ NPTL が使う) は
				 * **問い合わせ自体が EINVAL になる**ので、ここで避けている。
				 * 「エラーを無視しているだけ」ではない。 */
				if ( sigaction(sg,NULL,&cur) < 0 )
					continue;
				if ( cur.sa_handler == SIG_IGN || cur.sa_handler == SIG_DFL )
					continue;
				::signal(sg,SIG_DFL);
			}
			sigemptyset(&unblock_all);
			sigprocmask(SIG_SETMASK,&unblock_all,NULL);
		}
		signal(SIGPIPE,signal_handler_empty);
		if ( dmode & DM_TTY ) {
	                ::close(pipe_p2c[W]);
	                ::dup2(pipe_p2c[R],0);
        	        ::dup2(pipe_c2p[W],1);
  	        	::dup2(pipe_err[W],2);
	                ::close(pipe_p2c[R]);
		}
		else {
	                ::close(pipe_p2c[W]);
	                ::close(pipe_c2p[R]);
	                ::close(pipe_err[R]);
	                ::dup2(pipe_p2c[R],0);
        	        ::dup2(pipe_c2p[W],1);
  	        	::dup2(pipe_err[W],2);
	                ::close(pipe_p2c[R]);
        	        ::close(pipe_c2p[W]);
                	::close(pipe_err[W]);
		}

		/* 3 番以降を全部閉じる。0/1/2 は上で dup2 済みなので、ここから上は
		 * 「親から引き継いだかもしれない何か」しかない。
		 * ★ getdtablesize() は ulimit -n を返すので、素直にループすると
		 *   **system() 1 回につき close() が ulimit -n 回**走る。ulimit -n = 524288 の
		 *   機体では fork→exec が 40ms かかっていた (子プロセスを作る全経路に乗る固定費)。
		 * ⇒ Linux では close_range で 1 回の syscall に畳む。無い/古いカーネルでは
		 *   失敗が返るだけなので、そのままループへ落ちる。
		 * ⚠ ここは fork 後の子なので async-signal-safe なものだけ。syscall() は可、
		 *   opendir("/proc/self/fd") で数える手は malloc を踏むので採れない。 */
		rc = -1;
#if defined(__linux__) && defined(SYS_close_range)
		rc = ::syscall(SYS_close_range,(unsigned)3,~0U,0);
#endif
		if ( rc < 0 ) {
			rc = getdtablesize();
			for ( fd = 3 ; fd < rc ; fd ++ )
				::close(fd);
		}

		setpgid(0,0);	// always: put child in its own pgroup so kill(-pgid) reaches grandchildren
		if ( command[0] == '#' ) {


			if ( execvp(args_cmd->ary[0],(char**const)&args_ptr[0]) < 0 ) {
	                        ::close(pipe_p2c[R]);
				if ( !(dmode & DM_TTY) ) {
	        	                ::close(pipe_c2p[W]);
        	        	        ::close(pipe_err[W]);
				}
                        	_Exit(1);
			}
		}
		else
                if ( execlp("sh","sh","-c",command,NULL) < 0 ){
                        ::close(pipe_p2c[R]);
			if ( !(dmode & DM_TTY) ) {
	                        ::close(pipe_c2p[W]);
        	                ::close(pipe_err[W]);
			}
                        _Exit(1);
                }
        }

	/* 親の mask を戻す。止めていたのは fork の窓を無くすためだけなので、ここより先へ
	 * 持ち越さない。止めている間に届いたシグナルは pending で残っているので、この直後に
	 * 配送される — 遅れるだけで、落ちない。 */
	pthread_sigmask(SIG_SETMASK,&saved_sigmask,NULL);

	setpgid(pid, pid);	// race guard: parent also calls setpgid before any kill

	if ( !(dmode & DM_APPLY) ) {
	        soCLOSE(pipe_p2c[R]);
		if ( !(dmode & DM_TTY) ) {
	        	soCLOSE(pipe_c2p[W]);
	        	soCLOSE(pipe_err[W]);
		}
	        *fd_w = pipe_p2c[W];
        	*fd_r = pipe_c2p[R];
		*fd_err = pipe_err[R];
	}

        return pid;
}




/*******************************************
	STATE MACHINE
********************************************/



TS_STATE(INI_START)
{
	if ( d_rfd == thNULL && d_efd == thNULL )
		return rDO|ACT_FINISH;
	thNEW(ts2IOdevNull,(ifThis,d_rfd));
	thNEW(ts2IOdevNull,(ifThis,d_efd));
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
	if ( is_destroyed() )
		return rDO|ACT_FINISH;
	if ( d_rfd != thNULL && !C_TEST(d_rfd->tinyState::state(),C_ZOM) )
		return ACT_START;
	if ( d_efd != thNULL && !C_TEST(d_efd->tinyState::state(),C_ZOM) )
		return ACT_START;
	return rDO|ACT_FINISH;
}

TS_PRIVATE(sPtr<tsSignal>  sig;)
TS_STATE(ACT_FINISH)
{
	if ( this->ret < 0 )
		return rDO|FIN_START;
	sig =  thNEW( tsSignal,(ifThis,SIGCHLD));
	kill_timer = stdInterval::now();
	stdInterval::wait(ifThis,1000*1000,TSE_TIMER);
	return rDO|ACT_FINISH_RET;
}
TS_STATE(ACT_FINISH_RET)
{
int er;
	if ( detach_flag ) {
		sig->destroy();
		sig = thNULL;
		return rDO|FIN_START;
	}
	errno = 0;
	er = ::waitpid(this->ret,&status, WNOHANG);
	if ( er == 0 ) {
		if ( kill_flag && (destroy_mode == DM_CONT_TERM || destroy_mode == DM_CONT_KILL ) &&
				kill_timer + 1000*1000 <= stdInterval::now() ) {
		   	if ( destroy_mode == DM_CONT_TERM )
			     kill(-ret,SIGTERM);
			else
			     kill(-ret,SIGKILL);
		}
		if ( kill_timer + 1000*1000 >= stdInterval::now() ) {
			kill_timer = stdInterval::now();
			stdInterval::wait(ifThis,1000*1000,TSE_TIMER);
		}
		return 0;
	}
	sig->destroy();
	sig = thNULL;
	return rDO|FIN_START;
}
TS_STATE(FIN_START)
{
	this->parent->eventHandler(
		thNEW( stdEvent,(TSE_RETURN,ifThis,(INTEGER64)status)));
	if ( d_rfd != thNULL )
		d_rfd->destroy();
	if ( d_efd != thNULL )
		d_efd->destroy();
	d_rfd = thNULL;
	d_efd = thNULL;
	return rDO|FIN_TINYSTATE_START;
}


