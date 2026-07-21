

#include	"_ts2/c++/ts2IO_.h"

CLASS_TINYSTATE(ts2/c++/ts2IO,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT


#include	"ts2/c++/sPicoState.h"

/**
 * @brief ノンブロッキング I/O ストリームの抽象基底クラス。/ Abstract base class for non-blocking I/O streams.
 * @details
 * `ts2IO` は tinyState フレームワーク内で fd・ソケット・PTY 等のバイトストリームを扱う抽象クラス。
 * 実装は `ts2IOdescriptor`・`ts2IOsocket` 等の派生クラスが提供する。
 * / Abstract base for byte-stream I/O (fd, socket, PTY, …) in the tinyState framework.
 * Concrete implementations are `ts2IOdescriptor`, `ts2IOsocket`, etc.
 *
 * @par ライフサイクル契約 / Lifecycle contract
 * - **未使用なら自動回収される。** 構築後 read / write / read_c / write_c / read_s /
 *   write_s / seek を一度も呼ばなければ、この I/O はリアクタ (`fwIO`) に何も登録せず、
 *   自分をどこにも pin しない。よって所有者が sPtr を手放した時点で自動的に回収される。
 *   → `ts2System` が返す `rfd` / `wfd` / `efd` のうち<b>使わないストリームはそのまま無視してよい</b>。
 * - **一度でも I/O を呼んだら、最後に必ず `destroy()` する。** read / write は EAGAIN 時に
 *   呼び出し元 tinyState (POSIX) または自分自身 (MinGW: 発行済み overlapped の keep-alive pin)
 *   をリアクタに繋いで yield する。<b>着信が無いまま放置すると完了イベントが永久に来ないため、
 *   その I/O は回収されず残り続ける</b> (呼び出し元 tinyState もリアクタのキューに繋がれたまま
 *   なので、そちらも自動回収されない)。`destroy()` は保留中 I/O をキャンセルし、リアクタから
 *   detach し、fd/HANDLE を close するので、これで初めて全体が確実に回収される。
 * - これはフレームワークの設計方針: 呼び出し元の消滅を監視して自動回収する方式は取らない
 *   (read/write ごとに監視コストが乗り、かつ parked な呼び出し元自身がリアクタに pin される)。
 *   <b>明示的な `destroy()` によるライフサイクル制御はプログラマの責任</b>とする。
 * / - **Unused streams self-collect.** If you never call read/write/read_c/write_c/
 *   read_s/write_s/seek after construction, the stream registers nothing with the
 *   reactor (`fwIO`) and pins nothing, so it is reclaimed as soon as its owner drops
 *   its sPtr. Hence any of the `rfd`/`wfd`/`efd` streams returned by `ts2System` that
 *   you do not use may simply be ignored.
 * - **Once you call any I/O, you MUST `destroy()` it at the end.** On EAGAIN, read/write
 *   park the calling tinyState (POSIX) or the stream itself (MinGW: keep-alive pin for the
 *   posted overlapped) in the reactor and yield. If abandoned with no incoming data the
 *   completion never arrives, so the I/O — and the parked caller, itself still linked in
 *   the reactor's queue — lingers forever. `destroy()` cancels the pending I/O, detaches
 *   from the reactor and closes the fd/HANDLE; only then is everything reclaimed.
 * - By design the framework does NOT auto-reclaim on caller death (it would add per-call
 *   watch overhead, and the parked caller is itself pinned in the reactor): explicit
 *   lifecycle control via `destroy()` is the programmer's responsibility.
 *
 * **read_c / write_c の yield-resume セマンティクス:**
 *
 * `read_c` / `write_c` は sPicoState (`ps_read_c` / `ps_write_c`) で部分進捗を保持し、
 * データが揃うまで sException で yield → 状態関数を先頭から再実行する。
 * このため以下のルールを守ること:
 *
 * 1. **1 状態に read_c / write_c は 1 回だけ。**
 *    2 回以上呼ぶと、後段 yield からの再実行で前段が再走し副作用が二重化する。
 * 2. **読み書きバッファはメンバ変数にする。**
 *    stack ローカル変数は yield 再実行で別アドレスになり、sPicoState の bp ポインタが無効化する。
 * 3. **イベント分岐の中で呼ばない。**
 *    yield 後の再実行では `ev` が I/O 準備イベントに変わるため分岐が成立せず再開不能になる。
 *    → イベント検出は専用状態で行い、I/O は別の ev 非依存状態で 1 回だけ呼ぶ。
 *
 * @code{.cpp}
 * // ✓ 正しい使い方: バッファをメンバに、1 状態 = read_c 1 回
 * TS_PRIVATE(char hdr[8];)
 * TS_STATE(ACT_READ_HDR) {
 *     if ( rfd->read_c(hdr, 8) != 8 ) return rDO|FIN_START;
 *     return rDO|ACT_READ_BODY;
 * }
 *
 * // ✗ NG: 同じ状態で read_c を 2 回
 * TS_STATE(ACT_READ) {
 *     rfd->read_c(hdr, 8);    // 後段 yield の再入でこれが再走する
 *     rfd->read_c(body, len); // ← 危険
 * }
 * @endcode
 *
 * ---
 *
 * `read_c` / `write_c` use sPicoState to hold partial progress across sException yields.
 * Rules: (1) call at most once per TS_STATE, (2) buffer must be a member variable,
 * (3) do not call inside an event-guarded branch.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	ts2IO_(
		sPtr<tinyState> parent);
	void inherit(
		sPtr<tinyState> parent);

	/** @brief 生読み込み (サブクラス実装)。/ Raw read — implemented by subclass.
	 * @param buf    読み込みバッファ。/ Destination buffer.
	 * @param length 要求バイト数。/ Requested byte count.
	 * @return 実際に読んだバイト数。0 は EOF、負値は EAGAIN 等のエラー。
	 *         / Bytes actually read. 0 = EOF; negative = EAGAIN or error.
	 */
	virtual int read(void * buf,int length);
	/** @brief 生書き込み (サブクラス実装)。/ Raw write — implemented by subclass.
	 * @param buf    書き込みバッファ。/ Source buffer.
	 * @param length 書き込みバイト数。/ Byte count to write.
	 * @return 実際に書いたバイト数。負値はエラー。/ Bytes actually written; negative = error.
	 */
	virtual int write(void * buf,int length);
	/** @brief シーク (サブクラス実装)。fd がシーク可能な場合のみ有効。
	 *  / Seek — implemented by subclass; only valid for seekable fds. */
	virtual INTEGER64 seek(INTEGER64 pos,int where);

	/** @brief `length` バイト読み切るまで yield しながら待つ。/ Read exactly `length` bytes, yielding until complete.
	 * @details
	 * sPicoState で部分進捗を保持する。データ不足時は sException を投げて状態関数を yield させ、
	 * fd が読める状態になると先頭から再実行される (部分読みは自動再開)。<br>
	 * **制約**: 1 TS_STATE につき 1 回まで。`buf` はメンバ変数であること。<br>
	 * / Holds partial progress in sPicoState; throws sException on EAGAIN and resumes when ready.
	 * **Constraint**: at most once per TS_STATE; `buf` must be a member variable.
	 * @param buf    読み込み先バッファ (メンバ変数)。/ Destination buffer (must be a member variable).
	 * @param length 読み込みバイト数。/ Exact byte count to read.
	 * @return 成功時は `length`。EOF・エラー時は ≤ 0。/ `length` on success; ≤ 0 on EOF or error.
	 */
  	int read_c(void * buf,int length);
	/** @brief `length` バイト書き切るまで yield しながら待つ。/ Write exactly `length` bytes, yielding until complete.
	 * @details
	 * `read_c` と同じ sPicoState による yield-resume 機構。<br>
	 * **制約**: 1 TS_STATE につき 1 回まで。`buf` はメンバ変数であること。<br>
	 * / Same yield-resume mechanism as `read_c`.
	 * **Constraint**: at most once per TS_STATE; `buf` must be a member variable.
	 * @param buf    書き込み元バッファ (メンバ変数)。/ Source buffer (must be a member variable).
	 * @param length 書き込みバイト数。/ Exact byte count to write.
	 * @return 成功時は `length`。エラー時は ≤ 0。/ `length` on success; ≤ 0 on error.
	 */
  	int write_c(void * buf,int length);
	/** @brief `write_c` を分割書き込みモードに切り替える。/ Switch `write_c` to divisible (split-write) mode.
	 * @details
	 * 設定後、`write_c` は EAGAIN 時にサイズを半分に減らして即座にリトライする。
	 * パイプの空き容量がちょうど収まるサイズを binary search で探す動作になる。
	 * 書き込みデータが不可分でない場合 (メッセージ境界が不要な場合) に使う。<br>
	 * / After calling, `write_c` retries with half the size on EAGAIN, binary-searching
	 * for the largest chunk that fits in the pipe's available space.
	 * Use when the data stream has no atomicity requirement (no message boundaries).
	 */
	void set_divisible();
	/** @brief 分割書き込みモードかどうかを返す。/ Returns whether divisible (split-write) mode is active. */
	int  get_divisible();

	/** @brief 改行区切りで 1 行読む (非ブロッキング)。/ Read one newline-delimited line (non-blocking).
	 * @details
	 * 内部バッファに `\n` / `\r\n` が溜まるまで `read()` を繰り返す。
	 * 行末が見つからない場合は `thNULL` を返す (呼び出し元は次イベントで再試行)。
	 * EOF に達した場合は残りバイトを返す。<br>
	 * / Calls `read()` repeatedly until a newline appears in the internal buffer.
	 * Returns `thNULL` if no complete line yet; returns remaining bytes at EOF.
	 * @param length 内部バッファの最大サイズ (バイト)。/ Internal buffer size in bytes.
	 * @return 行文字列 (改行含む)、EOF 残余、またはデータ不足時 `thNULL`。
	 *         / Line string (including newline), EOF remainder, or `thNULL` if incomplete.
	 */
	sPtr<stdString> read_s(int length);
	/** @brief `stdString` を `write_c` で書き込む。/ Write a `stdString` via `write_c`.
	 * @details `read_c` / `write_c` と同じ yield-resume 制約が適用される。
	 *          / Same yield-resume constraints as `write_c` apply.
	 */
  	int write_s(sPtr<stdString> str);
	/** @brief C 文字列を `write_c` で書き込む。/ Write a C string via `write_c`. */
	int write_s(const char * str);

	/** @brief I/O 状態 (`TS2IO_INIT` / `TS2IO_ACTIVE` / `TS2IO_CLOSE` / `TS2IO_ERROR`)。
	 *  / I/O state (`TS2IO_INIT` / `TS2IO_ACTIVE` / `TS2IO_CLOSE` / `TS2IO_ERROR`). */
	int		state;
	/** @brief 最後のエラーコード (errno 相当)。/ Last error code (errno equivalent). */
	int		err;

	unsigned	invoke_flag:1;
	/** @brief 分割書き込みモードフラグ。set_divisible() で 1 にする。/ Split-write mode flag; set by set_divisible(). */
	unsigned	divisible_flag:1;
private:
protected:
	struct {
		PS_PRESET
		char *		bp;
		int		s;
	} ps_read_c;
	struct {
		PS_PRESET
		char *		bp;
		int		s;
	} ps_write_c;

	int write_div(void * buf,int length);


	int get_s();

	sArray<int8_t>		sBuffer;
	int			sBuffer_length;
	int			sBuffer_eof;
};

TS_END_IMPLEMENT

#endif


ts2IO_::ts2IO_(
		sPtr<tinyState> _parent)
        : tinyState_(_parent)
{
}

void
ts2IO_::inherit(
	sPtr<tinyState> _parent)
{
	this->TS_BASECLASS::inherit(_parent);
}

int 
ts2IO_::read(void * buf,int length)
{
	stdObject::panic("not implement");
	return -1;
}
int 
ts2IO_::write(void * buf,int length)
{
	stdObject::panic("not implement");
	return -1;
}

INTEGER64
ts2IO_::seek(INTEGER64 pos,int where)
{
	stdObject::panic("not implement");
	return -1;
}


/*******************************************
	INSTANCE FUNCTIONS
********************************************/


int
ts2IO_::read_c(void * buf,int length)
{
	PS_STATEMENT(ps_read_c,
		PS_DEF(bp)
		PS_DEF(s)
	,
	PS_STATE(psINI)
		bp = (char*)buf;
		s = length;
	PS_STATE(psDO)
		for ( ; s ; ) {
		int er;
			er = read(bp,s);
			if ( er <= 0 )
				PS_RETURN(er)
			bp += er;
			s -= er;
		}
		PS_RETURN(length);
	);
	return -1;
}

void
ts2IO_::set_divisible()
{
	divisible_flag = 1;
}

int
ts2IO_::get_divisible()
{
	return divisible_flag;
}

int
ts2IO_::write_div(void * buf,int length)
{
	/* Passthrough.  write() is partial-immediate on every backend (POSIX ::write,
	   MinGW internal ring), so "shrink and retry on sException" is obsolete — and
	   harmful: each retry re-registers the caller.  Divisible mode is now a no-op;
	   write_c loops on the partial return. */
	return write(buf,length);
}

int
ts2IO_::write_c(void * buf,int length)
{
	PS_STATEMENT(ps_write_c,
		PS_DEF(bp)
		PS_DEF(s)
	,
	PS_STATE(psINI)
		bp = (char*)buf;
		s = length;
	PS_STATE(psDO)
		for ( ; s ; ) {
		int er;
			if ( divisible_flag )
				er = write_div(bp,s);
			else
				er = write(bp,s);
			if ( er <= 0 )
				PS_RETURN(er)
			bp += er;
			s -= er;
		}
		PS_RETURN(length);
	);
	return -1;
}


int
ts2IO_::get_s()
{
int i;
	for ( i = 0 ; i < sBuffer_length &&
		sBuffer[i] != '\r' &&
		sBuffer[i] != '\n' ; i ++ );
	if ( i == sBuffer_length ) {
		if ( sBuffer_eof )
			return i;
		return -1;
	}
	switch ( sBuffer[i] ){
	case '\r':
		if ( i+1 == sBuffer_length )
			return i+1;
		if ( sBuffer[i+1] == '\n' )
			return i+2;
		return i+1;
	case '\n':
		return i+1;
	}
	return -1;
}

sPtr<stdString>
ts2IO_::read_s(int length)
{
	if ( sBuffer.length() < length )
		sBuffer.length(length);
int ret;
	for ( ; ; ) {
		ret = get_s();
		if ( ret > 0 ) {
			if ( ret > length-1 ) 
				ret = length-1;
			break;
		}
		if ( ret == 0 )
			return thNEW(stdString,(""));
	int er;
		er = read(&sBuffer[sBuffer_length],sBuffer.length()-sBuffer_length);
		if ( er < 0 )
			return thNULL;
		if ( er == 0 )
			sBuffer_eof = 1;
		else 	sBuffer_length += er;
	}
sPtr<stdString> str;
	str = thNEW(stdString,((const char*)&sBuffer[0],0,ret));
	memmove(&sBuffer[0],&sBuffer[ret],sBuffer_length-ret);
	sBuffer_length -= ret;
	return str;
}

int
ts2IO_::write_s(sPtr<stdString> str)
{
	return write_c((void*)str->get_str(),str->length());
}

int
ts2IO_::write_s(const char * str)
{
	return write_c((void*)str,strlen(str));
}

/*******************************************
	STATE MACHINE
********************************************/


TS_STATE(INI_START)
{
	return rDO|INI_ts2IO_START;
}
TS_STATE(INI_ts2IO_START)
{
	return rDO|ACT_PREV;
}
TS_STATE(ACT_PREV)
{
	invoke_listen([this](int*tp){
		return tp ? *tp=TSE_ASSERT,thNULL : thNEW(stdEvent,(TSE_ASSERT,ifThis,thNULL));
	});
	invoke_flag = 1;
	return rDO|ACT_START;
}


TS_STATE(ACT_ERROR)
{
	state = TS2IO_ERROR;
	invoke_listen([this](int*tp){
		return tp ? *tp=TSE_ASSERT,thNULL : thNEW(stdEvent,(TSE_ASSERT,ifThis,thNULL));
	});
	invoke_flag = 1;
	return rDO|FIN_START;
}


TS_STATE(FIN_START)
{
	return rDO|FIN_ts2IO_START;
}
TS_STATE(FIN_ts2IO_START)
{
	if ( invoke_flag == 0 ) {
		invoke_listen([this](int*tp){
			return tp ? *tp=TSE_ASSERT,thNULL : thNEW(stdEvent,(TSE_ASSERT,ifThis,thNULL));
		});
		invoke_flag = 1;
	}
	return rDO|FIN_TINYSTATE_START;
}
