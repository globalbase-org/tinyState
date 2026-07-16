
#include	<cstdlib>
#include	"ts2/c++/sObject.h"
#include	"ts2/c++/stdObject.h"

typedef struct code_pos {
	struct code_pos *	cph_next;
	const char *		filename;
	int			line;
	int			subline;
	int			size;
} CODE_POS;

typedef struct ref_point {
	struct ref_point *	next;
	void *			ptr;
} REF_POINT;

typedef struct mem_header {
	size_t			size;
  	CODE_POS *		cp;
	struct mem_header *	prev;
	struct mem_header *	next;
} MEM_HEADER;


#define CP_HASH_SIZE	53
#define CP_TOP_LEN	10

static CODE_POS * cp_hash_mem[CP_HASH_SIZE];
static CODE_POS * cp_top[CP_TOP_LEN];
INTEGER64	total_size,total_mem_cnt;
int ref_subline;

INTEGER64
sObject::total_mem_max = 0;
pthread_mutex_t
sObject::refMutex;
uint8_t
sObject::refMutex_init;
uint8_t
sObject::refMutex_debug;
pthread_t
sObject::refMutex_thread;
int
sObject::refMutex_count;



static int
get_cp_hash(const char * file,int line)
{
	return line %CP_HASH_SIZE;
}

static CODE_POS *
get_cp(const char * file,int line,int subline,CODE_POS * __cp_hash[CP_HASH_SIZE])
{
CODE_POS * ret;
int key;
	key = get_cp_hash(file,line);
	for ( ret = __cp_hash[key];
			ret;
			ret = ret->cph_next )
		if ( ret->line == line &&
				ret->subline == subline &&
				strcmp(ret->filename,file) == 0 )
			return ret;
	ret = (CODE_POS*)malloc(sizeof(*ret));
	memset(ret,0,sizeof(*ret));
	ret->filename = file;
	ret->line = line;
	ret->subline = subline;
	ret->cph_next = __cp_hash[key];
	__cp_hash[key] = ret;
	return ret;
}

static void
cp_set_size(CODE_POS * cp,int size)
{
int i;
	cp->size += size;
	total_size += size;
	if ( sObject::total_mem_max && (total_size > sObject::total_mem_max) )
		stdObject::panic("allocation over");
	for ( i = 0 ; i < CP_TOP_LEN ; i ++ )
		if ( cp_top[i] == cp ) {
			memmove(&cp_top[i],&cp_top[i+1],
				(CP_TOP_LEN-i-1)*sizeof(void*));
			cp_top[CP_TOP_LEN-1] = 0;
			break;
		}
	for ( i = 0 ; i < CP_TOP_LEN ; i ++ ) {
		if ( cp_top[i] == 0 ) {
			cp_top[i] = cp;
			return;
		}
		if ( cp_top[i]->size < cp->size )
			break;
	}
	if ( i == CP_TOP_LEN )
		return;
	memmove(&cp_top[i+1],&cp_top[i],(CP_TOP_LEN-i-1)*sizeof(void*));
	cp_top[i] = cp;
}




void
sObject::panic(const char * msg)
{
	printf("panic: %s\n",msg);
	fflush(stdout);
	/* 即クラッシュして core / クラッシュダンプを残す。旧実装は 0x1 を %s で読んで
	 * SEGV させるハックだったが、abort() なら POSIX/Windows 両対応 (SIGABRT →
	 * Linux は core、Windows は WER)。abort() は [[noreturn]]。 */
	abort();
}





void * 
sObject::operator new(size_t cbSize,const char * __file,int __line)
#if __cplusplus >= 201103L
        noexcept(false)
#else
        throw(std::bad_alloc)
#endif
{
	sObject::refLock();
	MEM_HEADER * mem = (MEM_HEADER*)malloc(cbSize+sizeof(MEM_HEADER));
	mem->size = cbSize;
	mem->cp = get_cp(__file,__line,ref_subline,cp_hash_mem);
	cp_set_size(mem->cp,mem->size);
	total_mem_cnt ++;
	mem ++;
	memset(mem,0,cbSize);
	sObject::refUnlock();
	return (void*)mem;
}

void
sObject::operator delete(void * pv)
#if __cplusplus >= 201103L
        noexcept(true)
#else
	throw()
#endif
{

MEM_HEADER * mem;
size_t cbSize;

	refLock();
	mem = (MEM_HEADER*)pv;
	mem --;
	cp_set_size(mem->cp,-mem->size);
	total_mem_cnt --;
	//delete_active_list(mem);
	cbSize = mem->size;
	memset(pv,0xaa,cbSize);
	free(mem);
	refUnlock();
}

void
sObject::refine_sobj(void * mp,const char * file,int line)
{
MEM_HEADER * mem;

	refLock();
	mem = (MEM_HEADER*)mp;
	mem--;
	cp_set_size(mem->cp,-mem->size);
	mem->cp = get_cp(file,line,0,cp_hash_mem);
	cp_set_size(mem->cp,mem->size);
	refUnlock();
}

void
sObject::refine_sobj(const char * file,int line)
{
	refine_sobj((void*)this,file,line);
}


const char *
sObject::get_sobj_file()
{
MEM_HEADER * mem;
const char * ret;

	refLock();
  	mem = (MEM_HEADER*)this;
	mem--;
	ret = mem->cp->filename;
	refUnlock();
	return ret;
}

int
sObject::get_sobj_line()
{
MEM_HEADER * mem;
int ret;
	refLock();
  	mem = (MEM_HEADER*)this;
	mem--;
	ret = mem->cp->line;
	refUnlock();
	return ret;
}

void * 
operator new[](size_t cbSize,const char * __file,int __line)
#if __cplusplus >= 201103L
        noexcept(false)
#else
	throw(std::bad_alloc)
#endif
{
void * ret;
	ret = (void*)malloc(cbSize);
	memset(ret,0,cbSize);
	return ret;


	sObject::refLock();
  //  ::printf("new[] %lu %s %i\n",cbSize,__file,__line);
	MEM_HEADER * mem = (MEM_HEADER*)malloc(cbSize+sizeof(MEM_HEADER));
	mem->size = cbSize;
	mem->cp = get_cp(__file,__line,ref_subline,cp_hash_mem);
	cp_set_size(mem->cp,mem->size);
	total_mem_cnt ++;
	//insert_active_list(mem);
	mem ++;
	memset(mem,0,cbSize);
	sObject::refUnlock();
	return mem;
}

void
operator delete[](void * pv)
#if __cplusplus >= 201103L
        noexcept(true)
#else
	throw()
#endif
{
  	free(pv);
	return;

	sObject::refLock();
//printf("DELETE [] %p\n",pv);
	MEM_HEADER * mem = (MEM_HEADER*)pv;
	mem --;
	cp_set_size(mem->cp,-mem->size);
	total_mem_cnt --;
	//delete_active_list(mem);
	memset(pv,0xaa,mem->size);
	free(mem);
	sObject::refUnlock();
}

static CODE_POS * cp_hash_fd[CP_HASH_SIZE];
static CODE_POS *
descriptor_list[FD_SETSIZE];
static CODE_POS * descriptor_top;

static int lock_open_init;
static pthread_mutex_t open_mu;
static void lock_open()
{
	if ( lock_open_init )
		pthread_mutex_init(&open_mu,0);
	pthread_mutex_lock(&open_mu);
}

static void unlock_open()
{
	pthread_mutex_unlock(&open_mu);
}

static void
set_open_hash(int s,const char * filename,int line)
{
CODE_POS * cp;
	/* The fd-indexed descriptor_list only makes sense for dense CRT fds
	   (< FD_SETSIZE).  On Windows a winsock SOCKET / HANDLE is not a CRT fd and
	   its value routinely exceeds FD_SETSIZE (=64), so indexing here would be an
	   out-of-bounds write.  Skip out-of-range descriptors (sockets/handles); on
	   POSIX every fd is < FD_SETSIZE so this never triggers.  Windows-port design memo */
	if ( s < 0 || s >= FD_SETSIZE )
		return;
	lock_open();
	if ( descriptor_list[s] )
		stdObject::panic("open not free description!!");
	cp = descriptor_list[s] = get_cp(filename,line,0,cp_hash_fd);
	cp->size ++;
	if ( descriptor_top == 0 || descriptor_top->size < cp->size )
		descriptor_top = cp;
	unlock_open();
}

static void
flush_open_hash(int s)
{
	if ( s < 0 || s >= FD_SETSIZE )		/* see set_open_hash: untracked (socket/handle). Windows-port design memo */
		return;
	lock_open();
	if ( descriptor_list[s] == 0 )
		stdObject::panic("not opened fd is closed");
	descriptor_list[s]->size --;
	descriptor_list[s] = 0;
	unlock_open();
}
void
sObject::report_descriptor()
{
int i;
	for ( i = 0 ; i < FD_SETSIZE; i ++ ) {
		if ( descriptor_list[i] == 0 )
			continue;
		descriptor_list[i]->subline = 0;
	}
	::printf("DESCRIPTOR\n");
	for ( i = 0 ; i < FD_SETSIZE; i ++ ) {
		if ( descriptor_list[i] == 0 )
			continue;
		if ( descriptor_list[i]->subline == 1 )
			continue;
		descriptor_list[i]->subline = 1;
		::printf("\t%p::%s:%i -> %i\n",
			descriptor_list[i],
			descriptor_list[i]->filename,
			descriptor_list[i]->line,
			descriptor_list[i]->size);
	}
	for ( i = 0 ; i < FD_SETSIZE; i ++ ) {
		if ( descriptor_list[i] == 0 )
			continue;
		descriptor_list[i]->subline = 0;
	}
}

int
sObject::__open(const char * filename,int line,const char * pathname,int flags,mode_t mode)
{
int ret;
	ret = open(pathname,flags,mode);
	if ( ret < 0 )
		return ret;
	set_open_hash(ret,filename,line);
	return ret;
}
int
sObject::__close(int s)
{
int er;
const char * file;
int line;
	if ( s < 0 )
		return -1;
#ifdef _WIN32
	/* A descriptor at/above FD_SETSIZE is not a CRT fd but a winsock SOCKET
	   handle (see set_open_hash): close it with closesocket(), not CRT close(),
	   and don't touch the (untracked) descriptor_list.  Windows-port design memo */
	if ( s >= FD_SETSIZE )
		return closesocket((SOCKET)s);
#endif
	if ( descriptor_list[s] == 0 )
		stdObject::panic("close not opened fd");
	file = descriptor_list[s]->filename;
	line = descriptor_list[s]->line;
	flush_open_hash(s);
	for ( ; ; ) {
		er = close(s);
		if ( er == 0 )
			break;
		if ( errno == EINTR )
			continue;
		set_open_hash(s,file,line);
		return er;
	}
	return 0;
}
int
sObject::__fclose(FILE * fd)
{
int er;
const char * file;
int line;
int s;
 	s = fileno(fd);
	if ( descriptor_list[s] == 0 )
		stdObject::panic("close not opened fd");
	file = descriptor_list[s]->filename;
	line = descriptor_list[s]->line;
	flush_open_hash(s);
	for ( ; ; ) {
		er = fclose(fd);
		if ( er == 0 )
			break;
		if ( errno == EINTR )
			continue;
		set_open_hash(s,file,line);
		return er;
	}
	return 0;
}
int
sObject::__pipe(const char *filename,int line,int pipefd[2])
{
int ret;
#ifdef _WIN32
	/* MinGW: no POSIX pipe().  CRT _pipe gives int-fd anonymous pipes.  (The
	   overlapped/HANDLE pipes ts2System needs are created with CreatePipe in the
	   posix_MinGW ts2System overlay; this stays for any int-fd pipe use.) Windows-port design memo */
	ret = _pipe(pipefd, 65536, _O_BINARY);
#else
	ret = pipe(pipefd);
#endif
	if ( ret < 0 )
		return ret;
	set_open_hash(pipefd[0],filename,line);
	set_open_hash(pipefd[1],filename,line);
	return ret;
}
int
sObject::__socket(const char *filename,int line,int sfamily,int type ,int proto)
{
int ret;
	ret = socket(sfamily,type,proto);
	if ( ret < 0 )
		return ret;
	set_open_hash(ret,filename,line);
	return ret;
}
int
sObject::__accept(const char *filename,int line,int sock,struct sockaddr * saddr,socklen_t * lenp)
{
int ret;
	ret = accept(sock,saddr,lenp);
	if ( ret < 0 )
		return ret;
	set_open_hash(ret,filename,line);
	return ret;
}
int
sObject::__openpty(const char * filename,int line,int * amaster,int * aslave,char * name,
	struct termios * termp,
	struct winsize *winp)
{
int ret;
#ifdef _WIN32
	/* MinGW has no openpty (no pty layer).  DM_TTY is stubbed in the posix_MinGW
	   ts2System overlay; a pty here is simply unavailable.  Windows-port design memo §8. */
	(void)amaster; (void)aslave; (void)name; (void)termp; (void)winp;
	(void)filename; (void)line;
	errno = ENOSYS;
	return -1;
#else
	ret = openpty(amaster,aslave,name,termp,winp);
	if ( ret < 0 )
		return ret;
	set_open_hash(*amaster,filename,line);
	set_open_hash(*aslave,filename,line);
	return ret;
#endif
}


void
sObject::fd_stdio()
{
	set_open_hash(0,__FILE__,__LINE__);
	set_open_hash(1,__FILE__,__LINE__);
	set_open_hash(2,__FILE__,__LINE__);
}

void
sObject::refine_fd(int s,const char * filename,int line)
{
CODE_POS * cp;
	if ( s < 0 || s >= FD_SETSIZE )		/* untracked (socket/handle) on Windows. Windows-port design memo */
		return;
	lock_open();
	cp = descriptor_list[s];
	if ( cp == 0 )
		stdObject::panic("no defined descriptor !!");
	cp->size --;
	descriptor_list[s] = 0;
	if ( descriptor_list[s] )
		stdObject::panic("open not free description!!");
	cp = descriptor_list[s] = get_cp(filename,line,0,cp_hash_fd);
	cp->size ++;
	if ( descriptor_top == 0 || descriptor_top->size < cp->size )
		descriptor_top = cp;
	unlock_open();
}


void 
sObject::refLock() {
int er;
pthread_t tid;

	if ( refMutex_init == 0 ) {
	pthread_mutexattr_t attr;
		pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
		if ( (er = pthread_mutex_init(&refMutex,&attr)) ) {
			refMutex_init = 2;
			if ( (er = pthread_mutex_init(&refMutex,0)) ) {
				fprintf(stderr,"%s\n",strerror(er));
				stdObject::panic("refLock-0");
			}
		}
		else
			refMutex_init = 1;
	}
	if ( refMutex_init == 1 ) {
		if ( (er=pthread_mutex_lock(&refMutex)) ) {
			fprintf(stderr,"%s\n",strerror(er));
			stdObject::panic("refLock-0");
		}
	}
	else {
		tid = pthread_self();
if ( refMutex_debug )
::printf("refLock-IN %p %p %i\n",tid,refMutex_thread,refMutex_count);
		if ( refMutex_thread == tid ) {
			refMutex_count ++;
if ( refMutex_debug )
::printf("refLock-0 %p %i\n",refMutex_thread,refMutex_count);
			return;
		}
		if ( (er=pthread_mutex_lock(&refMutex)) ) {
			fprintf(stderr,"%s\n",strerror(er));
			stdObject::panic("refLock-0");
		}
		if ( refMutex_count ) {
			if ( refMutex_thread != tid )
				stdObject::panic("refLock-1");
		}
		else {
			if ( refMutex_thread != 0 )
				stdObject::panic("refLock-2");
		}
		refMutex_thread = tid;
		refMutex_count ++;
if ( refMutex_debug )
::printf("refLock-1 %p %i\n",refMutex_thread,refMutex_count);
	}
}


void 
sObject::refUnlock() {
int er;
	if ( refMutex_init == 1 ) {
		if ( (er=pthread_mutex_unlock(&refMutex)) ) {
			fprintf(stderr,"%s\n",strerror(er));
			stdObject::panic("refUnlock-3");
		}
	}
	else {
if ( refMutex_debug )
::printf("refUnlock %p %i\n",refMutex_thread,refMutex_count);
		if ( refMutex_count == 0 )
			stdObject::panic("refUnlock-1!!");
	pthread_t tid;
		tid = pthread_self();
		if ( refMutex_thread != tid )
			stdObject::panic("refUnlock-2!!");
		refMutex_count --;
		if ( refMutex_count == 0 ) {
	  		refMutex_thread = 0;
if ( refMutex_debug )
::printf("refUnlock-1 %p %i\n",refMutex_thread,refMutex_count);
			if ( (er=pthread_mutex_unlock(&refMutex)) ) {
				fprintf(stderr,"%s\n",strerror(er));
				stdObject::panic("refUnlock-3");
			}
		}
	}
}
