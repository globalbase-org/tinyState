
#ifndef __stdFrameWork_cpp_H__
#define __stdFrameWork_cpp_H__

#include	"ts2/c++/ts_types.h"
#include	"ts2/c++/tinyState.h"

#define FWTR_WRITE	0x01
#define FWTR_READ	0x02
#define FWTR_INTERVAL	0x04
#define FWTR_ACTIVE	0x08
#define FWTR_ALL	0x0f
#define FWTR_RWI	(FWTR_WRITE|FWTR_READ|FWTR_INTERVAL)

class tsApplication;
class stdFrameWork : public stdObject {
protected:
public:
	virtual int wait(sPtr<tinyState> ,INTEGER64,int) = 0;
	virtual int loop(sPtr<tsApplication>) = 0;
	virtual int detach(sPtr<tinyState> ,int flags=FWTR_ALL) = 0;
	virtual int printf(sPtr<tinyState> ,const char * fmt,...) = 0;
	virtual int v_printf(sPtr<tinyState> ,const char * fmt,va_list arg) = 0;
	virtual int printf_err(sPtr<tinyState> ,const char * fmt,...) = 0;
	virtual int v_printf_err(sPtr<tinyState> ,const char * fmt,va_list arg) = 0;
	virtual void dump(const char *msg) = 0;
	virtual void announce(int  msg) {}
	virtual int announce() { return 0;}
	virtual void addRefio(sPtr<tinyState> obj = thNULL) {};
	virtual void delRefio(sPtr<tinyState> obj = thNULL) {};

	static 	const char * trace_all;
	static 	int8_t trace_bit;
};

#endif
