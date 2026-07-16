

#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/tinyState.h"
#include	"ts2/c++/tsSignal.h"
#include	"_ts2/c++/ssApplication_.h"

CLASS_TINYSTATE(ss/c++/ssApplication,ss/c++/ssObject)


#if 0

TS_BEGIN_IMPLEMENT


class TS_THISCLASS : public TS_BASECLASS {
public:
	ssApplication_(
		sPtr<tsApplication> parent,
		int argc,
		char ** argv);
	
private:
protected:
	TS_DEFARGS
	sPtr<tsSignal>		sig_pipe;
	sPtr<tsSignal>		sig_int;
	sPtr<tsSignal>		sig_usr1;
	sPtr<tsSignal>		sig_usr2;
	sPtr<tsSignal>		sig_term;
	sPtr<tsSignal>		sig_hup;
	sPtr<tsSignal>		sig_quit;
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tsApplication;

TS_END_INTERFACE

#endif


ssApplication_::ssApplication_(TS_ARGS0)
        : ssObject_(parent)
{
    TS_CPARGS0
}



/*******************************************
	INSTANCE FUNCTIONS
********************************************/




/*******************************************
	STATE MACHINE
********************************************/



TS_STATE(INI_ssObject_START)
{
	ssApp = ifThis;
	application->gc->exe(ifThis);
	return INI_RET;
}
TS_STATE(INI_RET)
{
	R_TEST


	sig_pipe = thNEW(tsSignal,(ifThis,SIGPIPE));
	sig_int =  thNEW(tsSignal,(ifThis,SIGINT));
	sig_usr1 = thNEW(tsSignal,(ifThis,SIGUSR1));
	sig_usr2 = thNEW(tsSignal,(ifThis,SIGUSR2));
	sig_term = thNEW(tsSignal,(ifThis,SIGTERM));
	sig_hup =  thNEW(tsSignal,(ifThis,SIGHUP));
	sig_quit = thNEW(tsSignal,(ifThis,SIGQUIT));
	return rDO|ACT_START;
}


TS_STATE(ACT_START)
{
	::printf("naming-conventions: ssApplication started\n");
	return rDO|FIN_START;
}

TS_STATE(FIN_START)
{
	sig_pipe->destroy();
	sig_int->destroy();
	sig_usr1->destroy();
	sig_usr2->destroy();
	sig_term->destroy();
	sig_hup->destroy();
	sig_quit->destroy();
	return rDO|FIN_ssObject_START;
}
