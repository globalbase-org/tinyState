

#include	"ts2/c++/stdBalancedTree.h"


sBalancedTreeCondition
sBalancedTreeCondition::plusInfty = sBalancedTreeCondition(1);

sBalancedTreeCondition
sBalancedTreeCondition::minusInfty = sBalancedTreeCondition(-1);


sBalancedTreeCondition::sBalancedTreeCondition()
{
}

sBalancedTreeCondition::sBalancedTreeCondition(int inf)
{
	if ( inf < 0 )
		minusInf = 1;
	else if ( inf > 0 )
		plusInf = 1;
}

sBalancedTreeCondition::~sBalancedTreeCondition()
{
}

int
sBalancedTreeCondition::is_infty()
{
	if ( minusInf && plusInf == 0 )
		return -1;
	if ( plusInf && minusInf == 0 )
		return 1;
	return 0;
}


stdBalancedTreeElement_::stdBalancedTreeElement_(
	sPtr<stdBalancedTreeObject_> obj)
{
	this->obj = obj;
	level = 1;
}

stdBalancedTreeObject_::stdBalancedTreeObject_(sBalancedTreeCondition & c,sPtr<stdObject> o) 
	: cond(c)
{
	obj = o;
}

stdBalancedTreeObject_::~stdBalancedTreeObject_()
{
	obj = thNULL;
	next = thNULL;
}


stdBalancedTreeElement_::~stdBalancedTreeElement_()
{
	small = thNULL;
	large = thNULL;
	obj = thNULL;
}


void
stdBalancedTreeElement_::balanced()
{
	if ( levelSmall() + 2 <= levelLarge() ) {
	sPtr<stdBalancedTreeElement_>  x_large = large;
	sPtr<stdBalancedTreeElement_>  x_small = small;
	sPtr<stdBalancedTreeObject_>  x_obj = obj;

	sPtr<stdBalancedTreeElement_>  z_large = large->large;
	sPtr<stdBalancedTreeElement_>  z_small = large->small;
	sPtr<stdBalancedTreeObject_>  z_obj = large->obj;

	sPtr<stdBalancedTreeElement_>  new_z = this;
	sPtr<stdBalancedTreeElement_>  new_x = large;

		new_z->obj = z_obj;
		new_z->large = z_large;
		new_z->small = new_x;

		new_x->obj = x_obj;
		new_x->large = z_small;
		new_x->small = x_small;

		new_x->leveled();
	}
	else if ( levelSmall() >= levelLarge() + 2 ) {
	sPtr<stdBalancedTreeElement_>  x_large = large;
	sPtr<stdBalancedTreeElement_>  x_small = small;
	sPtr<stdBalancedTreeObject_>  x_obj = obj;

	sPtr<stdBalancedTreeElement_>  z_large = small->large;
	sPtr<stdBalancedTreeElement_>  z_small = small->small;
	sPtr<stdBalancedTreeObject_>  z_obj = small->obj;

	sPtr<stdBalancedTreeElement_>  new_z = this;
	sPtr<stdBalancedTreeElement_>  new_x = small;

		new_z->obj = z_obj;
		new_z->large = new_x;
		new_z->small = z_small;

		new_x->obj = x_obj;
		new_x->large = x_large;
		new_x->small = z_large;

		new_x->leveled();
	}
	leveled();
}

sPtr<stdBalancedTreeObject_> 
stdBalancedTreeElement_::ins(sPtr<stdBalancedTreeObject_>  inp,int enabledEqu)
{
int r;
sPtr<stdBalancedTreeObject_>  ret;
	ret = thNULL;
	r = obj->cond.cmp(inp->cond);
	if ( r < 0 ) {
		if ( large == thNULL )
			large = thNEW( stdBalancedTreeElement_,(inp));
		else	ret = large->ins(inp,enabledEqu);
	}
	else if ( r > 0 ) {
		if ( small == thNULL )
			small = thNEW( stdBalancedTreeElement_,(inp));
		else 	ret = small->ins(inp,enabledEqu);
	}
	else {
		if ( enabledEqu == 0 )
			return obj;
		inp->next = obj;
		obj = inp;
	}
	if ( ret.is_notNull() )
		return ret;
	balanced();
	return thNULL;
}


sPtr<stdBalancedTreeObject_> 
stdBalancedTreeElement_::del(sPtr<stdBalancedTreeObject_>  inp,sPtr<stdBalancedTreeElement_> * posThis)
{
int r;
	r = obj->cond.cmp(inp->cond);
	if ( r < 0 ) {
	sPtr<stdBalancedTreeObject_>  ret;
		if ( large == thNULL ) {
			if ( inp->cond.is_infty() > 0 )
				goto equProc;
			return thNULL;
		}
		ret = large->del(inp,&large);
		if ( ret == thNULL )
			return thNULL;
		balanced();
		return ret;
	}
	else if ( r > 0 ) {
	sPtr<stdBalancedTreeObject_>  ret;
		if ( small == thNULL ) {
			if ( inp->cond.is_infty() < 0 )
				goto equProc;
			return thNULL;
		}
		ret = small->del(inp,&small);
		if ( ret == thNULL )
			return thNULL;
		balanced();
		return ret;
	}
	else {
	equProc:
		if ( inp->obj.is_notNull() ) {
		sPtr<stdBalancedTreeObject_> * pp;
			for ( pp = &obj ; (*pp).is_notNull() ; pp = &(*pp)->next )
				if ( (*pp)->obj == inp->obj ) {
					*pp = (*pp)->next;
					return inp;
				}
			return thNULL;
		}
	sPtr<stdBalancedTreeObject_> ret;
		 ret = obj;
	int s = levelSmall();
	int l = levelLarge();
		if ( s < l ) {
			obj = large->del(
				thNEW( stdBalancedTreeObject_,(sBalancedTreeCondition::minusInfty,thNULL)),&large);
		}
		else if ( s ) {
			obj = small->del(
				thNEW( stdBalancedTreeObject_,(sBalancedTreeCondition::plusInfty,thNULL)),&small);
		}
		else {
			*posThis = thNULL;
		}
		return ret;
	}
}

void
stdBalancedTreeElement_::walk(
	sPtr<stdBalancedTree_>  parent,
	sBalancedTreeCondition & c1,
	sPtr<stdBalancedTreeCallback>  cb)
{
int r = obj->cond.cmp(c1);
	if ( r > 0 ) {
		if ( small.is_notNull() )
			small->walk(parent,c1,cb);
	}
	if ( r >= 0 ) {
	sPtr<stdBalancedTreeObject_>  op;
		op = obj;
		for ( ; op.is_notNull() ; op = op->next ) 
			if ( parent->callback(c1,cb,obj->cond,obj->obj) )
				return;
	}
	if ( large.is_notNull() )
		large->walk(parent,c1,cb);
}

stdBalancedTree_::~stdBalancedTree_()
{
	top = thNULL;
}

stdBalancedTree_::stdBalancedTree_(int enabledEqu)
{
	this->enabledEqu = enabledEqu;
}


sPtr<stdBalancedTreeObject_>  
stdBalancedTree_::search(sBalancedTreeCondition & cond)
{
sPtr<stdBalancedTreeElement_>  e;
	e = top;
	for ( ; e.is_notNull() ; ) {
	int r = e->obj->cond.cmp(cond);
		if ( r == 0 )
			return e->obj;
		if  ( r < 0 )
			e = e->large;
		else 	e = e->small;
	}
	return thNULL;
}


sPtr<stdBalancedTreeObject_> 
stdBalancedTree_::ins(sPtr<stdBalancedTreeObject_>  obj)
{
sPtr<stdBalancedTreeObject_>  ret;
	if ( top == thNULL ) {
		top = thNEW( stdBalancedTreeElement_,(obj));
		ret = obj;
	}
	else {
		ret = top->ins(obj,enabledEqu);
	}
	if ( ret.is_notNull() )
		_count ++;
	return ret;
}



sPtr<stdBalancedTreeObject_> 
stdBalancedTree_::del(sPtr<stdBalancedTreeObject_>  obj)
{
sPtr<stdBalancedTreeObject_>  ret, p;
	if ( top == thNULL )
		return thNULL;
	ret = top->del(obj,&top);
	p = ret;
	for ( ; p.is_notNull() ; p = p->next )
		_count --;
	return ret;
}



void
stdBalancedTree_::walk(
	sBalancedTreeCondition & c1,
	sPtr<stdBalancedTreeCallback>  cb)
{
	if ( top == thNULL )
		return;
	top->walk(this,c1,cb);
}


