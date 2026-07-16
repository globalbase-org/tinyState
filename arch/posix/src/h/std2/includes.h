


#ifndef ___INCLUDES_H___
#define ___INCLUDES_H___


#include <math.h>
#include <stdint.h>
#include "tinyState_config.h"
#include	<errno.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<strings.h>
#include 	<sys/types.h>
#include 	<sys/time.h>
#include	<sys/socket.h>
#include	<sys/ioctl.h>
#include 	<unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include	<termios.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include 	<pthread.h>
#include	<dirent.h>
#include	<sys/uio.h>
#include	<stdarg.h>
#include	<signal.h>
#include	<netdb.h>

#include	"std2/m_include.h"


#define ___us		((INTEGER64)1)
#define ___ms		(1000*___us)
#define ___sec		(1000*___ms)
#define ___min		(60*___sec)
#define ___h		(60*___min)
#define ___day		(24*___h)
#endif

