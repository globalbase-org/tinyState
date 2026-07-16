#include	"_ts2/c++/hwRegexTest_.h"

#include	<cstdio>
#include	<cstdlib>
#include	"ts2/c++/stdString.h"
#include	"ts2/c++/stdRx.h"

CLASS_TINYSTATE(hw/c++/hwRegexTest,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

class TS_THISCLASS : public TS_BASECLASS {
public:
	hwRegexTest_(
		sPtr<tinyState> parent);
private:
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
// predefine
#include	"ts2/c++/sRptr.h"
class tinyState;
TS_END_INTERFACE

#endif


hwRegexTest_::hwRegexTest_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
}


/*
 * Assert that matching `pat` against `in` yields the whole-match substring
 * `expect`.  The point is POSIX ERE *leftmost-longest*: e.g. "a|ab" on "ab"
 * matches "ab", not "a" (which a leftmost-first / std::regex engine returns).
 */
static int
check_whole(const char * pat,const char * in,const char * expect)
{
sPtr<stdString>	s  = thNEW( stdString,(in));
sPtr<stdRx>	re = thNEW( stdRx,(pat));
sPtr<stdString>	hit = s->rx("mv",re);
	const char * got = "<no-match>";
	int ok = 0;
	if ( hit.is_notNull() && s->rxVar(0).is_notNull() ) {
		got = s->rxVar(0)->get_str();
		ok = (s->rxVar(0)->cmp(expect) == 0);
	}
	::printf("  %-6s pat=%-30s in=%-11s -> whole='%s' (want '%s')\n",
		ok ? "[ok]" : "[FAIL]", pat, in, got, expect);
	return ok ? 0 : 1;
}

/* Assert capture group `grp` of `pat` on `in` equals `expect`. */
static int
check_group(const char * pat,const char * in,int grp,const char * expect)
{
sPtr<stdString>	s  = thNEW( stdString,(in));
sPtr<stdRx>	re = thNEW( stdRx,(pat));
sPtr<stdString>	hit = s->rx("mv",re);
	const char * got = "<no-match>";
	int ok = 0;
	if ( hit.is_notNull() && s->rxVar(grp).is_notNull() ) {
		got = s->rxVar(grp)->get_str();
		ok = (s->rxVar(grp)->cmp(expect) == 0);
	}
	::printf("  %-6s pat=%-30s in=%-11s -> $%d='%s' (want '%s')\n",
		ok ? "[ok]" : "[FAIL]", pat, in, grp, got, expect);
	return ok ? 0 : 1;
}


/*******************************************
	STATE MACHINE
********************************************/


TS_STATE(INI_START)
{
	return rDO|ACT_START;
}

TS_STATE(ACT_START)
{
int fails = 0;
	::printf("regextest: POSIX ERE leftmost-longest via stdRx\n");

	/* Leftmost-longest: the whole match spans the LONGER alternative. */
	fails += check_whole("a|ab",         "ab",       "ab");
	fails += check_whole("(foo|foobar)", "foobar",   "foobar");
	/* Greedy quantifier. */
	fails += check_whole("a*",           "aaa",      "aaa");
	/* A real stdRx pattern (ts2System / stdString $-substitution shape). */
	fails += check_whole("\\$([0-9]+)|\\$\\{([0-9]+)\\}", "x${42}y", "${42}");
	/* Capture group extraction. */
	fails += check_group("^([^ \t]+)",   "word rest", 1, "word");

	if ( fails ) {
		::printf("regextest: %d FAILURE(s)\n",fails);
		::exit(1);
	}
	::printf("regextest: all checks passed\n");
	return rDO|FIN_START;
}

TS_STATE(FIN_START)
{
	return rDO|FIN_TINYSTATE_START;
}
