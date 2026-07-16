

#include	"std2/nt.h"


namespace ts2nif {

int _if_fetch_m(int skfd,struct nt_interface *ife)
{
    struct ifreq ifr;
	strcpy(ifr.ifr_name, ife->name);
	if (ioctl(skfd, SIOCGIFHWADDR, &ifr) < 0)
		memset(ife->hwaddr, 0, 32);
    	else
		memcpy(ife->hwaddr, ifr.ifr_hwaddr.sa_data, 8);
	ife->type = ifr.ifr_hwaddr.sa_family;
	return 0;
}

}
