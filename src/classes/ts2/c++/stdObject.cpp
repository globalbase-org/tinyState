// TS2_DONT_MODIFY

#include	<stdio.h>
#include	<stdlib.h>
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/sThreadMutexHandle.h"
#include	"ts2/c++/sThreadCond.h"

#include	"_ts2/c++/tinyState_.h"
class tinyState;

#define delayedGC_MAX		1000


sThreadMutex
stdObject::refMtx[stdObject_REF_MUTEX_NUM];
int8_t
stdObject::refFlags[stdObject_REF_MUTEX_NUM];
stdObject *
stdObject::refList[stdObject_REF_MUTEX_NUM];
stdObject *
stdObject::refEventHead[stdObject_REF_MUTEX_NUM];
stdObject *
stdObject::refEventTail[stdObject_REF_MUTEX_NUM];
sThreadCond
stdObject::refCond;
int8_t
stdObject::start_flag;

int8_t
stdObject::finish_flag;

stdObject::stdObject()
{
	this->ref = 1;
	refEvent_num = -1;
}


stdObject::~stdObject()
{
	if ( ref != -1 )
		stdObject::panic("ref count is not zero");
}


#define doID(x)		(((U_INTPTR)(x))%stdObject_REF_MUTEX_NUM)
#define doLOCK		sThreadMutexHandle __hdr(refMtx[doID(this)]);
#define doLOCK_ID(x)	sThreadMutexHandle __hdr(refMtx[x]);
#define doLOCK_REL_ID(x)	sThreadMutexHandleRelease __hdr(refMtx[x]);


void
stdObject::nRefEvent(int n)
{
doLOCK;
	refEvent_num = n;
}

int
stdObject::nRefEvent()
{
doLOCK;
	return refEvent_num;
}

void
stdObject::addref()
{
	doLOCK;
	if ( ref == -1 )
		return;
	ref ++;
}

void
stdObject::relref()
{
int id;
	id = doID(this);
	{
	doLOCK_ID(id)
		if ( ref == -1 )
			return;
		if ( ref <= 0 ) {
			::printf("FILE:%s LINE:%i\n",
				this->get_sobj_file(),
				this->get_sobj_line());
			panic("release!!");
		}
		if ( ref <= refEvent_num+1 ) {
			refEvent_num = -1;
			if ( refEventHead[id] == 0 )
				refEventHead[id] = refEventTail[id] = this;
			else {
				refEventTail[id]->next = this;
				refEventTail[id] = this;
			}
		}
		else {
			ref --;
			if ( ref )
				return;
			next = refList[id];
			refList[id] = this;
		}
		if ( id == 0 ) {
			refCond.signal();
			return;
		}
	}
	for ( ; id > 0 ; ) {
	int nxt;
		nxt = (id-1)>>1;
		{
		doLOCK_ID(nxt);
		int mask;
			mask = (1<<((id-1)&1));
			if ( refFlags[nxt] & mask )
				return;
			refFlags[nxt] |= mask;
			id = nxt;
			if ( id == 0 )
				refCond.signal();
		}
	}
}



void
stdObject::start()
{
pthread_attr_t 		phy_attr;
pthread_t		phy_thread;
	if ( start_flag )
		return;
	start_flag = 1;
	pthread_attr_init(&phy_attr);
	pthread_attr_setdetachstate(&phy_attr,PTHREAD_CREATE_DETACHED);
	pthread_create(&phy_thread,&phy_attr,stdObject::gc_thread,(void*)0);
}

void
stdObject::finish()
{
doLOCK_ID(0)
	finish_flag = 1;
	refCond.broadcast();	/* idle で refCond.wait に寝ている gc_thread を起こす。
				 * これが無いと、GC work が尽きて gc_thread が先に寝た後で
				 * finish() が flag を立てても誰も signal せず両者永眠（teardown
				 * デッドロック）。work 残存時は投入側 signal で偶発的に起きるため
				 * idle 終了時のみ出るレースだった。 */
	for ( ; finish_flag == 1 ; )
		refCond.wait(refMtx[0]);
}


void
stdObject::gc(int id)
{
stdObject * target;
stdObject * ev;
int flags;
	{
	doLOCK_ID(id);
		target = refList[id];
		refList[id] = 0;
		flags = refFlags[id];
		refFlags[id] = 0;
		ev = refEventHead[id];
		refEventHead[id] = refEventTail[id] = 0;
	}
	for ( ; target ; ) {
	stdObject * obj;
		obj = target;
		target = target->next;
		if ( obj->getref() != 0 )
			stdObject::panic("???");
		obj->ref = -1;
		delete obj;
	}
	for ( ; ev ; ) {
	stdObject * obj;
		obj = ev;
		ev = ev->next;
		obj->refEvent();
		obj->relref();
	}
	if ( flags & 1 )
		gc(id*2+1);
	if ( flags & 2 )
		gc(id*2+2);
}


void *
stdObject::gc_thread(void * arg)
{
	for ( ; ; ) {
		{
		doLOCK_ID(0)
			for ( ; refList[0] == 0 && refFlags[0] == 0 ; ) {
				if ( finish_flag ) {
				  	finish_flag = 2;
					refCond.signal();
					return 0;
				}
				refCond.wait(refMtx[0]);
			}
		}
		gc(0);
	}
}



int
stdObject::getref()
{
	return this->ref;
}




int
stdObject::seq()
{
static int seq_no;
	refLock();
	seq_no ++;
	if ( seq_no <= 0 )
		seq_no = 1;
	refUnlock();
	return seq_no;
}


int
stdObject::printf(const char * fmt,...)
{
	int 	ret;
	va_list arg;
	va_start(arg,fmt);
	ret = vprintf(fmt,arg);
	va_end(arg);
	return ret;
}

int
stdObject::v_printf(const char * fmt,va_list arg)
{
	return vprintf(fmt,arg);
}

