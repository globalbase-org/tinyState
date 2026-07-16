#include	"_ts2/c++/hwSysTest_.h"
#include	<stdlib.h>
CLASS_TINYSTATE(hw/c++/hwSysTest,ts2/c++/tinyState)
#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2System.h"
#include	"ts2/c++/ts2IO.h"
#include	"ts2/c++/stdInterval.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwSysTest_(sPtr<tinyState> parent);
protected:
	TS_DEFARGS
	int retPid; sPtr<ts2IO> rfd; sPtr<tinyState> sys; char rbuf[256];
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE
#endif
hwSysTest_::hwSysTest_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }
TS_STATE(INI_START) { return rDO|ACT_START; }
TS_STATE(ACT_START) {
	sys = thNEW(ts2System,(ifThis,&retPid,"#cmd /c echo hello_from_child",&rfd,(sPtr<ts2IO>*)0,(sPtr<ts2IO>*)0));
	if ( retPid < 0 ) return rDO|FIN_START;
	return rDO|ACT_READ;
}
TS_STATE(ACT_READ) {
	int n = rfd->read(rbuf,sizeof(rbuf)-1);
	if ( n > 0 ) { rbuf[n]=0; ::printf("[systest] read: %s",rbuf); return rDO|ACT_READ; }
	::printf("[systest] EOF\n");
	return rDO|ACT_WAIT_EXIT;
}
TS_STATE(ACT_WAIT_EXIT) {
	if ( ev->type != TSE_RETURN ) return 0;
	if ( ev->source != sys ) return 0;
	::printf("[systest] child exited — OK\n");
	if ( ::getenv("SYSTEST_LINGER") ) {
		/* release rfd+sys NOW so their (IOCP descriptor) teardown runs while
		   fwIO is still alive, then linger 300ms before the app exits — so the
		   descriptor teardown does NOT coincide with fwIO's IOCP port-close. */
		rfd = thNULL;
		sys = thNULL;
		stdInterval::wait(ifThis,300*1000,TSE_TIMER);
		return rDO|ACT_LINGER;
	}
	return rDO|FIN_START;
}
TS_STATE(ACT_LINGER) {
	if ( ev->type != TSE_TIMER ) return 0;
	::printf("[systest] lingered — exit now\n");
	return rDO|FIN_START;
}
TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
