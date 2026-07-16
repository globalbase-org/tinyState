

#include	"ts2/c++/sCallSection.h"


sThreadKey<sCallSection> 
sCallSection::key;

sCallSection::sCallSection()
{
	list = 0;
}

sCallSection::~sCallSection()
{
	list = 0;
}

void
sCallSection::push(sCallSectionNode * n)
{
	n->next = list;
	list = n;
}

sPtr<tinyState>
sCallSection::pop(sCallSectionNode * n)
{
sPtr<tinyState> ret;
	if ( list != n )
		sObject::panic("ENTER_CALL is required");
	ret = list->ts;
	list = list->next;
	return ret;
}


sPtr<tinyState>
sCallSection::caller()
{
	if ( list )
		return list->ts;
	return thNULL;
}
