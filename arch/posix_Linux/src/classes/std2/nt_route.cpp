/*
 * route        This file contains an implementation of the command
 *              that manages the IP routing table in the kernel.
 *
 * Version:     $Id: route.c,v 1.10 2002-07-30 05:24:20 ecki Exp $
 *
 * Maintainer:  Bernd 'eckes' Eckenfels, <net-tools@lina.inka.de>
 *
 * Author:      Fred N. van Kempen, <waltje@uwalt.nl.mugnet.org>
 *              (derived from FvK's 'route.c     1.70    01/04/94')
 *
 * Modifications:
 *              Johannes Stille:        for Net-2Debugged by 
 *                                      <johannes@titan.os.open.de>
 *              Linus Torvalds:         Misc Changes
 *              Alan Cox:               add the new mtu/window stuff
 *              Miquel van Smoorenburg: rt_add and rt_del
 *       {1.79} Bernd Eckenfels:        route_info
 *       {1.80} Bernd Eckenfels:        reject, metric, irtt, 1.2.x support.
 *       {1.81} Bernd Eckenfels:        reject routes need a dummy device
 *960127 {1.82} Bernd Eckenfels:        'mod' and 'dyn' 'reinstate' added
 *960129 {1.83} Bernd Eckenfels:        resolve and getsock now in lib/, 
 *                                      REJECT displays '-' as gatway.
 *960202 {1.84} Bernd Eckenfels:        net-features support added
 *960203 {1.85} Bernd Eckenfels:        "#ifdef' in '#if' for net-features
 *                                      -A  (aftrans) support, get_longopts
 *960206 {1.86} Bernd Eckenfels:        route_init();
 *960218 {1.87} Bernd Eckenfels:        netinet/in.h added
 *960221 {1.88} Bernd Eckenfels:        aftrans_dfl support
 *960222 {1.90} Bernd Eckenfels:        moved all AF specific code to lib/.
 *960413 {1.91} Bernd Eckenfels:        new RTACTION support+FLAG_CACHE/FIB
 *960426 {1.92} Bernd Eckenfels:        FLAG_SYM/-N support
 *960823 {x.xx} Frank Strauss:          INET6 stuff
 *980629 {1.95} Arnaldo Carvalho de Melo: gettext instead of catgets
 *990101 {1.96} Bernd Eckenfels:	fixed usage and FLAG_CACHE Output
 *20010404 {1.97} Arnaldo Carvalho de Melo: use setlocale
 *
 */


#include	"std2/nt.h"
#include	<asm/param.h>
#include 	<netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include	<net/route.h>
#include	<errno.h>
#include	<ctype.h>
#include	"std2/safe_strncpy.h"

namespace ts2nif {

#define mask_in_addr(x) (((struct sockaddr_in *)&((x).rt_genmask))->sin_addr.s_addr)
#define full_mask(x) (x)
/*
#define mask_in_addr(x) ((x).rt_genmask)
#define full_mask(x) (((struct sockaddr_in *)&(x))->sin_addr.s_addr)
*/



static int usage()
{
	return E_USAGE;
}

static void INET_reserror(char *text)
{
    herror(text);
}


static int INET_resolve(char *name, struct sockaddr_in *sin, int hostfirst)
{
    struct hostent *hp;
    struct netent *np;

    /* Grmpf. -FvK */
    sin->sin_family = AF_INET;
    sin->sin_port = 0;

    /* Default is special, meaning 0.0.0.0. */
    if (!strcmp(name, "default")) {
	sin->sin_addr.s_addr = INADDR_ANY;
	return (1);
    }
    /* Look to see if it's a dotted quad. */
    if (inet_aton(name, &sin->sin_addr)) {
	return 0;
    }
    /* If we expect this to be a hostname, try hostname database first */
#ifdef DEBUG
    if (hostfirst) fprintf (stderr, "gethostbyname (%s)\n", name);
#endif
    if (hostfirst && 
	(hp = gethostbyname(name)) != (struct hostent *) NULL) {
	memcpy((char *) &sin->sin_addr, (char *) hp->h_addr_list[0], 
		sizeof(struct in_addr));
	return 0;
    }
    /* Try the NETWORKS database to see if this is a known network. */
#ifdef DEBUG
    fprintf (stderr, "getnetbyname (%s)\n", name);
#endif
    if ((np = getnetbyname(name)) != (struct netent *) NULL) {
	sin->sin_addr.s_addr = htonl(np->n_net);
	return 1;
    }
    if (hostfirst) {
	/* Don't try again */
	errno = h_errno;
	return -1;
    }
#ifdef DEBUG
    res_init();
    _res.options |= RES_DEBUG;
#endif

#ifdef DEBUG
    fprintf (stderr, "gethostbyname (%s)\n", name);
#endif
    if ((hp = gethostbyname(name)) == (struct hostent *) NULL) {
	errno = h_errno;
	return -1;
    }
    memcpy((char *) &sin->sin_addr, (char *) hp->h_addr_list[0], 
	   sizeof(struct in_addr));

    return 0;
}

static int INET_getsock(char *bufp, struct sockaddr *sap)
{
    char *sp = bufp, *bp;
    unsigned int i;
    unsigned val;
    struct sockaddr_in *sin;

    sin = (struct sockaddr_in *) sap;
    sin->sin_family = AF_INET;
    sin->sin_port = 0;

    val = 0;
    bp = (char *) &val;
    for (i = 0; i < sizeof(sin->sin_addr.s_addr); i++) {
	*sp = toupper(*sp);

	if ((*sp >= 'A') && (*sp <= 'F'))
	    bp[i] |= (int) (*sp - 'A') + 10;
	else if ((*sp >= '0') && (*sp <= '9'))
	    bp[i] |= (int) (*sp - '0');
	else
	    return (-1);

	bp[i] <<= 4;
	sp++;
	*sp = toupper(*sp);

	if ((*sp >= 'A') && (*sp <= 'F'))
	    bp[i] |= (int) (*sp - 'A') + 10;
	else if ((*sp >= '0') && (*sp <= '9'))
	    bp[i] |= (int) (*sp - '0');
	else
	    return (-1);

	sp++;
    }
    sin->sin_addr.s_addr = htonl(val);

    return (sp - bufp);
}


static int INET_getnetmask(char *adr, struct sockaddr *m, char *name)
{
    struct sockaddr_in *mask = (struct sockaddr_in *) m;
    char *slash, *end;
    int prefix;

    if ((slash = strchr(adr, '/')) == NULL)
	return 0;

    *slash++ = '\0';
    prefix = strtoul(slash, &end, 0);
    if (*end != '\0')
	return -1;

    if (name) {
	sprintf(name, "/%d", prefix);
    }
    mask->sin_family = AF_INET;
    mask->sin_addr.s_addr = htonl(~(0xffffffffU >> prefix));
    return 1;
}
static int INET_input(int type, char *bufp, struct sockaddr *sap)
{
    switch (type) {
    case 1:
	return (INET_getsock(bufp, sap));
    case 256:
	return (INET_resolve(bufp, (struct sockaddr_in *) sap, 1));
    default:
	return (INET_resolve(bufp, (struct sockaddr_in *) sap, 0));
    }
}



static int INET_setroute(int skfd,int action, int options,const char **args)
{
    struct rtentry rt;
    char target[128], gateway[128] = "NONE", netmask[128] = "default";
    int xflag, isnet;

    xflag = 0;

    if (!strcmp(*args, "#net")) {
	xflag = 1;
	args++;
    } else if (!strcmp(*args, "#host")) {
	xflag = 2;
	args++;
    }
    if (*args == NULL)
	return (usage());

    ts2nif::safe_strncpy(target, *args++, (sizeof target));

    /* Clean out the RTREQ structure. */
    memset((char *) &rt, 0, sizeof(struct rtentry));

    /* Special hack for /prefix syntax */
    {
	union {
	    struct sockaddr_in m;
	    struct sockaddr d;
	} mask;
	int n;

	n = INET_getnetmask(target, &mask.d, netmask);
	if (n < 0)
	    return usage();
	else if (n)
	    rt.rt_genmask = full_mask(mask.d);
    }

    /* Prefer hostname lookup is -host flag was given */
    if ((isnet = INET_input((xflag!=2? 0: 256), target, &rt.rt_dst)) < 0) {
	INET_reserror(target);
	return (1);
    }
    switch (xflag) {
    case 1:
       isnet = 1; break;
    case 2:
       isnet = 0; break;
    }

    /* Fill in the other fields. */
    rt.rt_flags = (RTF_UP | RTF_HOST);
    if (isnet)
	rt.rt_flags &= ~RTF_HOST;

    while (*args) {
	if (!strcmp(*args, "metric")) {
	    int metric;

	    args++;
	    if (!*args || !isdigit(**args))
		return (usage());
	    metric = atoi(*args);
	    rt.rt_metric = metric + 1;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "netmask")) {
	    struct sockaddr mask;

	    args++;
	    if (!*args || mask_in_addr(rt))
		return (usage());
	    safe_strncpy(netmask, *args, (sizeof netmask));
	    if ((isnet = INET_input(0, netmask, &mask)) < 0) {
		INET_reserror(netmask);
		return (E_LOOKUP);
	    }
	    rt.rt_genmask = full_mask(mask);
	    args++;
	    continue;
	}
	if (!strcmp(*args, "gw") || !strcmp(*args, "gateway")) {
	    args++;
	    if (!*args)
		return (usage());
	    if (rt.rt_flags & RTF_GATEWAY)
		return (usage());
	    safe_strncpy(gateway, *args, (sizeof gateway));
	    if ((isnet = INET_input(256, gateway, &rt.rt_gateway)) < 0) {
		INET_reserror(gateway);
		return (E_LOOKUP);
	    }
	    if (isnet) {
		fprintf(stderr, "route: %s: cannot use a NETWORK as gateway!\n",
			gateway);
		return (E_OPTERR);
	    }
	    rt.rt_flags |= RTF_GATEWAY;
	    args++;
	    continue;
	}
	if (!strcmp(*args, "mss") || !strcmp(*args,"mtu")) {
	    args++;
	    rt.rt_flags |= RTF_MSS;
	    if (!*args)
		return (usage());
	    rt.rt_mss = atoi(*args);
	    args++;
	    if (rt.rt_mss < 64 || rt.rt_mss > 65536) {
		fprintf(stderr, "route: Invalid MSS/MTU.\n");
		return (E_OPTERR);
	    }
	    continue;
	}
	if (!strcmp(*args, "window")) {
	    args++;
	    if (!*args)
		return (usage());
	    rt.rt_flags |= RTF_WINDOW;
	    rt.rt_window = atoi(*args);
	    args++;
	    if (rt.rt_window < 128) {
		fprintf(stderr, "route: Invalid window.\n");
		return (E_OPTERR);
	    }
	    continue;
	}
	if (!strcmp(*args, "irtt")) {
	    args++;
	    if (!*args)
		return (usage());
	    args++;
//#if HAVE_RTF_IRTT
	    rt.rt_flags |= RTF_IRTT;
	    rt.rt_irtt = atoi(*(args - 1));
	    rt.rt_irtt *= (HZ / 100);	/* FIXME */
//#if 0				/* FIXME: do we need to check anything of this? */
	    continue;
	}
	if (!strcmp(*args, "reject")) {
	    args++;
//#if HAVE_RTF_REJECT
	    rt.rt_flags |= RTF_REJECT;
//#else
	    continue;
	}
	if (!strcmp(*args, "mod")) {
	    args++;
	    rt.rt_flags |= RTF_MODIFIED;
	    continue;
	}
	if (!strcmp(*args, "dyn")) {
	    args++;
	    rt.rt_flags |= RTF_DYNAMIC;
	    continue;
	}
	if (!strcmp(*args, "reinstate")) {
	    args++;
	    rt.rt_flags |= RTF_REINSTATE;
	    continue;
	}
	if (!strcmp(*args, "device") || !strcmp(*args, "dev")) {
	    args++;
	    if (rt.rt_dev || *args == NULL)
		return usage();
	    rt.rt_dev = (char*)(*args++);
	    continue;
	}
	/* nothing matches */
	if (!rt.rt_dev) {
	    rt.rt_dev = (char*)(*args++);
	    if (*args)
		return usage();	/* must be last to catch typos */
	} else
	    return usage();
    }

#if HAVE_RTF_REJECT
    if ((rt.rt_flags & RTF_REJECT) && !rt.rt_dev)
	rt.rt_dev = "lo";
#endif

    /* sanity checks.. */
    if (mask_in_addr(rt)) {
	__u32 mask = ~ntohl(mask_in_addr(rt));
	if ((rt.rt_flags & RTF_HOST) && mask != 0xffffffff) {
	    fprintf(stderr, "route: netmask %.8x doesn't make sense with host route\n", mask);
	    return (E_OPTERR);
	}
	if (mask & (mask + 1)) {
	    fprintf(stderr, "route: bogus netmask %s\n"), netmask;
	    return (E_OPTERR);
	}
	mask = ((struct sockaddr_in *) &rt.rt_dst)->sin_addr.s_addr;
	if (mask & ~mask_in_addr(rt)) {
	    fprintf(stderr, "route: netmask doesn't match route address\n");
	    return (E_OPTERR);
	}
    }
    /* Fill out netmask if still unset */
    if ((action == RTACTION_ADD) && rt.rt_flags & RTF_HOST)
	mask_in_addr(rt) = 0xffffffff;

    /* Create a socket to the INET kernel. */
    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	perror("socket");
	return (E_SOCK);
    }
    /* Tell the kernel to accept this route. */
    if (action == RTACTION_DEL) {
	if (ioctl(skfd, SIOCDELRT, &rt) < 0) {
	    perror("SIOCDELRT");
	    return (E_SOCK);
	}
    } else {
	if (ioctl(skfd, SIOCADDRT, &rt) < 0) {
	    perror("SIOCADDRT");
	    return (E_SOCK);
	}
    }

    /* Close the socket. */
    return (0);
}


int nt_route_set(int action, int options,const char **args)
{
    int skfd;
    int ret;

	if ((skfd = nt_sockets_open(0)) < 0) {
		perror("socket");
		return -1;
	}
    ret = INET_setroute(skfd,action,options,args);
    nt_safe_close(skfd);
    return ret;
}

}
