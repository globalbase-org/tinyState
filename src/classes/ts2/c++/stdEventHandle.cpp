
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
	if ( source.is_notNull() )
		source->remove_listener(this);
	source = thNULL;
	listener = thNULL;
}

void
stdEventHandle::remove()
{
	if ( source.is_notNull() )
		source->remove_listener(this);
	source = thNULL;
	listener = thNULL;
}
