

#include	"ts2/c++/sCallSection.h"


sThreadKey<sCallSection>
sCallSection::key;

/* thread_local の実体はこの翻訳単位だけに置く (宣言は sThreadKey.h)。
 * ヘッダに inline のままだと exe と DLL がそれぞれ自分のスロットを持ち、
 * PE には ELF の STB_GNU_UNIQUE に相当する統一機構が無いので複数実体になる。
 * docs/GOTCHAS.md §13。 */
template<> sCallSection *
sThreadKey<sCallSection>::operator -> () const
{
	struct holder {
		sCallSection * p;
		holder() : p(0) {}
		~holder() { if ( p ) delete p; }
	};
	thread_local holder h;
	if ( !h.p )
		h.p = new(__FILE__,__LINE__) sCallSection();
	return h.p;
}

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
