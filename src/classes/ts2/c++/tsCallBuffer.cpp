
#include	"_ts2/c++/tsCallBuffer_.h"

CLASS_TINYSTATE(tsCallBuffer,tinyState)


#if 0
TS_BEGIN_IMPLEMENT


class TS_THISCLASS : public TS_BASECLASS {
public:
	tsCallBuffer_(sPtr<tinyState>  parent);
	void inherit(sPtr<tinyState>  parent);
	void push(sPtr<stdObject> );
	sPtr<tsCallBuffer>  ignore();

	int status;
private:
	sPtr<stdQueue<stdObject> > 	data;
	unsigned 		ignore_flag:1;
};
TS_END_IMPLEMENT
#endif

/*******************************************
	PUBLIC FUNCTIONS
********************************************/

tsCallBuffer_::tsCallBuffer_(
	sPtr<tinyState>  parent)
	:
	tinyState_(parent)
{
	this->data = 	thNEW( stdQueue<stdObject>,());
}

void
tsCallBuffer_::inherit(
	sPtr<tinyState>  parent)
{
	this->TS_BASECLASS::inherit(parent);
}


void
tsCallBuffer_::push(sPtr<stdObject>  obj)
{
	this->data->ins(0,obj);
}


sPtr<tsCallBuffer> 
tsCallBuffer_::ignore()
{
	this->ignore_flag = 1;
	return ifThis;
}

/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	return ACT_START;
}

TS_STATE(ACT_START)
{
	if ( is_destroyed() )
		return rDO|FIN_START;
	if ( ev->type != TSE_RETURN )
		return 0;
	ev->source->listen(parent,TSE_DESTROY);
	status = ev->msg_int;
	if ( this->ignore_flag )
		return rDO|FIN_START;
	this->parent->eventHandler(ev);
	return rDO|FIN_START;
}
TS_STATE(FIN_START)
{
	this->data = thNULL;
	return rDO|FIN_TINYSTATE_START;
}

