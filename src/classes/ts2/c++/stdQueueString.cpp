

#include	"ts2/c++/stdQueueString.h"

stdQueueString::stdQueueString(sPtr<stdQueue<stdString> >  str)
{
		str = thNEW( stdQueue<stdString>,(str));
		head = str->head;
		tail = str->head;
		count = str->count;
}

sPtr<stdQueueString> 
stdQueueString::filter(sPtr<stdArray<sPtr<stdString> > > flt)
{
sPtr<stdString>  target;
sPtr<stdQueueElement<stdString> > elp1,elp;
sPtr<stdArray<uint8_t> > flags;
int i,j;
int len,slen,qlen;
const char* str;
#define MATCH_ALL 	0
#define MATCH_PARTIAL	1
#define MATCH_FORWARD	2
#define MATCH_BIHIND	3
sPtr<stdQueueString>  ret;

	ret = thNEW( stdQueueString,());
	if ( flt == thNULL || flt->length() == 0 )
		return ret;
	len = flt->length();
	flags = thNEW( stdArray<uint8_t>,(len));
	for ( i = 0 ; i < len ; i ++ ) {
		str = flt->ary[i]->get_str();
		slen = strlen(str);
		if ( str[0] == '^' ) {
			if ( str[slen-1] == '$' )
				flags->ary[i] = MATCH_ALL;
			else 	flags->ary[i] = MATCH_FORWARD;
		}
		else {
			if ( str[slen-1] == '$' )
				flags->ary[i] = MATCH_BIHIND;
			else	flags->ary[i] = MATCH_PARTIAL;
		}
	}

	for ( elp1 = head ; elp1.is_notNull() ; elp1 = elp1->next ) {
		for ( i = 0 ; i < len ; i ++ ) {
			switch ( flags->ary[i] ) {
			case MATCH_ALL:
				slen = flt->ary[i]->length();
				if ( elp1->data->length() != slen-2 )
					break;
				if ( memcmp(elp1->data->get_str(),
						&flt->ary[i]->get_str()[1],
						slen-2) == 0 ) {
					ret->ins(MAX_INTEGER64,elp1->data);
					continue;
				}
				break;
			case MATCH_FORWARD:
				slen = flt->ary[i]->length();
				if ( elp1->data->length() < slen-1 )
					break;
				if ( memcmp(elp1->data->get_str(),
						&flt->ary[i]->get_str()[1],
						slen-1) == 0 ) {
					ret->ins(MAX_INTEGER64,elp1->data);
					continue;
				}
				break;
			case MATCH_BIHIND:
				slen = flt->ary[i]->length();
				qlen = elp1->data->length();
				if ( qlen < slen-1 )
					break;
				if ( memcmp(&elp1->data->get_str()[qlen-slen+1],
						flt->ary[i]->get_str(),
						slen-1) == 0 ) {
					ret->ins(MAX_INTEGER64,elp1->data);
					continue;
				}
				break;
			case MATCH_PARTIAL:
				slen = flt->ary[i]->length();
				qlen = elp1->data->length();
				if ( qlen < slen )
					break;
				for ( j = 0 ; j < qlen-slen ; j ++ ) {
					if ( memcmp(&elp1->data->get_str()[j],
							flt->ary[i]->get_str(),
							slen-1) == 0 ) {
						ret->ins(MAX_INTEGER64,elp1->data);
						continue;
					}
				}
				break;
			}
		}
	}
	return ret;
}



sPtr<stdQueueString> 
stdQueueString::add(
	sPtr<stdQueueString>  a2)
{
sPtr<stdQueueElement<stdString> > elp1,elp2;
sPtr<stdQueueString>  ret;
	elp1 = this->head;
	elp2 = a2->head;
	ret = thNEW( stdQueueString,());
	for ( ; elp1.is_notNull() && elp2.is_notNull() ; ) {
	int r;
		r = STR_NULL(elp1->data)->cmp(elp2->data);
		if ( r < 0 ) {
			ret->ins(MAX_INTEGER64,elp1->data);
			elp1 = elp1->next;
		}
		else if ( r > 0 ) {
			ret->ins(MAX_INTEGER64,elp2->data);
			elp2 = elp2->next;
		}
		else {
			ret->ins(MAX_INTEGER64,elp1->data);
			elp1 = elp1->next;
			elp2 = elp2->next;
		}
	}
	for ( ; elp1.is_notNull() ; elp1 = elp1->next )
		ret->ins(MAX_INTEGER64,elp1->data);
	for ( ; elp2.is_notNull() ; elp2 = elp2->next )
		ret->ins(MAX_INTEGER64,elp2->data);
	return ret;
}

sPtr<stdQueueString> 
stdQueueString::sub(
	sPtr<stdQueueString>  a2)
{
sPtr<stdQueueElement<stdString> > elp1,elp2;
sPtr<stdQueueString>  ret;
	elp1 = this->head;
	elp2 = a2->head;
	ret = thNEW( stdQueueString,());
	for ( ; elp1.is_notNull() && elp2.is_notNull() ; ) {
	int r;
		r = STR_NULL(elp1->data)->cmp(elp2->data);
		if ( r < 0 ) {
			ret->ins(MAX_INTEGER64,elp1->data);
			elp1 = elp1->next;
		}
		else if ( r > 0 ) {
			elp2 = elp2->next;
		}
		else {
			elp1 = elp1->next;
			elp2 = elp2->next;
		}
	}
	for ( ; elp1.is_notNull() ; elp1 = elp1->next )
		ret->ins(MAX_INTEGER64,elp1->data);
	return ret;
}

sPtr<stdQueueString> 
stdQueueString::add(
	sPtr<stdQueue<stdString> >  a2)
{
	return add(thNEW( stdQueueString,(a2)));
}

sPtr<stdQueueString> 
stdQueueString::sub(
	sPtr<stdQueue<stdString> >  a2)
{
	return sub(thNEW( stdQueueString,(a2)));
}
