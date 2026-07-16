

#include	<stdlib.h>
#include	<string.h>
#include	"ts2/c++/stdEvent.h"

stdEvent::~stdEvent()
{
	this->msg_obj = thNULL;
	this->source = thNULL;
}

stdEvent::stdEvent(
	int type,
	sPtr<tinyState>  source,
	INTEGER64 msg_int)
{
	this->type = type;
	this->seq = stdObject::seq();
	this->source = source;
	this->msg_int = msg_int;
	this->msg_obj = thNULL;
}

stdEvent::stdEvent(
	int type,
	sPtr<tinyState>  source,
	void * msg_ptr)
{
	this->type = type;
	this->seq = stdObject::seq();
	this->source = source;
	this->msg_ptr = msg_ptr;
	this->msg_obj = thNULL;

}

stdEvent::stdEvent(
	int type,
	sPtr<tinyState>  source,
	sPtr<stdObject>  msg_obj)
{
	this->type = type;
	this->seq = stdObject::seq();
	this->source = source;
	this->seq = stdObject::seq();
	this->msg_obj = msg_obj;

}


stdEvent::stdEvent(
	int type,
	sPtr<tinyState>  source,
	int	seq,
	INTEGER64 msg_int)
{
	this->type = type;
	if ( seq )
		this->seq = seq;
	else	this->seq = stdObject::seq();
	this->source = source;
	this->msg_int = msg_int;
	this->msg_obj = thNULL;

}


stdEvent::stdEvent(
	int type,
	sPtr<tinyState>  source,
	int	seq,
	void * msg_ptr)
{
	this->type = type;
	if ( seq )
		this->seq = seq;
	else	this->seq = stdObject::seq();
	this->source = source;
	this->msg_ptr = msg_ptr;
	this->msg_obj = thNULL;

}

stdEvent::stdEvent(
	int type,
	sPtr<tinyState>  source,
	int	seq,
	sPtr<stdObject>  msg_obj)
{
	this->type = type;
	if ( seq )
		this->seq = seq;
	else	this->seq = stdObject::seq();
	this->source = source;
	this->msg_int = 0;
	this->msg_obj = msg_obj;

}

stdEvent::stdEvent(
	int type,
	sPtr<tinyState>  source,
	int	seq,
	INTEGER64 msg_int,
	void * msg_ptr,
	sPtr<stdObject>  msg_obj)
{
	this->type = type;
	if ( seq )
		this->seq = seq;
	else	this->seq = stdObject::seq();
	this->source = source;
	this->msg_int = msg_int;
	this->msg_ptr = msg_ptr;
	this->msg_obj = msg_obj;

}


stdEvent::stdEvent(sPtr<stdEvent>  ev)
{
	this->type = ev->type;
	this->source = ev->source;
	this->seq = ev->seq;
	this->msg_int = ev->msg_int;
	this->msg_ptr = ev->msg_ptr;
	this->msg_obj = ev->msg_obj;

}
