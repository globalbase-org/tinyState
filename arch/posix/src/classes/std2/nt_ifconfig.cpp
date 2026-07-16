

#include	"std2/includes.h"
#include	"std2/nt.h"
#include	"std2/safe_strncpy.h"

namespace ts2nif {

int nt_sockets_open(int family)
{
	if ( family == 0 )
		return socket(AF_INET,SOCK_DGRAM,0);
	return  socket(family, SOCK_DGRAM, 0);
}

int nt_safe_close(int s)
{
int er;
	if ( s < 0 )
		return -1;
	for ( ; ; ) {
		er = close(s);
		if ( er == 0 )
			break;
		if ( errno == EINTR )
			continue;
		return er;
	}
	return 0;
}

/* Set a certain interface flag. */
static int _set_flag(int skfd,const char *ifname, short flag)
{
    struct ifreq ifr;

    /* Create a channel to the NET kernel. */
    ts2nif::safe_strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0) {
	fprintf(stderr, "%s: ERROR while getting interface flags: %s\n", 
		ifname,	strerror(errno));
	return (-1);
    }
    ts2nif::safe_strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags |= flag;
    if (ioctl(skfd, SIOCSIFFLAGS, &ifr) < 0) {
	perror("SIOCSIFFLAGS");
	return -1;
    }
    return (0);
}

/* Clear a certain interface flag. */
static int _clr_flag(int skfd,const char *ifname, short flag)
{
    struct ifreq ifr;
    int fd;

    fd = skfd;

    ts2nif::safe_strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
	fprintf(stderr, "%s: ERROR while getting interface flags: %s\n", 
		ifname, strerror(errno));
	return -1;
    }
    ts2nif::safe_strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    ifr.ifr_flags &= ~flag;
    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0) {
	perror("SIOCSIFFLAGS");
	return -1;
    }
    return (0);
}

/** test is a specified flag is set */
static int _test_flag(int skfd,const char *ifname, short flags)
{
    struct ifreq ifr;
    int fd;

    fd = skfd;

    ts2nif::safe_strncpy(ifr.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
	fprintf(stderr, "%s: ERROR while testing interface flags: %s\n", 
		ifname, strerror(errno));
	return -1;
    }
    return (ifr.ifr_flags & flags);
}

int nt_ifconfig_test_flag(const char *ifname, short flags)
{
int ret;
int skfd;

printf("IFCONFIG-TEST\n");
    /* Create a channel to the NET kernel. */
	if ((skfd = nt_sockets_open(0)) < 0) {
		perror("socket");
		return -1;
	}
    	ret = _test_flag(skfd,ifname,flags);
	nt_safe_close(skfd);
	return ret;
}

int nt_ifconfig_set_flag(const char *ifname, short flag)
{
int ret;
int skfd;

printf("IFCONFIG-SET\n");
/* Create a channel to the NET kernel. */
	if ((skfd = nt_sockets_open(0)) < 0) {
		perror("socket");
		return -1;
	}
    	ret = _set_flag(skfd,ifname,flag);
	nt_safe_close(skfd);
	return ret;
}


int nt_ifconfig_clr_flag(const char *ifname, short flag)
{
int ret;
int skfd;

printf("IFCONFIG-CLR\n");
/* Create a channel to the NET kernel. */
	if ((skfd = nt_sockets_open(0)) < 0) {
		perror("socket");
		return -1;
	}
    	ret = _clr_flag(skfd,ifname,flag);
	nt_safe_close(skfd);
	return ret;
}


static int _if_fetch(int skfd,struct nt_interface *ife)
{
    struct ifreq ifr;
    int fd;
    char *ifname = ife->name; 
    int ret;

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(skfd, SIOCGIFFLAGS, &ifr) < 0)
	return (-1);
    ife->flags = ifr.ifr_flags;

    ret = ts2nif::_if_fetch_m(skfd,ife);
    if ( ret < 0 )
        return ret;
      

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(skfd, SIOCGIFMETRIC, &ifr) < 0)
	ife->metric = 0;
    else
	ife->metric = ifr.ifr_metric;

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(skfd, SIOCGIFMTU, &ifr) < 0)
	ife->mtu = 0;
    else
	ife->mtu = ifr.ifr_mtu;

    /*
    strcpy(ifr.ifr_name, ifname);
    if (ioctl(skfd, SIOCGIFMAP, &ifr) < 0)
	memset(&ife->map, 0, sizeof(struct ifmap));
    else
	memcpy(&ife->map, &ifr.ifr_map, sizeof(struct ifmap));

    strcpy(ifr.ifr_name, ifname);
    if (ioctl(skfd, SIOCGIFMAP, &ifr) < 0)
	memset(&ife->map, 0, sizeof(struct ifmap));
    else
	ife->map = ifr.ifr_map;
    */

    /* IPv4 address? */
//    fd = get_socket_for_af(AF_INET);
    fd = skfd;
    if (fd >= 0) {
	strcpy(ifr.ifr_name, ifname);
	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
	    ife->has_ip = 1;
	    ife->addr = ifr.ifr_addr;
	    strcpy(ifr.ifr_name, ifname);
	    if (ioctl(fd, SIOCGIFDSTADDR, &ifr) < 0)
	        memset(&ife->dstaddr, 0, sizeof(struct sockaddr));
	    else
	        ife->dstaddr = ifr.ifr_dstaddr;

	    strcpy(ifr.ifr_name, ifname);
	    if (ioctl(fd, SIOCGIFBRDADDR, &ifr) < 0)
	        memset(&ife->broadaddr, 0, sizeof(struct sockaddr));
	    else
		ife->broadaddr = ifr.ifr_broadaddr;
	    strcpy(ifr.ifr_name, ifname);
	    if (ioctl(fd, SIOCGIFNETMASK, &ifr) < 0)
		memset(&ife->netmask, 0, sizeof(struct sockaddr));
	    else {
		ife->netmask = ifr.ifr_netmask;
	    }
	 } else
	    memset(&ife->addr, 0, sizeof(struct sockaddr));
    }


    return 0;
}

int nt_ifconfig_fetch(struct nt_interface *ife)
{
    int skfd;
    int ret;

	if ((skfd = nt_sockets_open(0)) < 0) {
		perror("socket");
		return -1;
	}
    ret = _if_fetch(skfd,ife);
    nt_safe_close(skfd);
    return ret;
}


}
