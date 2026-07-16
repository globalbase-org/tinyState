


#ifndef ___sObject_cpp_H___
#define ___sObject_cpp_H___

#if __cplusplus >= 201103L
#else
#include	<new>
#endif
#include	"ts2/c++/ts_types.h"
#include	"std2/includes.h"


class sObject {
public:
  	virtual ~sObject() {
	}
	static void * operator new(size_t cbSize,const char * __file,int __line)
#if __cplusplus >= 201103L
			noexcept(false);
#else
			throw(std::bad_alloc);
#endif
	static void operator delete(void * pv)
#if __cplusplus >= 201103L
    		noexcept(true);
#else
		throw();
#endif
	static void refLock();
	static void refUnlock();
	static uint8_t		refMutex_debug;


	[[noreturn]] static void panic(const char*);

	static INTEGER64 total_mem_max;

	static void test();
	static void test_one(void *);

	const char * get_sobj_file();
	int get_sobj_line();

	static void refine_sobj(void * mp,const char * file,int line);
	void refine_sobj(const char * file,int line);

	static void report_descriptor();

	static int __open(const char *,int,const char *,int,mode_t);
	static int __close(int);
	static int __fclose(FILE*);
	static int __pipe(const char *,int,int *);
	static int __socket(const char *,int,int,int,int);
	static int __accept(const char *,int,int,struct sockaddr *,socklen_t *);
	static int __openpty(const char *,int,
			int * amaster,
			int * aslave,
			char * name,
			struct termios * termp,
			struct winsize *winp);

	static void refine_fd(int s,const char * filename,int line);

	static void fd_stdio();

 protected:
	static pthread_mutex_t 	refMutex;
	static uint8_t		refMutex_init;
	static pthread_t	refMutex_thread;
	static int		refMutex_count;

};

#define soOPEN(x,y,z)	sObject::__open(__FILE__,__LINE__,x,y,z)
#define soCLOSE(x)	sObject::__close(x)
#define soPIPE(x)	sObject::__pipe(__FILE__,__LINE__,x)
#define soSOCKET(x,y,z)	sObject::__socket(__FILE__,__LINE__,x,y,z)
#define soACCEPT(x,y,z)	sObject::__accept(__FILE__,__LINE__,x,y,z)
#define soOPENPTY(x,y,z,a,b)	sObject::__openpty(__FILE__,__LINE__,x,y,z,a,b)
#define soFCLOSE(x)	sObject::__fclose(x)

void * operator new[](size_t cbSize,const char * __file,int __line)
#if __cplusplus >= 201103L
			noexcept(false);
#else
		throw(std::bad_alloc);
#endif
void operator delete[](void * pv)
#if __cplusplus >= 201103L
			noexcept(true);
#else
		throw();
#endif

#define COMMA	,
#define thNEW(__type,__arg)	(sPtr<__type>(new (__FILE__,__LINE__) __type __arg,1))



#endif
