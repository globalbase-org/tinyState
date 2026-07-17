/*
 * hwTsChild — a *tinyState* child process that reads its stdin through the
 * framework (s2IOstd -> ts2IOdescriptor overlapped read + IOCP), exactly as a
 * real tinyState agent (e.g. cgalp's) does — as opposed to the plain _read()
 * helpers (echo1/count1) that a shell child like `sort` uses.
 *
 * It counts every byte on stdin until EOF, then writes "COUNT=<n>\n" to stdout.
 * Spawned by systest Phase E (SYSTEST_BIG_EXE=<this>.exe) it verifies whether
 * parent->child delivery reaches a child doing OVERLAPPED reads on the pipe end
 * that ts2System created *synchronous* (see the s2IOstd.cpp follow-up note).
 */
#include	"_ts2/c++/hwTsChild_.h"
#include	<stdio.h>
#include	<string.h>
CLASS_TINYSTATE(hw/c++/hwTsChild,ts2/c++/tinyState)
#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/s2IOstd.h"
#include	"ts2/c++/ts2IO.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	hwTsChild_(sPtr<tinyState> parent);
protected:
	TS_DEFARGS
	sPtr<ts2IO> in; sPtr<ts2IO> out; long total; char rbuf[65536]; char obuf[64];
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE
#endif
hwTsChild_::hwTsChild_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }

TS_STATE(INI_START) {
	total = 0;
	s2IOstd::init(ifThis,&in,&out,(sPtr<ts2IO>*)0);
	if ( in == thNULL || out == thNULL ) { ::printf("COUNT=-1\n"); return rDO|FIN_START; }
	return rDO|ACT_READ;
}
TS_STATE(ACT_READ) {			/* ev-independent, single read (鉄則5) */
	int n = in->read(rbuf,sizeof(rbuf));
	if ( n > 0 ) { total += n; return rDO|ACT_READ; }
	return rDO|ACT_REPORT;		/* n<=0 : EOF */
}
TS_STATE(ACT_REPORT) {			/* ev-independent, single write_c on member buf */
	snprintf(obuf,sizeof(obuf),"COUNT=%ld\n",total);
	out->write_c(obuf,(int)strlen(obuf));
	return rDO|FIN_START;
}
TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
