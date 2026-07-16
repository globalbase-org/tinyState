#include	"_ts2/c++/hwPiCalc_.h"

CLASS_TINYSTATE(hw/c++/hwPiCalc,ts2/c++/tinyState)


#if 0

TS_BEGIN_IMPLEMENT

#include <cstdlib>
#include <ctime>
#include <cstdio>

/** @brief Monte Carlo π estimator running in a TS_THREAD.
 *
 * Each round samples _trials random points in the unit square and
 * counts how many fall inside the unit circle.  The heavy loop runs
 * inside TS_THREAD so mtx is released and other state machines
 * (e.g. hwIntervalTimer) keep ticking concurrently.
 */
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief Construct the estimator.
	 *  @param parent  Parent state machine.
	 *  @param _trials Monte Carlo trials per round.
	 */
	hwPiCalc_(sPtr<tinyState> parent, long long _trials);
private:
	double		_pi;
	double		_elapsed_sec;
	int		_round;
	unsigned	_seed;
protected:
	TS_DEFARGS
};

TS_END_IMPLEMENT

TS_BEGIN_INTERFACE
TS_END_INTERFACE

#endif


hwPiCalc_::hwPiCalc_(TS_ARGS0)
	: tinyState_(parent)
{
	TS_CPARGS0
}


TS_STATE(INI_START)
{
	_pi = 0.0;
	_elapsed_sec = 0.0;
	_round = 0;
	_seed = 12345;
	return rDO | ACT_COMPUTE;
}

/// Estimate π via Monte Carlo — releases mtx so ping-timer keeps ticking.
TS_THREAD(ACT_COMPUTE)
{
	long long inside = 0;
	struct timespec t0, t1;
	::clock_gettime(CLOCK_MONOTONIC, &t0);
	for ( long long i = 0; i < _trials; i++ ) {
		double x = (double)::rand_r(&_seed) / RAND_MAX;
		double y = (double)::rand_r(&_seed) / RAND_MAX;
		if ( x*x + y*y <= 1.0 ) inside++;
	}
	::clock_gettime(CLOCK_MONOTONIC, &t1);
	_pi = 4.0 * (double)inside / (double)_trials;
	_elapsed_sec = (double)(t1.tv_sec  - t0.tv_sec)
	             + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
	return rDO | ACT_REPORT;
}

/// Print the result, then start the next round.
TS_STATE(ACT_REPORT)
{
	_round++;
	::printf("π ≈ %.6f  (round %d, %lld trials, %.2f sec)\n",
		_pi, _round, _trials, _elapsed_sec);
	if ( is_destroyed() )
		return rDO | FIN_START;
	return rDO | ACT_COMPUTE;
}

/** @brief Graceful shutdown: print summary and exit. */
TS_STATE(FIN_START)
{
	::printf("quit π calc  (total %d rounds, last π ≈ %.6f)\n",
		_round, _pi);
	return rDO | FIN_TINYSTATE_START;
}
