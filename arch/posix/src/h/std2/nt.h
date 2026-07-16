

#ifndef ___NT_IFCONFIG_cpp_H___
#define ___NT_IFCONFIG_cpp_H___

#include	"std2/includes.h"
#include	"std2/nt_m.h"


#define RTACTION_ADD   1
#define RTACTION_DEL   2
#define RTACTION_HELP  3
#define RTACTION_FLUSH 4
#define RTACTION_SHOW  5

#define FLAG_EXT       3		/* AND-Mask */
#define FLAG_NUM_HOST  4
#define FLAG_NUM_PORT  8
#define FLAG_NUM_USER 16
#define FLAG_NUM     (FLAG_NUM_HOST|FLAG_NUM_PORT|FLAG_NUM_USER)
#define FLAG_SYM      32
#define FLAG_CACHE    64
#define FLAG_FIB     128
#define FLAG_VERBOSE 256

#define E_NOTFOUND      8
#define E_SOCK          7
#define E_LOOKUP        6
#define E_VERSION       5
#define E_USAGE         4
#define E_OPTERR        3
#define E_INTERN        2
#define E_NOSUPP        1

struct nt_interface {
    struct interface *next, *prev; 
    char name[IFNAMSIZ];	/* interface name        */
    short type;			/* if type               */
    short flags;		/* various flags         */
    int metric;			/* routing metric        */
    int mtu;			/* MTU value             */
//    struct ifmap map;		/* hardware setup        */
    struct sockaddr addr;	/* IP address            */
    struct sockaddr dstaddr;	/* P-P IP address        */
    struct sockaddr broadaddr;	/* IP broadcast address  */
    struct sockaddr netmask;	/* IP network mask       */
    char hwaddr[32];		/* HW address            */
    unsigned	has_ip:1;
};


namespace ts2nif {

int nt_sockets_open(int family);
int nt_safe_close(int s);

int _if_fetch_m(int,struct nt_interface *);

int nt_ifconfig_test_flag(const char *ifname, short flags);
int nt_ifconfig_set_flag(const char *ifname, short flag);
int nt_ifconfig_clr_flag(const char *ifname, short flag);
int nt_ifconfig_fetch(struct nt_interface * inf);

int nt_route_set(int action, int options, const char **args);

}

#endif
