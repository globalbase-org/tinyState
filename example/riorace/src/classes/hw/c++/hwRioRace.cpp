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
#include	"_ts2/c++/hwRioRace_.h"
#include	<string.h>

CLASS_TINYSTATE(hw/c++/hwRioRace,ts2/c++/tinyState)

#if 0
TS_BEGIN_IMPLEMENT
#include	"ts2/c++/ts2IOsockUDP.h"
#include	"ts2/c++/stdInterval.h"
class TS_THISCLASS : public TS_BASECLASS {
public:
	/** @brief role 0 = victim (create/receive/destroy churn on vport, spawns the flooder),
	 *  role 1 = flooder (owns fport, blasts datagrams at vport).  rounds = churn count. */
	hwRioRace_(sPtr<tinyState> parent, int vport, int fport, int rounds, int role);
protected:
	TS_DEFARGS
	sPtr<tinyState>		flooder;	/* victim: the child flooder */
	sPtr<ts2IOsockUDP>	udp;		/* this role's socket */
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
	iter = 0; got = 0; sent = 0; burst = 0; reopens = 0;
	if ( role != 1 ) {
		::printf("[riorace] victim=%d flooder=%d rounds=%d\n",vport,fport,rounds);
		flooder = thNEW(hwRioRace,(ifThis,vport,fport,rounds,1));
		return rDO|ACT_V_ROUND;
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


/* ---- shared cleanup ---- */

TS_STATE(ACT_CLEANUP)
{
	if ( flooder != thNULL )	flooder->destroy();
	if ( udp != thNULL )		udp->destroy();
	flooder = thNULL;
	udp = thNULL;
	return rDO|FIN_START;
}

TS_STATE(FIN_START) { return rDO|FIN_TINYSTATE_START; }
