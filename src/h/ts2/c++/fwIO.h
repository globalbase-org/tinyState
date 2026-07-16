
#ifndef ___fwIO_cpp_H___
#define ___fwIO_cpp_H___

#include	"ts2/c++/stdObject.h"
#include	"ts2/c++/tinyState.h"
#include	"ts2/c++/stdQueue.h"
#include	"ts2/c++/stdFrameWork.h"
#include	"ts2/c++/tsApplication.h"

class fwIOdata : public stdObject {
public:
	int			type;
	int			seq;
  	INTEGER64		data;
	int			opt;
	sPtr<tinyState>  		obj;

	fwIOdata()
	{
	}
	fwIOdata(INTEGER64 dd)
	{
		data = dd;
	}
	virtual ~fwIOdata() {
		obj = thNULL;
	}
};



class fwIO : public stdFrameWork {
private:
	sPtr<stdQueue<fwIOdata> > 	write_objs;
	sPtr<stdQueue<fwIOdata> > 	read_objs;
	sPtr<stdQueue<fwIOdata> > 	interval_objs;

#ifndef _WIN32
	static struct timeval ts_get_timeval(INTEGER64 inp);
#endif

	sPtr<tsApplication>		application;


	int 			announceMaxfd;
	int 			realMaxfd;

	sThreadMutex		mu;

	unsigned		wait_flag:1;

#ifndef _WIN32
	int	pfd[2];
#define FW_PFD_READ	pfd[0]
#define FW_PFD_WRITE	pfd[1]
	void readPipe();
	void writePipe();
#else
	/* IOCP backend (MinGW). The self-pipe wake becomes PostQueuedCompletionStatus
	   on the completion port; select becomes GetQueuedCompletionStatus. iocp is a
	   HANDLE kept as void* to avoid <windows.h> ordering constraints in every
	   includer. IO objects associate their HANDLE/SOCKET with this port (keyed by
	   the tinyState object) and issue overlapped ops; loop() dispatches completions
	   by key. Windows-port design memo (§A/B). */
	void *		iocp;
	void		wake();       // PostQueuedCompletionStatus(iocp, WAKE_KEY)
	void		readPipe();   // drain wake completions (name kept for shared call sites)
	void		writePipe();  // == wake(), name kept so shared code paths compile
#endif


	int			refio;
public:
	fwIO(sPtr<tsApplication> app);
	~fwIO();

	int read(sPtr<tinyState>  obj,int fd,int opt=0);
	int write(sPtr<tinyState>  obj,int fd,int opt=0);
#define FWIO_OPT_NORMAL		0
#define FWIO_OPT_ONLY_EXP	1
	int wait(sPtr<tinyState> ,INTEGER64,int);

	int is_read(sPtr<tinyState> obj);
	int is_write(sPtr<tinyState> obj);
	int is_wait(sPtr<tinyState> obj);

	int loop(sPtr<tsApplication> app);
	int detach(sPtr<tinyState> ,int flags=FWTR_ALL);
	void abort(int);
#ifdef _WIN32
	void * port();   // IOCP HANDLE, for IO objects to associate their HANDLE/SOCKET
#endif
	void addRefio();
	void delRefio();

	void dump(const char * msg);
#ifndef _WIN32
	void dump_maskbits(
		const char *msg,
		const char * post,
		fd_set * fdread,
		fd_set * fdwrite,
		fd_set * fdexp,
		int max);
#endif

	int printf(sPtr<tinyState> ,const char * fmt,...);
	int v_printf(sPtr<tinyState> ,const char * fmt,va_list arg);
	int printf_err(sPtr<tinyState> ,const char * fmt,...);
	int v_printf_err(sPtr<tinyState> ,const char * fmt,va_list arg);
	virtual int announce() {
		return realMaxfd;
	}
	virtual void announce(int msg) {
		announceMaxfd = msg;
	}

	static INTEGER64 start_time;
};


#define TSE_IO_SOCKET		1
#define TSE_IO_BIND		2
#define TSE_IO_LISTEN		3
#define TSE_IO_ACCEPT		4

#endif

