


#ifndef ___stdArrayQueue_cpp_H___
#define ___stdArrayQueue_cpp_H___

#define AQ_DEBUG_MODE

#include	<pthread.h>

#define AQE_OK		0
#define AQE_ERROR 	(-1)

#define AQ_EMPTY	(-1)
#define AQ_INS		(-2)

#define AQ_SIZE		(sizeof(int)*8)


#define sAQ_INIT	0
#define sAQ_DETACH	1
#define sAQ_ATTACH	2


#define AQ_LINE(x)	(x).line(__FILE__,__LINE__);

template <class __TYPE>
class sAQindex;


template <class __TYPE>
class stdArrayQueue : public stdObject {

	friend class sAQindex<__TYPE>;
public:
	stdArrayQueue(int len) {
		initial(len);
	}
	stdArrayQueue(sPtr<stdArrayQueue<__TYPE> > inp) {
		initial(inp->length());

	int i;
	unsigned int ptr;
		lock();
		tail = inp->tail;
		head = inp->head;
		for ( i = 0 ; i < a->length() ; i ++ ) {
			a->ary[i] = inp->a->ary[i];
			ix->ary[i] = AQ_EMPTY;
		}
		for ( ptr = tail ; ptr != head ; ptr ++ )
			ix->ary[ptr%ix->length()] = 0;
		unlock();
	}
	~stdArrayQueue() {
		a = 0;
		ix = 0;
		pthread_mutex_destroy(&mu);
		pthread_cond_destroy(&cond);
	}

	int count() {
	int ret;
		lock();
		ret = i_count();
		unlock();
		return ret;
	}
	void length(int len) {
		lock();
		i_length(len);
		unlock();
	}
	int length() {
		return a->length();
	}
	int truncate(sAQindex<__TYPE> from,sAQindex<__TYPE> to) {
	sAQindex<__TYPE> ptr;
	AQ_LINE(ptr);
		if ( from > to )
			return -1;
		ptr.set(this);
		for ( ; ; ) {
			ptr.tail();
			if ( ptr == from )
				break;
			ptr.del();
		}
		for ( ; ; ) {
			ptr.head();
			if ( ptr == to )
				break;
			ptr.pop();
		}
		return 0;
	}


protected:
	int i_count() {
		return head - tail;
	}
	void i_length(int len) {
	int i,j,ncount;
	sPtr<stdArray<__TYPE> > n;
	sPtr<stdArray<int> > nix;
		len = regular_length(len);
		n =  thNEW(stdArray<__TYPE>, (len));
		nix = thNEW(stdArray<int>, (len));
#ifdef AQ_DEBUG_MODE
	sPtr<stdArray<sPtr<sAQindex<__TYPE> > > > dl;
		dl = thNEW(stdArray<sPtr<sAQindex<__TYPE> > >,(len));
#endif
		if ( count() < len  )
			ncount = count();
		else {
			ncount = len;
			for ( ; count() > len ; )
				del();
		}
		for ( i = tail ; i != head ; ) {
			n->ary[i%n->length()] = a->ary[i%a->length()];
			nix->ary[i%n->length()] = ix->ary[i%a->length()];
#ifdef AQ_DEBUG_MODE
			dl->ary[i%n->length()] 
				= debug_list->ary[i%a->length()];
#endif
			i ++;
		}
		a = n;
		ix = nix;
#ifdef AQ_DEBUG_MODE
		debug_list = dl;
#endif
	}
	int regular_length(int len) {
	int i;
		for ( i = 0 ; i < AQ_SIZE-1 ; i ++ )
			if ( len <= (1<<i) )
				break;
		return 1<<i;
	}
	void initial(int len) {
	int i;
		len = regular_length(len);
		a = thNEW(stdArray<__TYPE> , (len));
		ix = thNEW(stdArray<int>, (len));
		for ( i = 0 ; i < len ; i ++ )
			ix->ary[i] = AQ_EMPTY;
		head = tail = 0;

		pthread_mutex_init(&mu,0);
		pthread_cond_init(&cond,0);

#ifdef AQ_DEBUG_MODE
		debug_list = thNEW(stdArray<sPtr<sAQindex<__TYPE> > >,(len));
#endif
	}
	sPtr<__TYPE > get(unsigned int pos) {
		return &a->ary[pos%a->length()];
	}

	unsigned int ins() {
	int ret;
		if ( i_count() == a->length() )
			del();
		ret = head ++;
		ix->ary[ret%ix->length()] = AQ_INS;
		return ret;
	}
	int del() {
		if ( i_count() == 0 )
			return AQE_ERROR;
		for ( ; ix->ary[tail%a->length()] ; ) {
			wait_count ++;
			pthread_cond_wait(&cond,&mu);
		}
#ifdef AQ_DEBUG_MODE
		if ( ix->ary[tail%ix->length()] == 0 &&
				debug_list->ary[tail%a->length()] ) {
			stdObject::panic("invalid debug_list");
		}
#endif
		tail ++;
		return 0;
	}
	int pop() {
	int ret;
		if ( i_count() == 0 )
			return AQE_ERROR;
		ret = head - 1;
		for ( ; ix->ary[ret%a->length()] ; ) {
			wait_count ++;
			pthread_cond_wait(&cond,&mu);
		}
		head  = ret;
		return 0;
	}
	int idx(unsigned int p) {
		return ix->ary[p%ix->length()];
	}
	int attach(unsigned int p,sPtr<sAQindex<__TYPE> > org) {
		for  ( ; ; ) {
			if ( head < tail ) {
				if ( head <= p && p < tail )
					return AQE_ERROR;
			}
			else if  ( head <= p || p < tail )
				return AQE_ERROR;
			if ( ix->ary[p%ix->length()] >= 0 )
				break;
			wait_count ++;
			pthread_cond_wait(&cond,&mu);
		}
		ix->ary[p%ix->length()] ++;
#ifdef AQ_DEBUG_MODE
		org->debug_next = debug_list->ary[p%ix->length()];
		debug_list->ary[p%ix->length()] = org;
#endif
		return 0;
	}
	int detach(unsigned int p,sPtr<sAQindex<__TYPE> > org) {
	int ins_flag = 0;
		if ( head < tail ) {
			if ( head <= p && p < tail )
				return AQE_ERROR;
		}
		else if  ( head <= p || p < tail )
			return AQE_ERROR;
		if ( ix->ary[p%ix->length()] == 0 )
			return AQE_ERROR;
		if ( ix->ary[p%ix->length()] == AQ_INS ) {
			ix->ary[p%ix->length()] = 0;
			ins_flag = 1;
		}
		else {
			ix->ary[p%ix->length()] --;
		}
		if ( ix->ary[p%ix->length()] == 0 ) {
			for ( ; wait_count > 0 ; wait_count -- )
				pthread_cond_signal(&cond);
		}
#ifdef AQ_DEBUG_MODE
		sPtr<sAQindex<__TYPE> >* pp;
		for ( pp = &debug_list->ary[p%a->length()] ;
			*pp && *pp != org ;
			pp = &(*pp)->debug_next);
		if ( *pp )
			*pp = org->debug_next;
		else if ( ins_flag == 0 )
			stdObject::panic("no object that detach");
		if ( ix->ary[p%ix->length()] == 0 &&
				debug_list->ary[p%a->length()] ) {
			stdObject::panic("invalid debug_list");
		}
#endif
		return 0;
	}

	void lock() {
		pthread_mutex_lock(&mu);
	}
	void unlock() {
		pthread_mutex_unlock(&mu);
	}

	sPtr<stdArray<__TYPE> >	a;
	sPtr<stdArray<int> >	ix;
#ifdef AQ_DEBUG_MODE
	sPtr<stdArray<sPtr<sAQindex<__TYPE> > > > debug_list;
#endif

	unsigned int			head;
	unsigned int			tail;

	pthread_mutex_t			mu;
	pthread_cond_t			cond;
	int				wait_count;
};


template <class __TYPE>
class sAQindex : public sObject {
public:
	friend class stdArrayQueue<__TYPE>;

	sAQindex() {
		pos = 0;
		ptr = 0;
		a = 0;
		proc_flag = 0;
#ifdef AQ_DEBUG_MODE
		debug_next = 0;
		_line = 0;
		_file = 0;
		_stamp = this;
#endif
	}
	sAQindex(sPtr<stdArrayQueue<__TYPE> > aa) {
		pos = 0;
		ptr = 0;
		a = aa;
		proc_flag = 0;
#ifdef AQ_DEBUG_MODE
		debug_next = 0;
		_line = 0;
		_file = 0;
		_stamp = this;
#endif
	}
	sAQindex(const sAQindex & inp) {
		a = inp.a;
		pos = 0;
		ptr = 0;
		proc_flag = 0;
#ifdef AQ_DEBUG_MODE
		debug_next = 0;
		_line = 0;
		_file = 0;
		_stamp = this;
#endif
		if ( a.is_null() ) {
			pos = inp.pos;
			return;
		}
		a->lock();
		pos = inp.pos;
		attach();
		a->unlock();
	}
	~sAQindex() {
		if ( a.is_null() )
			return;
		a->lock();
		detach();
		a->unlock();
#ifdef AQ_DEBUG_MODE
		if ( _stamp != this )
			stdObject::panic("not equal stamp");
#endif
	}
	int status() {
	  	if ( a.is_null() )
			return sAQ_INIT;
		if ( ptr == 0 )
			return sAQ_DETACH;
		return sAQ_ATTACH;
	}
	void set(sPtr<stdArrayQueue<__TYPE> > aa) {
		if ( a.is_notNull() ) {
			a->lock();
			detach();
			a->unlock();
		}
		a = aa;
		if ( a.is_notNull() ) {
			a->lock();
			attach();
			a->unlock();
		}
	}

	void line(const char* __file,int __line) {
#ifdef AQ_DEBUG_MODE
		_line = __line;
		_file = __file;
#endif
	}

	sPtr<__TYPE > get_ptr() {
		return ptr;
	}
	void clear() {
		if ( a.is_null() )
			return;
		a->lock();
		detach();
		a->unlock();
	}
	int is_tail() {
	  	if ( a.is_notNull() && (pos == a->tail) )
			return 1;
		return 0;
	}
	int is_head() {
		if ( a.is_null() )
			return 0;
		if ( a->tail == a->head )
			return 0;
		if ( pos == a->head-1 )
			return 1;
		return 0;
	}
	int number() {
		return pos - a->tail;
	}
	int tail() {
	int ret;
		a->lock();
		ret = i_tail();
		a->unlock();
		return ret;
	}
	int i_tail() {
		detach();
		pos = a->tail;
		attach();
		return 0;
	}
	int head() {
	int ret;
		a->lock();
		ret = i_head();
		a->unlock();
		return ret;
	}
	int i_head() {
		if ( a->i_count() == 0 )
			return AQE_ERROR;
		detach();
		pos = a->head-1;
		attach();
		return 0;
	}
	int ins() {
	int ret;
		a->lock();
		ret = i_ins();
		a->unlock();
		return ret;
	}
	int i_ins() {
	int ret;
		ret = a->ins();
		if ( ret < 0 )
			return AQE_ERROR;
		detach();
		pos = ret;
		ptr = a->get(pos);
		if ( proc_flag )
			stdObject::panic("proc_flag ins");
		proc_flag = 1;
		return 0;
	}
	int del() {
	int ret;
		a->lock();
		ret = i_del();
		a->unlock();
		return ret;
	}
	int i_del() {
		if ( pos == a->tail )
			detach();
		return a->del();
	}
	int pop() {
	int ret;
		a->lock();
		ret = i_pop();
		a->unlock();
		return ret;
	}
	int i_pop() {
		if ( pos == (int)(a->head - 1) )
			detach();
		return a->pop();
	}
	sAQindex& operator=(sAQindex inp) {
sPtr<stdArrayQueue<__TYPE> >	prev_a;
int prev_pos;
sPtr<__TYPE> prev_ptr;
prev_a = a;
prev_pos = pos;
prev_ptr = ptr;
int prev_tt = 0;

		if ( a.is_null() ) {
if ( ptr )
stdObject::panic("???1");
			if ( inp.a.is_null() ) {
				pos = inp.pos;
			}
			else {
				a = inp.a;
				a->lock();
				pos = inp.pos;
				attach();
				a->unlock();
			}
		}
		else {
			if ( inp.a.is_null() ) {
				a->lock();
				detach();
				a->unlock();
				pos = inp.pos;
			}
			else {
				if ( a == inp.a ) {
					a->lock();
					detach();
					pos = inp.pos;
					attach();
					a->unlock();
				}
				else {
					a->lock();
					inp.a->lock();
					detach();
					a->unlock();

					a = inp.a;
					pos = inp.pos;
					attach();
					a->unlock();
				}
			}
		}
		return *this;

		if ( !a.is_null() ) {
prev_tt = 1;
			a->lock();
			detach();
			a->unlock();
		}
		if ( !inp.a.is_null() )
			a = inp.a;
		a->lock();
		pos = inp.pos;
		attach();
		a->unlock();
		return *this;
	}
	int operator - (sAQindex inp) const {
		return pos - inp.pos;
	}
	sAQindex operator + (int inp) const {
	sAQindex ret(a);
		ret.pos = pos + inp;
		a->lock();
		ret.attach();
		a->unlock();
		return ret;
	}
	sAQindex operator - (int inp) const {
	sAQindex ret(a);
		ret.pos = pos - inp;
		a->lock();
		ret.attach();
		a->unlock();
		return ret;
	}
	sAQindex & operator += (int inp) {
		a->lock();
		detach();
		pos += inp;
		attach();
		a->unlock();
		return *this;
	}
	sAQindex & operator ++ () {
		a->lock();
		detach();
		pos ++;
		attach();
		a->unlock();
		return *this;
	}
	sAQindex operator ++ (int) {
	sAQindex<__TYPE> ret;
		ret = *this;
		a->lock();
		detach();
		pos ++;
		attach();
		a->unlock();
		return ret;
	}
	sAQindex & operator -= (int inp) {
		a->lock();
		detach();
		pos -= inp;
		attach();
		a->unlock();
		return *this;
	}
	sAQindex & operator -- (int inp) {
		a->lock();
		detach();
		pos --;
		attach();
		a->unlock();
		return *this;
	}
	bool operator == (sAQindex inp) const {
		return (pos == inp.pos);
	}
	bool operator != (sAQindex inp) const {
		return (pos != inp.pos);
	}
	bool operator <= (sAQindex inp) const {
		return (pos - a->tail <= inp.pos - a->tail);
	}
	bool operator < (sAQindex inp) const {
		return (pos - a->tail < inp.pos - a->tail);
	}
	bool operator >= (sAQindex inp) const {
		return (pos - a->tail >= inp.pos - a->tail);
	}
	bool operator > (sAQindex inp) const {
		return (pos - a->tail > inp.pos - a->tail);
	}
	__TYPE * operator -> () const {
		return ptr;
	}
	__TYPE & operator * () const {
		return *ptr;
	}
protected:

#ifdef AQ_DEBUG_MODE
	sPtr<sAQindex<__TYPE> > 		debug_next;
	const char * 			_file;
	int				_line;
	sPtr<sAQindex<__TYPE> >		_stamp;
#endif
	unsigned			proc_flag:1;

	sPtr<stdArrayQueue<__TYPE> >	a;
	int				pos;
	sPtr<__TYPE>			ptr;

	int attach() {
	int ret;
		if ( ptr ) {
			stdObject::panic("double attach");
			return 0;
		}
		if ( this != _stamp )
			stdObject::panic("different stamp");
		ret = a->attach(pos,this);
		if ( ret == 0 ) {
			ptr = a->get(pos);
			proc_flag = 1;
		}
		else	ptr = 0;
		return ret;
	}
	int detach() {
		if ( ptr == 0 )
			return 0;
		if ( proc_flag == 0 )
			stdObject::panic("no attach detach");
		proc_flag = 0;
		a->detach(pos,this);
		ptr = 0;
		return 0;
	}
};



#endif
