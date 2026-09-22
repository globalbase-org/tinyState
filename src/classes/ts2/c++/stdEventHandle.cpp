
#include	"ts2/c++/stdEventHandle.h"


stdEventHandle::stdEventHandle(
	sPtr<tinyState>  source,
	sPtr<tinyState>  listener,
	int type, 
	TS_HANDLER_FUNC handler)
{
	if ( source == thNULL )
		stdObject::panic("source is NULL");
	if ( listener == thNULL )
		stdObject::panic("listener is NULL");
        this->source = source;
        this->listener = listener;
        this->type = type;
        this->handler = handler;
}

stdEventHandle::~stdEventHandle()
{
	this->remove();
}


/* 両端からの取り外しはここが駆動する。this への書き込み (source / listener の
 * クリア) をすべて外部呼び出しの *前* に済ませ、参照はローカルへ移しておく。
 * こうすると呼び出しから戻った後に this を一切触らないので、取り外しで自分の最後の
 * 参照が落ちても安全に抜けられる (src / lsn はスタック上なので、誰が死のうと
 * 関数の終わりまで生きる)。
 *
 * デストラクタからもここを呼ぶ。ref == -1 の間は addref / relref が no-op なので、
 * source->remove_listener(this) の一時 sPtr はカウントを動かさない。 */
void
stdEventHandle::remove()
{
sPtr<tinyState>  src = source;
sPtr<tinyState>  lsn = listener;
	source   = thNULL;
	listener = thNULL;
	if ( src.is_notNull() )
		src->remove_listener(this);	/* source 側のキューから外す */
	if ( lsn.is_notNull() )
		lsn->remove_handle(this);	/* listener 側のリストから外す */
}
