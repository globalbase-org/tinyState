/*
 * POSIX <regex.h> for the MinGW (native Windows) target, backed by vendored TRE.
 *
 * MinGW-w64 has no POSIX <regex.h> (regcomp/regexec). tinyState's stdRx /
 * stdString use the POSIX C regex API, so on MinGW this header supplies it by
 * mapping the POSIX names onto TRE (a true POSIX-ERE engine: leftmost-longest,
 * linear-time). Linux / macOS / Cygwin keep their system <regex.h>; only the
 * MinGW arch overlay pulls this file. See Windows-port design memo.
 *
 * TRE gives POSIX-correct results where the previous std::regex shim diverged
 * (e.g. "a|ab" on "ab" matches "ab", not "a").
 */
#ifndef ___TINYSTATE_MINGW_TRE_REGEX_H___
#define ___TINYSTATE_MINGW_TRE_REGEX_H___

#include "tre.h"   /* regex_t, regmatch_t, regoff_t, REG_* + tre_reg* */

/* Expose the TRE implementations under the standard POSIX names. */
#define regcomp   tre_regcomp
#define regexec   tre_regexec
#define regfree   tre_regfree
#define regerror  tre_regerror

#endif /* ___TINYSTATE_MINGW_TRE_REGEX_H___ */
