/*
 * fwIO — architecture-independent implementation.
 *
 * The generic registration / query / dispatch API shared by every backend:
 * read/write/wait, is_read/is_write/is_wait, detach, addRefio/delRefio, abort,
 * dump and printf.  The backend-specific pieces — the constructor, destructor,
 * the wake mechanism (writePipe/readPipe/wake/port) and the reactor loop() —
 * live in the per-arch overlay:
 *     arch/posix/.../fwIOarch.cpp        = select() + self-pipe wake
 *     arch/posix_MinGW/.../fwIOarch.cpp  = IOCP
 * Both overlays and this file compile against the one hand-written header
 * src/h/ts2/c++/fwIO.h, whose private members are already #ifdef _WIN32-split.
 * writePipe()/readPipe() are declared in *both* header branches (on MinGW
 * writePipe() just calls wake()), so this shared code calls writePipe() on
 * every platform without an #ifdef.
 */

#ifndef _WIN32
#include	<sys/select.h>	/* FD_SETSIZE bound check below (POSIX only) */
#endif
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/fwIO.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/stdEvent.h"
#include	"ts2/c++/tsApplication.h"
#include	"_ts2/c++/tinyState_.h"

INTEGER64
fwIO::start_time;


static int
_ts_io_dump(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
	printf("   %lli",dd->data);
	return dd->obj->printParent();
}

void
fwIO::dump(const char * msg)
{
//sThreadMutexHandle __hdr(mu);
	if ( stdFrameWork::trace_bit & FWTR_RWI )
		::printf("%s START\n",msg);
	if ( stdFrameWork::trace_bit & FWTR_READ ) {
		::printf("   ** READ **\n");
		read_objs->check(thNULL,_ts_io_dump);
	}
	if ( stdFrameWork::trace_bit & FWTR_WRITE ) {
		::printf("   ** WRITE **\n");
		write_objs->check(thNULL,_ts_io_dump);
	}
	if ( stdFrameWork::trace_bit & FWTR_INTERVAL ) {
		::printf("   ** INTERVAL **\n");
		interval_objs->check(thNULL,_ts_io_dump);
	}
	if ( stdFrameWork::trace_bit & FWTR_RWI )
		::printf("%s END\n",msg);
}

int
fwIO::printf(sPtr<tinyState>  caller,const char * fmt,...)
{
	int 	ret;
	va_list arg;
	va_start(arg,fmt);
	ret = v_printf(caller,fmt,arg);
	va_end(arg);
	return ret;
}

int
fwIO::v_printf(sPtr<tinyState>  caller,const char * fmt,va_list arg)
{
	return v_printf(caller,fmt,arg);
}


int
fwIO::printf_err(sPtr<tinyState>  caller,const char * fmt,...)
{
	int 	ret;
	va_list arg;
	va_start(arg,fmt);
	ret = v_printf(caller,fmt,arg);
	va_end(arg);
	return ret;
}

int
fwIO::v_printf_err(sPtr<tinyState>  caller,const char * fmt,va_list arg)
{
	return v_printf(caller,fmt,arg);
}


int
fwIO::read(sPtr<tinyState>  THIS,int fd,int opt)
{
sPtr<fwIOdata>  dd;
sThreadMutexHandle __hdr(mu);
#ifndef _WIN32
	/* on POSIX fd indexes an fd_set; MinGW's "fd" is an opaque IOCP key */
	if ( fd < 0 )
		stdObject::panic("invalid fd read 1");
	if ( fd >= FD_SETSIZE )
		stdObject::panic("invalid fd read 2");
#endif
	dd = thNEW( fwIOdata,());
	dd->type = TSE_RETURN;
	dd->seq = tinyState::getSeq();
	dd->obj = THIS;
	dd->data = fd;
	dd->opt = opt;
	this->read_objs->ins(fd,dd);
	writePipe();
	return dd->seq;
}

int
fwIO::write(sPtr<tinyState>  THIS,int fd,int opt)
{
sPtr<fwIOdata>  dd;
sThreadMutexHandle __hdr(mu);
#ifndef _WIN32
	if ( fd < 0 )
		stdObject::panic("invalid fd write 3");
	if ( fd >= FD_SETSIZE )
		stdObject::panic("invalid fd write 4");
#endif
	dd = thNEW( fwIOdata,());
	dd->type = TSE_RETURN;
	dd->seq = tinyState::getSeq();
	dd->obj = THIS;
	dd->data = fd;
	dd->opt = opt;
	this->write_objs->ins(fd,dd);
	writePipe();
	return dd->seq;
}

int
fwIO::wait(sPtr<tinyState>  THIS,INTEGER64 tm,int type)
{
sPtr<fwIOdata>  dd;
sThreadMutexHandle __hdr(mu);

	dd = thNEW( fwIOdata,());
	dd->type = type;
	dd->seq = tinyState::getSeq();
	dd->data = stdInterval::now() + tm;
	dd->obj = THIS;
	this->interval_objs->ins(dd->data,dd);
	writePipe();
	return dd->seq;
}

int
fwIO::is_read(sPtr<tinyState> THIS)
{
sThreadMutexHandle __hdr(mu);
	return this->read_objs->check([THIS](sPtr<fwIOdata> dd){
			if ( dd->obj == THIS )
				return 1;
			return 0;
		}).is_notNull();
}

int
fwIO::is_write(sPtr<tinyState> THIS)
{
sThreadMutexHandle __hdr(mu);
	return this->write_objs->check([THIS](sPtr<fwIOdata> dd){
			if ( dd->obj == THIS )
				return 1;
			return 0;
		}).is_notNull();
}

int
fwIO::is_wait(sPtr<tinyState> THIS)
{
sThreadMutexHandle __hdr(mu);
	return this->interval_objs->check([THIS](sPtr<fwIOdata> dd){
			if ( dd->obj == THIS )
				return 1;
			return 0;
		}).is_notNull();
}


static int
_ts_io_detach(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
sPtr<tinyState>  THIS = sPtr<tinyState>::d_cast(d2);
	if ( dd->obj == THIS )
		return 1;
	return 0;
}
int
fwIO::detach(sPtr<tinyState>  THIS,int flags)
{
sThreadMutexHandle __hdr(mu);
	if ( flags & FWTR_READ )
		for ( ; this->read_objs->del(THIS,_ts_io_detach).is_notNull() ; );
	if ( flags & FWTR_WRITE )
		for ( ; this->write_objs->del(THIS,_ts_io_detach).is_notNull() ; );
	if ( flags & FWTR_INTERVAL )
		for ( ; this->interval_objs->del(THIS,_ts_io_detach).is_notNull() ; );
	writePipe();
	return 0;
}

void
fwIO::addRefio(sPtr<tinyState> obj)
{
sThreadMutexHandle __hdr(mu);
	refio ++;
	if ( obj.is_notNull() )				/* pin the issuer alive until its delRefio */
		refio_pins->ins(MAX_INTEGER64,obj);
}

void
fwIO::delRefio(sPtr<tinyState> obj)
{
sThreadMutexHandle __hdr(mu);
	refio --;
	if ( obj.is_notNull() )				/* drop one pin for this issuer */
		refio_pins->del([obj](sPtr<tinyState> x){ return x == obj ? 1 : 0; });
	writePipe();
}

static int
_ts_io_abort(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
sPtr<fwIOdata>  ref;
	ref  = sPtr<fwIOdata>::d_cast(d2);
	if ( ref->data != dd->data )
		return 0;
	return 1;
}

void
fwIO::abort(int fd)
{
sPtr<fwIOdata>  d, ret;
	d = thNEW( fwIOdata,(fd));
	for ( ; ; ) {
		{
		sThreadMutexHandle __hdr(mu);
			if ( !this->read_objs.is_notNull() )
				break;
			ret = this->read_objs->del(d,_ts_io_abort);
			if ( ret == thNULL )
				break;
		}
		ret->obj->eventHandler(
			thNEW( stdEvent,(ret->type,ret->obj,ret->seq,-1)));
	}
	for ( ; ; ) {
		{
		sThreadMutexHandle __hdr(mu);
			if ( !this->write_objs.is_notNull() )
				break;
			ret = this->write_objs->del(d,_ts_io_abort);
			if ( ret == thNULL )
				break;
		}
		ret->obj->eventHandler(
			thNEW( stdEvent,(ret->type,ret->obj,ret->seq,-1)));
	}
}
