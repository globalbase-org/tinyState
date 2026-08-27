/*
 * hwRioRace — teardown-vs-completion churn harness for the MinGW RIO datagram path.
 *
 * The bug under test: rio_teardown() closes the threadpool wait / the RIO CQ while a
 * completion callback that already decided to re-arm is still in flight.  The window is a
 * few instructions wide, so this harness does two things to make it reachable:
 *
 *   1. CHURN — role 0 (victim) creates a UDP socket, receives a few datagrams (which is what
 *      builds RIO and posts receives), then destroy()s it immediately and does it again.
 *      Every destroy lands while completions are being delivered.
 *   2. FLOOD — role 1 (flooder) blasts datagrams at the victim's port continuously, so the
 *      CQ keeps signalling right through the victim's teardown.  Sends into a closed port
 *      fail (ICMP port-unreachable → WSAECONNRESET on Windows); the flooder just reopens its
 *      socket and keeps going.
 *
 * Widen the window with the instrumented library build (TS2_RIO_RACE_SLEEP_MS): it sleeps in
 * the completion callback between "decide to re-arm" and the SetThreadpoolWait, which is
 * exactly the preemption the ticket describes.
 *
 * Buffers and the recvfrom source address are MEMBERS (they must survive the yield across
 * the op), and every datagram op is its own ev-independent state (CLAUDE.md 鉄則 5).
 */
#define	RIORACE_MAX_FANOUT	16	/* must precede _.h: fan[] is a member array */
#include	"_ts2/c++/hwRioRace_.h"
#include	<string.h>

CLASS_TINYSTATE(hw/c++/hwRioRace,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockUDP.h"
#include	"ts2/c++/stdInterval.h"
#include	"ts2/c++/tsThread.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief role 0 = victim (create/receive/destroy churn on vport, spawns the flooder),
	 *  role 1 = flooder (owns fport, blasts datagrams at vport).  rounds = churn count.
	 *  fanout = how many sockets the victim tears down AT ONCE (1 = one at a time, the
	 *  original churn).  Above 1 the round builds `fanout` sockets, gets each to the point
	 *  where it owns RIO state, then destroys them all in the same turn — so several FIN
	 *  TS_THREADs are queued together and the pool has to drain a backlog while shutting
	 *  down.  That is the state a single-socket churn never reaches. */
	hwRioRace_(sPtr<tinyState> parent, int vport, int fport, int rounds, int role, int fanout,
			int tlimit, int raiseto);
protected:
	TS_DEFARGS
	sPtr<tinyState>		flooder;	/* victim: the child flooder */
	sPtr<ts2IOsockUDP>	udp;		/* this role's socket (fanout==1 path) */
	sPtr<ts2IOsockUDP>	fan[RIORACE_MAX_FANOUT];	/* victim: the simultaneous set */
	int			nfan;		/* how many of fan[] are live */
	int			raised;		/* victim: rounds where the limit was raised mid-teardown */
	int			fi;		/* index while building / draining fan[] */
	struct sockaddr_in	dest;		/* flooder: 127.0.0.1:vport */
	struct sockaddr		from;		/* recvfrom source (must persist over yield) */
	int			flen;
	int			iter;		/* victim: rounds done */
	int			got;		/* victim: datagrams this round */
	int			sent;		/* victim: tail sends posted this round */
	int			burst;		/* flooder: sends left in this burst */
	int			reopens;	/* flooder: socket reopens (ICMP resets) */
	char			wbuf[8];
	char			rbuf[64];
};
TS_END_IMPLEMENT
TS_BEGIN_INTERFACE
#include	"ts2/c++/sRptr.h"
class tinyState;
class ts2IOsockUDP;
TS_END_INTERFACE
#endif

#define	RIORACE_PER_ROUND	3	/* datagrams the victim takes before it kills the socket */
#define	RIORACE_BURST		16	/* flooder datagrams between 1 ms breathers */
#define	RIORACE_TAIL_SENDS	6	/* victim datagrams posted just before destroy() */

hwRioRace_::hwRioRace_(TS_ARGS0) : tinyState_(parent) { TS_CPARGS0 }


/*******************************************
	STATE MACHINE
********************************************/

TS_STATE(INI_START)
{
	iter = 0; got = 0; sent = 0; burst = 0; reopens = 0; nfan = 0; fi = 0; raised = 0;
	if ( role != 1 ) {
		::printf("[riorace] victim=%d flooder=%d rounds=%d fanout=%d\n",
				vport,fport,rounds,fanout);
		/* The worker cap is a property of the whole application, so only the victim sets
		   it, and only when asked: with tlimit 0 the pool keeps its default unlimited
		   growth and this harness behaves exactly as it did before. */
		if ( tlimit > 0 ) {
		sPtr<tsThread> th = application->getThread();
			if ( th.is_notNull() ) {
				th->limitThreadsNumber(tlimit);
				::printf("[riorace] worker limit set to %d (pool now %s, reports %d)\n",
						tlimit,th->getStateName(),th->limitThreadsNumber());
			}
		}
		flooder = thNEW(hwRioRace,(ifThis,vport,fport,rounds,1,fanout,0,0));
		return rDO|(fanout > 1 ? ACT_FAN_ROUND : ACT_V_ROUND);
	}
	return rDO|ACT_F_OPEN;
}


/* ---- role 1: flooder — keep datagrams landing on the victim port ---- */

TS_STATE(ACT_F_OPEN)
{
	if ( is_destroyed() )
		return rDO|ACT_CLEANUP;
	udp = thNEW(ts2IOsockUDP,(ifThis,fport));
	return ACT_F_READY;
}
TS_STATE(ACT_F_READY)
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( udp->err != 0 ) {
		::printf("[riorace] flooder bind failed (err=%d)\n",udp->err);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_F_ASSOC;
}
TS_STATE(ACT_F_ASSOC)
{
	if ( C_TEST(udp->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,20,TSE_TIMER);
		return ACT_F_ASSOC_TICK;
	}
	return rDO|ACT_F_SEND;
}
TS_STATE(ACT_F_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_F_ASSOC;
}

TS_STATE(ACT_F_SEND)			/* ev-independent, exactly one sendto */
{
	if ( is_destroyed() )
		return rDO|ACT_CLEANUP;
	memset(&dest,0,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(vport);
	dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	memcpy(wbuf,"FLOOD",5);
	if ( udp->sendto(wbuf,5,0,(struct sockaddr*)&dest,sizeof(dest)) < 0 )
		return rDO|ACT_F_REOPEN;		/* ICMP unreachable while the victim is down */
	if ( ++burst < RIORACE_BURST )
		return rDO|ACT_F_SEND;
	burst = 0;
	stdInterval::wait(ifThis,1,TSE_TIMER);		/* breathe: let the victim run */
	return ACT_F_TICK;
}
TS_STATE(ACT_F_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_F_SEND;
}

TS_STATE(ACT_F_REOPEN)			/* the send errored (victim port closed) — fresh socket */
{
	reopens++;
	burst = 0;
	if ( udp != thNULL )	udp->destroy();
	udp = thNULL;
	stdInterval::wait(ifThis,2,TSE_TIMER);
	return ACT_F_REOPEN_TICK;
}
TS_STATE(ACT_F_REOPEN_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_F_OPEN;
}


/* ---- role 0: victim — create / receive / destroy, over and over ---- */

TS_STATE(ACT_V_ROUND)
{
	if ( is_destroyed() )
		return rDO|ACT_CLEANUP;
	if ( iter >= rounds ) {
		::printf("[riorace] %d/%d rounds survived teardown\n",iter,rounds);
		return rDO|ACT_CLEANUP;
	}
	got = 0;
	udp = thNEW(ts2IOsockUDP,(ifThis,vport));
	return ACT_V_READY;
}
TS_STATE(ACT_V_READY)
{
	if ( ev->type != TSE_WAKEUP )
		return 0;
	if ( udp->err != 0 ) {
		::printf("[riorace] victim bind failed round=%d (err=%d)\n",iter,udp->err);
		return rDO|ACT_CLEANUP;
	}
	return rDO|ACT_V_ASSOC;
}
TS_STATE(ACT_V_ASSOC)
{
	if ( C_TEST(udp->tinyState::state(),C_INI) ) {
		stdInterval::wait(ifThis,5,TSE_TIMER);
		return ACT_V_ASSOC_TICK;
	}
	return rDO|ACT_V_RECV;
}
TS_STATE(ACT_V_ASSOC_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_V_ASSOC;
}

TS_STATE(ACT_V_RECV)			/* ev-independent, exactly one recvfrom (builds+posts RIO) */
{
	flen = sizeof(from);
	if ( udp->recvfrom(0,rbuf,(int)sizeof(rbuf),0,&from,&flen) < 0 )
		return rDO|ACT_V_KILL;			/* socket gone — count the round anyway */
	if ( ++got < RIORACE_PER_ROUND )
		return rDO|ACT_V_RECV;
	sent = 0;
	return rDO|ACT_V_SEND;
}

/* Post sends right before the kill.  This is what makes the race reachable: the receive
   ring fills up under the flood and stops posting receives, so no receive completion is
   left to signal the CQ during teardown.  Sends DO complete during teardown, so a wait
   re-armed by a leaked callback actually fires. */
TS_STATE(ACT_V_SEND)			/* ev-independent, exactly one sendto */
{
	memset(&dest,0,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(fport);
	dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	memcpy(wbuf,"BYE",3);
	if ( udp->sendto(wbuf,3,0,(struct sockaddr*)&dest,sizeof(dest)) < 0 )
		return rDO|ACT_V_KILL;
	if ( ++sent < RIORACE_TAIL_SENDS )
		return rDO|ACT_V_SEND;
	return rDO|ACT_V_KILL;
}

/* THE RACE: destroy() with receives posted and completions being delivered — rio_teardown
   runs (TS_THREAD, app-mtx released) while the pool thread is inside the completion
   callback deciding whether to re-arm the wait. */
TS_STATE(ACT_V_KILL)
{
	if ( udp != thNULL )
		udp->destroy();
	return rDO|ACT_V_ZOM;
}
TS_STATE(ACT_V_ZOM)			/* poll the socket to ZOM (CLAUDE.md 鉄則 5-2) */
{
	if ( udp != thNULL && ! C_TEST(udp->tinyState::state(),C_ZOM) ) {
		stdInterval::wait(ifThis,2,TSE_TIMER);
		return ACT_V_ZOM_TICK;
	}
	udp = thNULL;
	iter++;
	if ( (iter % 10) == 0 )
		::printf("[riorace] round %d ok\n",iter);
	return rDO|ACT_V_ROUND;
}
TS_STATE(ACT_V_ZOM_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_V_ZOM;
}


/* ---- role 0 variant: tear down `fanout` sockets in the SAME turn ----
 *
 * The single-socket churn only ever has one FIN TS_THREAD outstanding, so the pool never
 * has a queue to drain while the app is shutting down.  Here every round builds `fanout`
 * sockets on consecutive ports, makes each of them own RIO state (one sendto is enough —
 * that is what runs ensure_rio and posts into the ring), and then destroys the whole set
 * without yielding in between.  The FIN threads are queued together, so with a small
 * worker limit the pool must work through a backlog with `ready` non-empty.
 */

TS_STATE(ACT_FAN_ROUND)
{
	if ( is_destroyed() )
		return rDO|ACT_CLEANUP;
	if ( iter >= rounds ) {
		::printf("[riorace] %d/%d rounds survived teardown (fanout=%d, limit=%d, raised=%d)\n",
				iter,rounds,fanout,tlimit,raised);
		return rDO|ACT_CLEANUP;
	}
	nfan = fanout > RIORACE_MAX_FANOUT ? RIORACE_MAX_FANOUT : fanout;
	for ( fi = 0 ; fi < nfan ; fi++ )
		fan[fi] = thNEW(ts2IOsockUDP,(ifThis,vport + fi));
	fi = 0;
	return ACT_FAN_READY;
}
TS_STATE(ACT_FAN_READY)			/* poll the set to "past INI" (CLAUDE.md 鉄則 5-2) */
{
	for ( int k = 0 ; k < nfan ; k++ ) {
		if ( fan[k] == thNULL )
			continue;
		if ( fan[k]->err != 0 ) {
			::printf("[riorace] fan bind failed round=%d idx=%d (err=%d)\n",
					iter,k,fan[k]->err);
			return rDO|ACT_CLEANUP;
		}
		if ( C_TEST(fan[k]->tinyState::state(),C_INI) ) {
			stdInterval::wait(ifThis,5,TSE_TIMER);
			return ACT_FAN_READY_TICK;
		}
	}
	fi = 0;
	return rDO|ACT_FAN_SEND;
}
TS_STATE(ACT_FAN_READY_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_FAN_READY;
}

TS_STATE(ACT_FAN_SEND)			/* ev-independent, exactly one sendto per state */
{
	if ( fi >= nfan )
		return rDO|ACT_FAN_KILL;
	memset(&dest,0,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(fport);
	dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	memcpy(wbuf,"FAN",3);
	fan[fi]->sendto(wbuf,3,0,(struct sockaddr*)&dest,sizeof(dest));	/* builds RIO */
	fi++;
	return rDO|ACT_FAN_SEND;
}

/* THE POINT: destroy the whole set without yielding, so every FIN TS_THREAD is queued
   before any of them can run. */
TS_STATE(ACT_FAN_KILL)
{
	for ( int k = 0 ; k < nfan ; k++ )
		if ( fan[k] != thNULL )
			fan[k]->destroy();
	/* Optionally widen the cap with the FIN threads already queued.  Whether the pool then
	   grows depends on WHICH state it is in, and that is not something to guess at from the
	   outside: while the pool is still in its ACT_* cycle the usual auto-growth applies and
	   it will grow (that is correct, not a fault); once it has passed FIN_DRAINED its target
	   is 0 and re-applying the cap re-applies 0.  So record the state name next to the call
	   instead of asserting an outcome — the log then says which window this run was in. */
	if ( raiseto > 0 ) {
	sPtr<tsThread> th = application->getThread();
		if ( th.is_notNull() ) {
			::printf("[riorace] round %d: raising limit %d -> %d while pool is in %s\n",
					iter,th->limitThreadsNumber(),raiseto,th->getStateName());
			th->limitThreadsNumber(raiseto);
			raised++;
		}
	}
	return rDO|ACT_FAN_ZOM;
}
TS_STATE(ACT_FAN_ZOM)
{
	for ( int k = 0 ; k < nfan ; k++ ) {
		if ( fan[k] == thNULL )
			continue;
		if ( ! C_TEST(fan[k]->tinyState::state(),C_ZOM) ) {
			stdInterval::wait(ifThis,2,TSE_TIMER);
			return ACT_FAN_ZOM_TICK;
		}
	}
	for ( int k = 0 ; k < nfan ; k++ )
		fan[k] = thNULL;
	nfan = 0;
	iter++;
	if ( (iter % 10) == 0 )
		::printf("[riorace] round %d ok (fanout)\n",iter);
	return rDO|ACT_FAN_ROUND;
}
TS_STATE(ACT_FAN_ZOM_TICK)
{
	if ( ev->type != TSE_TIMER )
		return 0;
	return rDO|ACT_FAN_ZOM;
}


/* ---- shared cleanup ---- */

TS_STATE(ACT_CLEANUP)
{
	if ( flooder != thNULL )	flooder->destroy();
	if ( udp != thNULL )		udp->destroy();
	for ( int k = 0 ; k < nfan ; k++ )
		if ( fan[k] != thNULL ) { fan[k]->destroy(); fan[k] = thNULL; }
	nfan = 0;
	flooder = thNULL;
	udp = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
