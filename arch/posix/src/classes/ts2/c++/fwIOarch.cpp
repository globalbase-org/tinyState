/*
 * fwIO — POSIX backend: select() + a self-pipe wake.
 *
 * Architecture-specific half of fwIO (constructor, destructor, the self-pipe
 * wake, the fd_set helpers and the select() reactor loop()).  The generic
 * registration / query / dispatch API (read/write/wait, is_*, detach,
 * addRefio/delRefio, abort, dump, printf) is shared and lives in
 * src/classes/ts2/c++/fwIO.cpp.  Used by every select()-capable target:
 * Linux, Darwin and Cygwin (posix_MinGW carries the IOCP variant instead).
 */

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
	refio_pins = thNEW(stdQueue<tinyState>,());
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


struct timeval
fwIO::ts_get_timeval(INTEGER64 inp)
{
	struct timeval ret;
	ret.tv_usec = inp % 1000000;
	ret.tv_sec = inp / 1000000;
	return ret;
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
			/* Cygwin: select() can return -1 with errno==0 (a spurious/interrupted
			   wait — e.g. a signal delivered through the no-op SIGPIPE handler).
			   Treat it like EINTR and retry; do NOT fall through to perror+break,
			   which would return <0 and make tsApplication mistake a transient error
			   for "reactor done" and finalize while agents are still live. */
			if ( errno == 0 )
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
