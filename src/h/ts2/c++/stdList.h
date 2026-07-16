
#ifndef ___stdList_H_cpp___
#define ___stdList_H_cpp___

template<class __TYPE>
class stdList : public stdObject {
public:
	sPtr<__TYPE> 	next;
	sPtr<__TYPE>&	head;
	sPtr<__TYPE>&	tail;
	stdList(sPtr<__TYPE>& _head,sPtr<__TYPE>& _tail)
		: head(_head),
		  tail(_tail)
	{
		if ( head.is_notNull() ) {
			tail->next = 
				sPtr<__TYPE>::d_cast(thThis);
			tail = 
				sPtr<__TYPE>::d_cast(thThis);
		}
		else {
			head = 
				sPtr<__TYPE>::d_cast(thThis);
			tail = 
				sPtr<__TYPE>::d_cast(thThis);
		}
	}
	~stdList() {
		next = thNULL;
	}
	void del() {
	sPtr<__TYPE>* pp;
	sPtr<__TYPE> tt;
		if ( head != sPtr<__TYPE>::d_cast(thThis) ) {
			for ( pp = &head ; (*pp).is_notNull() ; pp = &(*pp)->next )
				if ( *pp == thThis ) {
					*pp = next;
					break;
				}
			if ( thThis != tail )
				return;
			if ( head == thNULL ) {
				tail = thNULL;
				return;
			}
			for ( tt = head ; tt->next.is_notNull() ; tt = tt->next )
			  	;
			tail = tt->next;
			return;
		}
		head = next;
		if ( head == thNULL )
			tail = thNULL;
	}
};

#endif
