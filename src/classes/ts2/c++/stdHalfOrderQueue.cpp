

#include	"ts2/c++/stdHalfOrderQueue.h"


stdHalfOrderQueue::stdHalfOrderQueue()
{
}
stdHalfOrderQueue::~stdHalfOrderQueue()
{
	top = thNULL;
}
void
stdHalfOrderQueue::ins(int key,sPtr<stdObject>  data)
{
	if ( top == thNULL ) {
		top = thNEW( stdHalfOrderNode,(key,data));
		return;
	}
	_ins(top,key,data);
}
void
stdHalfOrderQueue::_ins(sPtr<stdHalfOrderNode>  np,int key,sPtr<stdObject>  data)
{
sPtr<stdObject>  tmp_data;
int tmp_key;
int swap;
	np->count ++;
	swap = -1;
	if ( np->n[0] == thNULL ) {
	  if ( np->n[1].is_notNull() )
			stdObject::panic("invalid node");
		np->n[0] = thNEW( stdHalfOrderNode,(key,data));
		if ( np->n[0]->key >= np->key )
			return;
		swap = 0;
	}
	else if ( np->n[1] == thNULL ) {
		np->n[1] = thNEW( stdHalfOrderNode,(key,data));
	}
	else {
		if ( np->n[0]->count <= np->n[1]->count )
			_ins(np->n[0],key,data);
		else 	_ins(np->n[1],key,data);
	}
	if ( swap < 0 ) {
		if ( np->key <= np->n[0]->key &&
				np->key <= np->n[1]->key )
			return;
		if ( np->n[0]->key < np->key &&
				np->n[0]->key < np->n[1]->key )
			swap = 0;
		else 	swap = 1;
	}
	if ( np->key <= np->n[swap]->key )
		return;
	tmp_data = np->data;
	tmp_key = np->key;
	np->data = np->n[swap]->data;
	np->key = np->n[swap]->key;
	np->n[swap]->data = tmp_data;
	np->n[swap]->key = tmp_key;
}
sPtr<stdObject> 
stdHalfOrderQueue::del(int * kp)
{
sPtr<stdObject>  ret;
sPtr<stdHalfOrderNode> np, prev;
int swap;
	if ( top == thNULL )
		return thNULL;
	ret = top->data;
	if ( kp )
		*kp = top->key;
	if ( top->n[0] == thNULL ) {
		top = thNULL;
		return ret;
	}
	prev = thNULL;
	swap = 0;
	for ( np = top ; np.is_notNull() ; ) {
		np->count --;
		if ( np->n[0] == thNULL ) {
			if ( prev == thNULL )
				stdObject::panic("del??");
			top->data = np->data;
			top->key = np->key;
			if ( swap == 0 ) {
				prev->n[0] = prev->n[1];
				prev->n[1] = thNULL;
			}
			else	prev->n[1] = thNULL;
			break;
		}
		if ( np->n[1] == thNULL )
			swap = 0;
		else if ( np->n[0]->count >= np->n[1]->count )
			swap = 0;
		else	swap = 1;
		prev = np;
		np = prev->n[swap];
	}
	for ( np = top ; np.is_notNull() ; ) {
	int tmp_key;
	sPtr<stdObject>  tmp_data;
		if ( np->n[0] == thNULL )
			break;
		if ( np->n[1] == thNULL ) {
			if ( np->key <= np->n[0]->key )
				break;
			swap = 0;
		}
		else {
			if ( np->key <= np->n[0]->key &&
					np->key <= np->n[1]->key )
				break;
			if ( np->n[0]->key <= np->key &&
					np->n[0]->key <= np->n[1]->key )
				swap = 0;
			else	swap = 1;
		}
		tmp_data = np->data;
		tmp_key = np->key;
		np->data = np->n[swap]->data;
		np->key = np->n[swap]->key;
		np->n[swap]->data = tmp_data;
		np->n[swap]->key = tmp_key;
		np = np->n[swap];
	}
	return ret;
}
int
stdHalfOrderQueue::count()
{
	if ( top == thNULL )
		return 0;
	return top->count;
}

