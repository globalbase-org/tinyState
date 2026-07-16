

#include <sys/sysctl.h>
#include <net/if_dl.h>
#include	"std2/nt.h"


namespace ts2nif {

int _if_fetch_m(int skfd,struct nt_interface *ife)
{
  	int			mib[6];
	size_t			len;
	char			*buf;
	unsigned char		*ptr;
	struct if_msghdr	*ifm;
	struct sockaddr_dl	*sdl;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_LINK;
	mib[4] = NET_RT_IFLIST;
	if ((mib[5] = if_nametoindex(ife->name)) == 0) {
		perror("if_nametoindex error");
		return -1;
	}

	if (sysctl(mib, 6, NULL, &len, NULL, 0) < 0) {
		perror("sysctl 1 error");
		return -1;
	}

	if ((buf = (char*)malloc(len)) == NULL) {
		perror("malloc error");
		return -1;
	}

	if (sysctl(mib, 6, buf, &len, NULL, 0) < 0) {
		perror("sysctl 2 error");
		return -1;
	}

	ifm = (struct if_msghdr *)buf;
	sdl = (struct sockaddr_dl *)(ifm + 1);
	ptr = (unsigned char *)LLADDR(sdl);
	memcpy(ife->hwaddr,ptr,6);
	return 0;
}

}
