#include <sys/types.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <net/if.h>
#include <sys/ioctl.h>

#include "log.h"
#include "socklib.h"

#ifdef LOSSTEST
/**
 * Packet loss/swap testing...
 */
long int write_loss = 0;
long int read_loss = 0;
long int read_swap = 0;
unsigned int seed=0;

int loseSendPacket(void) {
    if(write_loss) {
	long r = random();
	if(r < write_loss)
	    return 1;
    }
    return 0;
}

#define STASH_SIZE 64

static int stashed;
static struct packetStash {
    unsigned char data[4092];
    int size;
} packetStash[STASH_SIZE];

/**
 * Lose a packet
 */
void loseRecvPacket(int s) {
    if(read_loss) {
	while(random() < read_loss) {
	    int x;
	    flprintf("Losing packet\n");
	    recv(s, (void *) &x, sizeof(x),0);
	}
    }
    if(read_swap) {
	while(stashed < STASH_SIZE && random() < read_swap) {
	    int size;
	    flprintf("Stashing packet %d\n", stashed);
	    size = recv(s, packetStash[stashed].data, 
			sizeof(packetStash[stashed].data),0);
	    packetStash[stashed].size = size;
	    stashed++;
	}
    }
}

/**
 * bring stored packet back up...
 */
int RecvMsg(int s, struct msghdr *msg, int flags) {
    if(read_swap && stashed) {
	if(random() / stashed < read_swap) {
	    int slot = random() % stashed;
	    int iovnr;
	    char *data = packetStash[slot].data;
	    int totalLen = packetStash[slot].size;
	    int retBytes=0;
	    flprintf("Bringing out %d\n", slot);
	    for(iovnr=0; iovnr < msg->msg_iovlen; iovnr++) {
		int len = msg->msg_iov[iovnr].iov_len;
		if(len > totalLen)
		    len = totalLen;
		bcopy(data, msg->msg_iov[iovnr].iov_base, len);
		totalLen -= len;
		data += len;
		retBytes += len;
		if(totalLen == 0)
		    break;
	    }
	    packetStash[slot]=packetStash[stashed];
	    stashed--;
	    return retBytes;
	}
    }
    return recvmsg(s, msg, flags);
}

void setWriteLoss(char *l) {
    write_loss = (long) (atof(l) * RAND_MAX);
}

void setReadLoss(char *l) {
    read_loss = (long) (atof(l) * RAND_MAX);
}

void setReadSwap(char *l) {
    read_swap = (long) (atof(l) * RAND_MAX);
}


void srandomTime(int printSeed) {
    struct timeval tv;
    long seed;
    gettimeofday(&tv, 0);
    seed = (tv.tv_usec * 2000) ^ tv.tv_sec;
    if(printSeed)
	flprintf("seed=%ld\n", seed);
    srandom(seed);
}
#endif

/**
 * If queue gets almost full, slow down things
 */
void doAutoRateLimit(int sock, int dir, int qsize, int size)
{
    while(1) {
	int r = udpc_getCurrentQueueLength(sock);
	if(dir)
	    r = qsize - r;
	if(r < qsize / 2 - size)
	    return;
#if DEBUG
	flprintf("Queue full %d/%d... Waiting\n", r, qsize);
#endif
	usleep(2500);
    }
}


/* makes a socket address */
int makeSockAddr(char *hostname, short port, struct sockaddr *addr)
{
    struct hostent *host;

    bzero ((char *) addr, sizeof(struct sockaddr_in));
    if (hostname && *hostname) {
	char *inaddr;
	int len;

	host = gethostbyname(hostname);
	if (host == NULL) {
	    udpc_fatal(1, "Unknown host %s\n", hostname);
	}
	
	inaddr = host->h_addr_list[0];
	len = host->h_length;
	bcopy(inaddr, (void *)&((struct sockaddr_in *)addr)->sin_addr, len);
    }

    ((struct sockaddr_in *)addr)->sin_family = AF_INET;
    ((struct sockaddr_in *)addr)->sin_port = htons(port);
    return 0;
}



int getMyAddress(int s, char *ifname, struct sockaddr *addr) {
    struct ifreq ifc;
    
    strcpy(ifc.ifr_name, ifname);
    if(ioctl(s,  SIOCGIFADDR, &ifc) < 0) {
	perror("ifconfig");
	exit(1);
    }
    *addr = ifc.ifr_addr;
    return 0;
}


int getBroadCastAddress(int s, char *ifname, struct sockaddr *addr, 
			short port){
    struct ifreq ifc;

    strcpy(ifc.ifr_name, ifname);
    if(ioctl(s,  SIOCGIFBRDADDR, &ifc) < 0) {
	perror("ifconfig");
	exit(1);
    }
    ((struct sockaddr_in *) &ifc.ifr_ifru.ifru_broadaddr)->sin_port = 
	htons(port);
    if (addr)
	bcopy((void*)&ifc.ifr_broadaddr, (void*)addr, 
	      sizeof(struct sockaddr_in));
    return 0;
}

int getMcastAllAddress(int s, char *ifname, struct sockaddr *addr, 
		       char *address, short port){
    ((struct sockaddr_in *) addr)->sin_family = AF_INET;
    ((struct sockaddr_in *) addr)->sin_port = htons(port);
    if(address == NULL || address[0] == '\0')
	inet_aton("224.0.0.1", &((struct sockaddr_in *) addr)->sin_addr);
    else {
	inet_aton(address, &((struct sockaddr_in *) addr)->sin_addr);
	udpc_mcastListen(s, ifname, addr);
    }	
    return 0;
}


int doSend(int s, void *message, size_t len, struct sockaddr *to) {
/*    flprintf("sent: %08x %d\n", *(int*) message, len);*/
#ifdef LOSSTEST
    loseSendPacket();
#endif
    return sendto(s, message, len, 0, to, sizeof(*to));
}

int doReceive(int s, void *message, size_t len, 
	      struct sockaddr *from, int portBase) {
    socklen_t slen;
    int r;
    short port;
    char ipBuffer[16];

    slen = sizeof(*from);
#ifdef LOSSTEST
    loseRecvPacket(s);
#endif
    r = recvfrom(s, message, len, 0, from, &slen);
    if (r < 0)
	return r;
    port = ntohs(((struct sockaddr_in *)from)->sin_port);
    if(port != RECEIVER_PORT(portBase) && port != SENDER_PORT(portBase)) {
	udpc_flprintf("Bad message from port %s.%d\n", 
		      getIpString(from, ipBuffer),
		      ntohs(((struct sockaddr_in *)from)->sin_port));
	return -1;
    }
/*    flprintf("recv: %08x %d\n", *(int*) message, r);*/
    return r;
}

int getSendBuf(int sock) {
    int bufsize;
    int len=sizeof(int);
    if(getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufsize, &len) < 0)
	return -1;
    return bufsize;
}

void setSendBuf(int sock, unsigned int bufsize) {
    if(setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize))< 0)
	perror("Set send buffer");
}

unsigned int getRcvBuf(int sock) {
    unsigned int bufsize;
    int len=sizeof(int);
    if(getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, &len) < 0)
	return -1;
    return bufsize;
}

void setRcvBuf(int sock, unsigned int bufsize) {
    if(setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize))< 0)
	perror("Set receiver buffer");
}

int getCurrentQueueLength(int sock) {
#ifdef TIOCOUTQ
    int length;
    if(ioctl(sock, TIOCOUTQ, &length) < 0)
	return -1;
    return length;
#else
    return -1;
#endif
}


int setSocketToBroadcast(int sock) {
    /* set the socket to broadcast */
    int p = 1;
    return setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &p, sizeof(int));
}

int setTtl(int sock, int ttl) {
    /* set the socket to broadcast */
    return setsockopt(sock, SOL_IP, IP_MULTICAST_TTL, &ttl, sizeof(int));
}

#ifdef SIOCGIFINDEX
# define IP_MREQN ip_mreqn
#else
# define IP_MREQN ip_mreq
#endif

#define getSinAddr(addr) (((struct sockaddr_in *) addr)->sin_addr)

/**
 * Fill in the mreq structure with the given interface and address
 */
static int fillMreq(int sock, char *ifname, struct in_addr addr,
		    struct IP_MREQN *mreq) {
#ifdef SIOCGIFINDEX
    /* first determine if_index */
    struct ifreq ifc;
    strcpy(ifc.ifr_ifrn.ifrn_name, ifname);
    if(ioctl(sock,  SIOCGIFINDEX, &ifc) < 0) {
	perror("get ifindex");
	exit(1);
    }
    mreq->imr_ifindex = ifc.ifr_ifindex;
    mreq->imr_address.s_addr = 0;
#else
    struct sockaddr if_addr;
    getMyAddress(sock, ifname, &if_addr);
    mreq->imr_interface = getSinAddr(&if_addr);
#endif
    mreq->imr_multiaddr = addr;

    return 0;
}

/**
 * Perform a multicast operation
 */
static int mcastOp(int sock, char *ifname, struct in_addr addr,
		   int code, char *message) {
    struct IP_MREQN mreq;
    int r;
    
    fillMreq(sock, ifname, addr, &mreq);
    r = setsockopt(sock, SOL_IP, code, &mreq, sizeof(mreq));
    if(r < 0) {
	perror(message);
	exit(1);
    }
    return 0;
}


/*
struct in_addr getSinAddr(struct sockaddr *addr) {
    return ((struct sockaddr_in *) addr)->sin_addr;
}
*/

/**
 * Set socket to listen on given multicast address Not 100% clean, it
 * would be preferable to make a new socket, and not only subscribe it
 * to the multicast address but also _bind_ to it. Indeed, subscribing
 * alone is not enough, as we may get traffic destined to multicast
 * address subscribed to by other apps on the machine. However, for
 * the moment, we skip this concern, as udpcast's main usage is
 * software installation, and in that case it runs on an otherwise
 * quiet system.
 */
int mcastListen(int sock, char *ifname, struct sockaddr *addr) {
    return mcastOp(sock, ifname, getSinAddr(addr), IP_ADD_MEMBERSHIP,
		   "Subscribe to multicast group");
}


int setMcastDestination(int sock, char *ifname, struct sockaddr *addr) {
#ifdef __CYGWIN__
    int r;
    struct sockaddr interface_addr;
    struct in_addr if_addr;
    getMyAddress(sock, ifname, &interface_addr);
    if_addr = getSinAddr(&interface_addr);
    r = setsockopt (sock, IPPROTO_IP, IP_MULTICAST_IF, 
		&if_addr, sizeof(if_addr));
    if(r < 0)
	fatal(1, "Set multicast send interface");
    return 0;
#else
    /* IP_MULTICAST_IF not correctly supported on Cygwin */
    return mcastOp(sock, ifname, getSinAddr(addr), IP_MULTICAST_IF,
		   "Set multicast send interface");
#endif
}

/**
 * Canonize interface name. If attempt is not NULL, pick the interface
 * which has that address.
 * If attempt is NULL, pick interfaces int the following order of preference
 * 1. eth0
 * 2. Anything starting with eth0:
 * 3. Anything starting with eth
 * 4. Anything else
 * 5. localhost
 * 6. zero address
 */
static char *canonizeIfName(int s, char *wanted) {
	struct ifreq ibuf[100];
	struct ifreq *ifrp, *ifend, *chosen;
	struct ifconf ifc;
	int lastGoodness;
	struct in_addr wantedAddress;

	ifc.ifc_len = sizeof(ibuf);
	ifc.ifc_buf = (caddr_t) ibuf;

	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0 ||
            ifc.ifc_len < (signed int)sizeof(struct ifreq)) {
                perror("tcpdump: SIOCGIFCONF: ");
                exit(1);
        }

        ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);
	lastGoodness=0;
        chosen=NULL;

	if(wanted != NULL)
	    inet_aton(wanted, &wantedAddress);

        for (ifrp = ibuf ; ifrp < ifend; ifrp++) {
	    unsigned long iaddr = getSinAddr(&ifrp->ifr_addr).s_addr;
	    if(wanted != NULL) {
		if(iaddr == wantedAddress.s_addr) {
		    chosen = ifrp;
		    break;
		}
	    } else {
		int goodness;
		if(iaddr == 0) {
		    /* disregard interfaces whose address is zero */
		    goodness = 1;
		} else if(iaddr == htonl(0x7f000001)) {
		    /* disregard localhost type devices */
		    goodness = 2;
		} else if(strcmp("eth0", ifrp->ifr_name) == 0) {
		    /* prefer eth0 */
		    goodness = 6;
		} else if(strncmp("eth0:", ifrp->ifr_name, 5) == 0) {
		    /* second choice: any secondary interfaces on eth0 */
		    goodness = 5;
		} else if(strncmp("eth", ifrp->ifr_name, 3) == 0) {
		    /* and, if not available, any other ethernet device */
		    goodness = 4;
		} else {
		    goodness = 3;
		}
		if(goodness > lastGoodness) {
		    chosen = ifrp;
		    lastGoodness = goodness;
		}
	    }
	}


	if(!chosen) {
	    fprintf(stderr, "No suitable network interface found\n");
	    fprintf(stderr, "The following interfaces are available:\n");

	    for (ifrp = ibuf ; ifrp < ifend; ifrp++) {
		char buffer[16];
		fprintf(stderr, "\t%s\t%s\n",
			ifrp->ifr_name, 
			udpc_getIpString(&ifrp->ifr_addr, buffer));
	    }
	    exit(1);
	}
	return strdup(chosen->ifr_name);
}

int makeSocket(char **ifnamep, int port) {
    int ret, s;
    struct sockaddr myaddr;
    char *ifname = *ifnamep;

    s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	perror("make socket");
	exit(1);
    }

    if(ifname == NULL) {
	ifname = getenv("IFNAME");
    }

    /* canonize interface name */
    if(ifname == NULL || (ifname[0] >= '0' && ifname[0] <= '9')) {
	ifname = canonizeIfName(s, ifname);
    }

    *ifnamep = ifname;

#ifdef __CYGWIN__
    getMyAddress(s, ifname, &myaddr);    
    ((struct sockaddr_in *)&myaddr)->sin_port = htons(port);
#else
    makeSockAddr(NULL, port, &myaddr);
#endif
    ret = bind(s, (struct sockaddr *) &myaddr, sizeof(myaddr));
    if (ret < 0) {
	char buffer[16];
	udpc_fatal(1, "bind socket to %s:%d (%s)\n",
		   udpc_getIpString(&myaddr, buffer), 
		   udpc_getPort(&myaddr),
		   strerror(errno));
    }

    return s;
}

void printMyIp(char *ifname, int s)
{
    char buffer[16];
    struct sockaddr myaddr;
/*    struct sockaddr_in bcastaddr;*/

    getMyAddress(s, ifname, &myaddr);
    udpc_flprintf(udpc_getIpString(&myaddr,buffer));
}

char *udpc_getIpString(struct sockaddr *addr, char *buffer) {
    long iaddr = getSinAddr(addr).s_addr;
    sprintf(buffer,"%ld.%ld.%ld.%ld", 
	    iaddr & 0xff,
	    (iaddr >>  8) & 0xff,
	    (iaddr >> 16) & 0xff,
	    (iaddr >> 24) & 0xff);
    return buffer;
}

int ipIsEqual(struct sockaddr *left, struct sockaddr *right) {
    return getSinAddr(left).s_addr == getSinAddr(right).s_addr;
}

int ipIsZero(struct sockaddr *ip) {
    return getSinAddr(ip).s_addr == 0;
}

unsigned short udpc_getPort(struct sockaddr *addr) {
    return ntohs(((struct sockaddr_in *) addr)->sin_port);
}

void setPort(struct sockaddr *addr, unsigned short port) {
    ((struct sockaddr_in *) addr)->sin_port = htons(port);
}


void clearIp(struct sockaddr *addr) {
    ((struct sockaddr_in *) addr)->sin_addr.s_addr = 0;
    ((struct sockaddr_in *)&addr)->sin_family = AF_INET;
}

void setIpFromString(struct sockaddr *addr, char *ip) {
    inet_aton(ip, &((struct sockaddr_in *)addr)->sin_addr);
    ((struct sockaddr_in *)addr)->sin_family = AF_INET;
}

void copyIpFrom(struct sockaddr *dst, struct sockaddr *src) {
    ((struct sockaddr_in *)dst)->sin_addr = 
	((struct sockaddr_in *)src)->sin_addr;
    ((struct sockaddr_in *)dst)->sin_family = 
	((struct sockaddr_in *)src)->sin_family;
}

void getDefaultMcastAddress(int sock, char *net_if, struct sockaddr *mcast) {
    struct sockaddr_in *addr = (struct sockaddr_in *) mcast;
    getMyAddress(sock, net_if, mcast);
    addr->sin_addr.s_addr &= htonl(0x07ffffff);
    addr->sin_addr.s_addr |= htonl(0xe8000000);
}


void copyToMessage(unsigned char *dst, struct sockaddr *src) {    
    memcpy(dst, (char *) &((struct sockaddr_in *)src)->sin_addr,
	   sizeof(struct in_addr));
}


void copyFromMessage(struct sockaddr *dst, unsigned char *src) {
    memcpy((char *) &((struct sockaddr_in *)dst)->sin_addr, src,
	   sizeof(struct in_addr));
}

int isAddressEqual(struct sockaddr *a, struct sockaddr *b) {
    return !memcmp((char *) a, (char *)b, 8);
}

unsigned long parseSize(char *sizeString) {
    char *eptr;
    unsigned long size = strtoul(sizeString, &eptr, 10);
    if(eptr && *eptr) {
	switch(*eptr) {
	    case 'm':
	    case 'M':
		size *= 1024 * 1024;
		break;
	    case 'k':
	    case 'K':
		size *= 1024;
		break;
	    case '\0':
		break;
	    default:
		udpc_fatal(1, "Unit %c unsupported\n", *eptr);
	}
    }
    return size;
}


unsigned long parseSpeed(char *speedString) {
    char *eptr;
    unsigned long speed = strtoul(speedString, &eptr, 10);
    if(eptr && *eptr) {
	switch(*eptr) {
	    case 'm':
	    case 'M':
		speed *= 1000000;
		break;
	    case 'k':
	    case 'K':
		speed *= 1000;
		break;
	    case '\0':
		break;
	    default:
		udpc_fatal(1, "Unit %c unsupported\n", *eptr);
	}
    }
    return speed;
}
