

#include 	<sys/types.h>
#include 	<sys/time.h>
#include 	<unistd.h>
#include	<errno.h>
#include	<sys/stat.h>
#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/fwIO.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/stdEvent.h"
#include	"ts2/c++/tsApplication.h"
#include	"_ts2/c++/tinyState_.h"

INTEGER64 
fwIO::start_time;

class fwMaxFD : public stdObject {
public:
	int			maxfd;
	fd_set *		fsp;
	fd_set *		fexp;

	fwMaxFD()
	{
	}
};


class fwIOptr : public stdObject {
public:
	sPtr<fwIOdata> 	ptr;
	fwIOptr()	{}
	virtual ~fwIOptr() {
		ptr = thNULL;
	}
};

fwIO::fwIO(sPtr<tsApplication> app)
{
	pipe(pfd);
int flags;
	flags = fcntl(FW_PFD_READ,F_GETFL,0);
	flags |= O_NONBLOCK;
	fcntl(FW_PFD_READ,F_SETFL,flags);

	start_time = stdInterval::now();
	this->application = app;

	this->read_objs = thNEW( stdQueue<fwIOdata>,());
	this->write_objs = thNEW( stdQueue<fwIOdata>,());
	this->interval_objs = thNEW( stdQueue<fwIOdata>,());

	announceMaxfd = -1;

	refio = 0;
}



static int
_send_destroy_event(sPtr<fwIOdata>  d1,sPtr<stdObject>  d2)
{
sPtr<stdEvent>  ev;

	ev = thNEW( stdEvent,(TSE_DESTROY,d1->obj,(INTEGER64)0));
	d1->obj->eventHandler(ev);
	return 0;
}

fwIO::~fwIO()
{
sPtr<stdQueue<fwIOdata> > r, w, iv;
	{
	sThreadMutexHandle __hdr(mu);
		r  = this->read_objs;
		w  = this->write_objs;
		iv = this->interval_objs;
		this->read_objs     = thNULL;
		this->write_objs    = thNULL;
		this->interval_objs = thNULL;
	}
	r->check(this,_send_destroy_event);
	w->check(this,_send_destroy_event);
	iv->check(this,_send_destroy_event);
	r  = thNULL;
	w  = thNULL;
	iv = thNULL;
	this->application = thNULL;
}


void
fwIO::writePipe()
{
	if ( wait_flag == 1 )
		return;
	wait_flag = 1;
int er;
int ch;
	for ( ; ; ) {
		er = ::write(FW_PFD_WRITE,&ch,1);
		if ( er == 1 )
			break;
		if ( er < 0 ) {
			switch ( errno ) {
			case EINTR:
				continue;
			case EAGAIN:
				usleep(1);
				continue;
			default:
				break;
			}
			break;
		}
	}
}

void
fwIO::readPipe()
{
char buf[10];
int er;
	for ( ; ; ) {
		er = ::read(FW_PFD_READ,buf,10);
		if ( er == 0 )
			break;
		if ( er < 0 ) {
			switch ( errno ) {
			case EINTR:
				continue;
			case EAGAIN:
			default:
				break;
			}
			break;
		}
	}
}



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

void
fwIO::dump_maskbits(
	const char *msg,
	const char * post,
	fd_set * fdread,
	fd_set * fdwrite,
	fd_set * fdexp,
	int max)
{
const char * del;
int i;
sThreadMutexHandle __hdr(mu);
	::printf("%s:%s[%i]\n",msg,post,max);
	::printf("\tREAD:");
	del = "";
	for ( i = 0 ; i < max ; i ++ ) {
		if ( FD_ISSET(i,fdread) ) {
			::printf("%s%i",del,i);
			del = ",";
		}
	}
	::printf("\n");
	::printf("\tWRITE:");
	del = "";
	for ( i = 0 ; i < max ; i ++ ) {
		if ( FD_ISSET(i,fdwrite) ) {
			::printf("%s%i",del,i);
			del = ",";
		}
	}
	::printf("\n");
	::printf("\tEXP:");
	del = "";
	for ( i = 0 ; i < max ; i ++ ) {
		if ( FD_ISSET(i,fdexp) ) {
			::printf("%s%i",del,i);
			del = ",";
		}
	}
	::printf("\n");
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




struct timeval
fwIO::ts_get_timeval(INTEGER64 inp)
{
	struct timeval ret;
	ret.tv_usec = inp % 1000000;
	ret.tv_sec = inp / 1000000;
	return ret;
}


int
fwIO::read(sPtr<tinyState>  THIS,int fd,int opt)
{
sPtr<fwIOdata>  dd;
sThreadMutexHandle __hdr(mu);
	if ( fd < 0 )
		stdObject::panic("invalid fd read 1");
	if ( fd >= FD_SETSIZE )
		stdObject::panic("invalid fd read 2");
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
	if ( fd < 0 )
		stdObject::panic("invalid fd write 3");
	if ( fd >= FD_SETSIZE )
		stdObject::panic("invalid fd write 4");
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
/*
	if ( type == TSE_TIMER )
		dd->type = TSE_TIMER;
	else	dd->type = TSE_RETURN;
*/
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
fwIO::addRefio()
{
sThreadMutexHandle __hdr(mu);
	refio ++;
}

void 
fwIO::delRefio()
{
sThreadMutexHandle __hdr(mu);
	refio --;
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

static int
ts_io_get_maxfd(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
sPtr<fwMaxFD>  mfd;
	mfd = sPtr<fwMaxFD>::d_cast(d2);
	switch ( dd->opt ) {
	case FWIO_OPT_ONLY_EXP:
		FD_SET(dd->data,mfd->fexp);
		break;
	default:
		FD_SET(dd->data,mfd->fsp);
		FD_SET(dd->data,mfd->fexp);
	}
	if ( mfd->maxfd <= dd->data )
		mfd->maxfd = dd->data + 1;
	return 0;
}


static int
ts_io_get_interval_minimal(sPtr<fwIOdata>  dd,sPtr<stdObject>  d2)
{
sPtr<fwIOptr>  idptr;
	idptr = sPtr<fwIOptr>::d_cast(d2);
	if ( idptr->ptr == thNULL ) {
		idptr->ptr = dd;
		return 0;
	}
	if ( idptr->ptr->data > dd->data ) {
		idptr->ptr = dd;
		return 0;
	}
	if ( idptr->ptr->data < dd->data ) {
		return 1;
	}
	return 0;
}



static int
ts_io_filter_zom(sPtr<fwIOdata>  dd)
{
	if ( C_TEST(dd->obj->state(),C_ZOM) )
		return 1;
	return 0;
}

static int
ts_io_filter_zom2(sPtr<fwIOdata>  dd)
{
	if ( C_TEST(dd->obj->state(),C_ZOM) )
		return 1;
	return -1;
}

int
fwIO::loop(sPtr<tsApplication> app)
{
int ret;
int rret;
fd_set fdwrite;
fd_set fdread;
fd_set fdexp;
sPtr<fwIOptr>  idppptr;
	idppptr = thNULL;
sPtr<fwMaxFD>  mfd;
	mfd = thNULL;
sPtr<stdQueue<fwIOdata> >  result;
	result = thNULL;
sPtr<fwIOdata>  ok;
struct timeval intr;
INTEGER64 sub;
INTEGER64 nn;

	wait_flag = 1;

	idppptr = thNEW( fwIOptr,());
	mfd = thNEW( fwMaxFD,());
	rret = 0;
	for ( ; ; ) {
	sPtr<stdQueue<fwIOdata> > backup;
		backup = thNEW(stdQueue<fwIOdata>,());
		{
//		sThreadMutexHandle __hdr2(app->mtx);
		sThreadMutexHandle __hdr(mu);
			FD_ZERO(&fdread);
			FD_ZERO(&fdwrite);
			FD_ZERO(&fdexp);

			read_objs->check([backup](sPtr<fwIOdata> elm) {
				if ( C_TEST(elm->obj->state(),C_ZOM) )
					backup->ins(MAX_INTEGER64,elm);
				return 0;
			});
			write_objs->check([backup](sPtr<fwIOdata> elm) {
				if ( C_TEST(elm->obj->state(),C_ZOM) )
					backup->ins(MAX_INTEGER64,elm);
				return 0;
			});
			interval_objs->check([backup](sPtr<fwIOdata> elm) {
				if ( C_TEST(elm->obj->state(),C_ZOM) )
					backup->ins(MAX_INTEGER64,elm);
				return 0;
			});

			this->read_objs->move(ts_io_filter_zom);
			this->write_objs->move(ts_io_filter_zom);
			this->interval_objs->move(ts_io_filter_zom2);
			mfd->maxfd = 0;
			mfd->fsp = &fdread;
			mfd->fexp = &fdexp;
			this->read_objs->check(mfd,ts_io_get_maxfd);
			mfd->fsp = &fdwrite;
			this->write_objs->check(mfd,ts_io_get_maxfd);
			idppptr->ptr = thNULL;
			this->interval_objs->check(idppptr,ts_io_get_interval_minimal);
			if ( stdFrameWork::trace_all )
				dump(stdFrameWork::trace_all);
			if ( idppptr->ptr.is_notNull() )
				realMaxfd = this->read_objs->count + this->write_objs->count + announceMaxfd + 1;
			else	realMaxfd = this->read_objs->count + this->write_objs->count;
			if ( realMaxfd == announceMaxfd && idppptr->ptr == thNULL ) {
				app->frameWorkEvent(mfd->maxfd);
				continue;
			}
			if ( mfd->maxfd == 0 && idppptr->ptr == thNULL && refio == 0 ) {
				ret = rret;
				break;
			}
			FD_SET(FW_PFD_READ,&fdread);
			FD_SET(FW_PFD_READ,&fdexp);
			if ( mfd->maxfd <= FW_PFD_READ )
				mfd->maxfd = FW_PFD_READ+1;
			wait_flag = 0;
		}
		backup = thNULL;
		if ( stdFrameWork::trace_all )
			dump_maskbits(
				stdFrameWork::trace_all,
				"IN",
				&fdread,
				&fdwrite,
				&fdexp,
				mfd->maxfd);


		if ( idppptr->ptr.is_notNull() ) {
			sub = idppptr->ptr->data - stdInterval::now();
			if ( sub < 0 )
				memset(&intr,0,sizeof(intr));
			else	intr = ts_get_timeval(sub);	
			ret = select(mfd->maxfd,
				&fdread,
				&fdwrite,
				&fdexp,
				&intr);
		}
		else {
			ret = select(mfd->maxfd,
				&fdread,
				&fdwrite,
				&fdexp,
				(struct timeval *)NULPTR);
		}

		if ( stdFrameWork::trace_all )
			dump_maskbits(
				stdFrameWork::trace_all,
				"OUT",
				&fdread,
				&fdwrite,
				&fdexp,
				mfd->maxfd);
		if ( ret < 0 ) {
			if ( errno == EINTR )
				continue;
			if ( errno == EAGAIN ) {
				sleep(1);
				continue;
			}
			if ( errno == EBADF ) {
			int fd;
			struct stat buf;
			sPtr<stdQueueElement<fwIOdata> > elp;
				for ( fd = 0 ; fd < mfd->maxfd ; fd ++ ) {
					if ( FD_ISSET(fd,&fdread) ) {
						if ( fstat(fd,&buf) == 0 )
							continue;
						{
						sThreadMutexHandle __hdr(mu);
							elp = read_objs->head;
							for ( ; elp.is_notNull() ; elp = elp->next )
								if ( elp->data->data == fd ) {
									elp->data->obj->printParent();
									perror("fwIO ERROR read");
									stdObject::panic("fwIO ERROR read");
								}
						}
					}
					if ( FD_ISSET(fd,&fdwrite) ) {
						if ( fstat(fd,&buf) == 0 )
							continue;
						{
						sThreadMutexHandle __hdr(mu);
							elp = write_objs->head;
							for ( ; elp.is_notNull() ; elp = elp->next )
								if ( elp->data->data == fd ) {
									elp->data->obj->printParent();
									perror("fwIO ERROR write");
									stdObject::panic("fwIO ERROR write");
								}
						}
					}
					if ( FD_ISSET(fd,&fdexp) ) {
						if ( fstat(fd,&buf) == 0 )
							continue;
						{
						sThreadMutexHandle __hdr(mu);
							elp = read_objs->head;
							for ( ; elp.is_notNull() ; elp = elp->next )
								if ( elp->data->data == fd ) {
									elp->data->obj->printParent();
									perror("fwIO ERROR exp");
									stdObject::panic("fwIO ERROR");
								}
							elp = write_objs->head;
							for ( ; elp.is_notNull() ; elp = elp->next )
								if ( elp->data->data == fd ) {
									elp->data->obj->printParent();
									perror("fwIO ERROR");
									stdObject::panic("fwIO ERROR");
								}
						}
					}
				}
//				::printf("fwIO NOTHING FD\n");
				continue;
				stdObject::panic("fwIO NOTHING FD");
			}
			perror("fwIO ERROR");
			break;
		}

		{
		sThreadMutexHandle __hdr(mu);
			rret = 1;
			result = thNEW( stdQueue<fwIOdata>,());

			wait_flag = 1;
			readPipe();

			if ( ret > 0 ) {
			sPtr<stdQueue<fwIOdata> > q;
			fd_set target;
				FD_ZERO(&target);
				q = thNEW(stdQueue<fwIOdata>, ());
				this->read_objs->check([&fdread,&fdexp,&target,&result,&q]
						(sPtr<fwIOdata> dd){
					if ( !FD_ISSET(dd->data,&fdread) &&
							!FD_ISSET(dd->data,&fdexp) ) {
						q->ins(MAX_INTEGER64,dd);
						return 0;
					}
					if ( FD_ISSET(dd->data,&target) )
						return 0;
					result->ins(MAX_INTEGER64,dd);
					FD_SET(dd->data,&target);
					return 0;
				});
				read_objs = q;

				FD_ZERO(&target);
				q = thNEW(stdQueue<fwIOdata>, ());
				this->write_objs->del([&fdwrite,&fdexp,&target,&result,&q]
						(sPtr<fwIOdata> dd) {
					if ( !FD_ISSET(dd->data,&fdwrite) &&
							!FD_ISSET(dd->data,&fdexp) ) {
						q->ins(MAX_INTEGER64,dd);
						return 0;
					}
					if ( FD_ISSET(dd->data,&target) )
						return 0;
					result->ins(MAX_INTEGER64,dd);
					FD_SET(dd->data,&target);
					return 0;
				});
				write_objs = q;
			}
			{
			sPtr<fwIOdata>  now;
			INTEGER64 nn = stdInterval::now();
				interval_objs->check([&nn,&result]
						(sPtr<fwIOdata> dd){
					if ( dd->data > nn )
						return 1;
					result->ins(MAX_INTEGER64,dd);
					return 0;
				});
				for ( ; interval_objs->head.is_notNull() &&
					interval_objs->head->data->data <= nn ; 
					interval_objs->del() );
			}
		}
		for ( ; ; ) {
		sPtr<stdEvent>  ev;
			ok = result->del();
			if ( ok == thNULL )
				break;
			if ( stdFrameWork::trace_all && (stdFrameWork::trace_bit & FWTR_ACTIVE) ) {
				::printf("fwIO :: %s :: type=%i %p:%s:%s data=%lli\n\t",
					stdFrameWork::trace_all,
					ok->type,
					ok->obj.__get(),
					ok->obj->getClass(),
					ok->obj->getStateName(),
					ok->data);
				ok->obj->printParent();
			}
			ev = thNEW( stdEvent,(ok->type,ok->obj,ok->seq,ok->data));
			ok->obj->eventHandler(ev);
		}
	}
	idppptr = thNULL;
	mfd = thNULL;
	return ret;
}
