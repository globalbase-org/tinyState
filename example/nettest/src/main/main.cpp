#include	<cstdio>
#include	<cstdlib>
#include	<cstring>
#include	"ts2/c++/tsApplication.h"
#include	"ts2/c++/stdString.h"
#include	"hw/c++/hwNetTest.h"

/*
 * Cross-machine TCP test — server and client split so they can run on
 * different hosts (real network, not loopback).  Windows-port design memo.
 *
 *   nettest server <port>          bind INADDR_ANY:port, accept loop, PING->PONG
 *   nettest client <host> <port>   connect host:port, send PING, expect PONG
 */
int main(int argc, char** argv)
{
	setvbuf(stdout,NULL,_IONBF,0);
	setvbuf(stderr,NULL,_IONBF,0);
	int is_server = (argc > 1 && strcmp(argv[1],"server") == 0) ? 1 : 0;
	const char * host = "127.0.0.1";
	int port = 47800;
	if ( is_server ) {
		if ( argc > 2 ) port = atoi(argv[2]);
	} else {
		if ( argc > 2 ) host = argv[2];
		if ( argc > 3 ) port = atoi(argv[3]);
	}
	thNEW(tsApplication,(thNULL,[is_server,host,port](sPtr<tsApplication> app){
		thNEW(hwNetTest,(app,is_server,thNEW(stdString,(host)),port));
	}));
}
