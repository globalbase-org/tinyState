#include	<cstdio>
#include	<cstdlib>
#include	<cstring>
#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/stdString.h"
#include	"hw/c++/hwRioBench.h"

/*
 * riobench — datagram throughput / burst-loss harness (separate srv/cli processes,
 * loopback OR real NIC).  See hwRioBench.cpp header comment for the protocol.
 *
 *   riobench srv <port> <mode> [depth] [slots]
 *       Receive datagrams, count them, stop on the client's END sentinel, report
 *       received/expected/loss + receive rate.  mode: 0=plain, 1=ENHANCED(RIO).
 *       depth (enhanced only) caps concurrently-posted RIO receives — the Phase A
 *       (depth 1) vs Phase B (depth 4) knob for burst-loss.  0/omitted = ring size.
 *       slots (enhanced only) sets the RIO ring slot count = posted-depth ceiling;
 *       deeper amortizes the per-notification cost (throughput knob).  0/omitted = 4.
 *
 *   riobench cli <ip> <port> <mode> <N> <size> <slots> [vbatch]
 *       Blast N datagrams of <size> bytes toward <ip>:<port>, then send END sentinels,
 *       report sent + send rate.  slots = client RIO ring depth; vbatch = sendmmsg vector
 *       length (messages handed per call, default 16).  mode as above.
 */
static int
usage()
{
	::printf("usage:\n"
		"  riobench srv <port> <mode> [depth] [slots] [vbatch]\n"
		"  riobench cli <ip> <port> <mode> <N> <size> <slots> [vbatch]\n"
		"  mode: 0=plain, 1=ENHANCED(RIO);  vbatch: recv/sendmmsg vector length (default 16)\n");
	return 2;
}

int
main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	if ( argc < 3 )
		return usage();

	if ( ::strcmp(argv[1],"srv") == 0 ) {
	int port  = atoi(argv[2]);
	int mode  = (argc > 3) ? atoi(argv[3]) : 0;	/* ignored (RIO auto; plain via TS2_DISABLE_RIO) */
	int depth  = (argc > 4) ? atoi(argv[4]) : 0;
	int slots  = (argc > 5) ? atoi(argv[5]) : 0;
	int vbatch = (argc > 6) ? atoi(argv[6]) : 0;
		thNEW(tsApplication,(thNULL,[port,mode,depth,slots,vbatch](sPtr<tsApplication> app){
			/* role 1 = server; host/N/size unused, batch carries slots */
			thNEW(hwRioBench,(app,1,port,mode,depth,thNULL,0,0,slots,vbatch));
		}));
		return 0;
	}
	if ( ::strcmp(argv[1],"cli") == 0 ) {
		if ( argc < 8 )
			return usage();
	sPtr<stdString> ip = thNEW(stdString,(argv[2]));
	int port  = atoi(argv[3]);
	int mode  = atoi(argv[4]);			/* ignored (RIO auto; plain via TS2_DISABLE_RIO) */
	int N     = atoi(argv[5]);
	int size  = atoi(argv[6]);
	int batch = atoi(argv[7]);
	int vbatch = (argc > 8) ? atoi(argv[8]) : 0;
		thNEW(tsApplication,(thNULL,[=](sPtr<tsApplication> app){
			/* role 0 = client */
			thNEW(hwRioBench,(app,0,port,mode,0,ip,N,size,batch,vbatch));
		}));
		return 0;
	}
	return usage();
}
