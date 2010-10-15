#ifndef UDPCAST_H
#define UDPCAST_H

#include <sys/time.h>
#include <sys/socket.h>
#include <termios.h>

#define BITS_PER_INT (sizeof(int) * 8)
#define BITS_PER_CHAR 8


#define MAP_ZERO(l, map) (bzero(map, ((l) + BITS_PER_INT - 1)/ BIT_PER_INT))
#define BZERO(data) (bzero((void *)&data, sizeof(data)))


#define RDATABUFSIZE (2*(MAX_SLICE_SIZE + 1)* MAX_BLOCK_SIZE)

#define DATABUFSIZE (RDATABUFSIZE + 4096 - RDATABUFSIZE % 4096)

#define writeSize udpc_writeSize
#define largeReadSize udpc_largeReadSize
#define smallReadSize udpc_smallReadSize
#define makeDataBuffer udpc_makeDataBuffer
#define parseCommand udpc_parseCommand
#define printLongNum udpc_printLongNum
#define waitForProcess udpc_waitForProcess
#define printProcessStatus udpc_printProcessStatus
#define swapl udpc_swapl
#define swaps udpc_swaps

#define setRaw udpc_setRaw
#define restoreTerm udpc_restoreTerm

int writeSize(void);
int largeReadSize(void);
int smallReadSize(void);
int makeDataBuffer(int blocksize);
int parseCommand(char *pipeName, char **arg);

int printLongNum(unsigned long long x);
int waitForProcess(int pid, char *message);
int printProcessStatus(char *message, int status);
unsigned int swapl(int pc);
unsigned short swaps(short pc);

void setRaw(int keyboardFd, struct termios *oldtio);
void restoreTerm(int keyboardFd, struct termios *oldtio);

#define pctohl(x) ntohl(swapl(x))
#define htopcl(x) swapl(htonl(x))

#define pctohs(x) ntohs(swaps(x))
#define htopcs(x) swaps(htons(x))


#define xtohl(x) (endianness == PC_ENDIAN ? pctohl(x) : ntohl(x))
#define htoxl(x) (endianness == PC_ENDIAN ? htopcl(x) : htonl(x))

#define xtohs(x) (endianness == PC_ENDIAN ? pctohs(x) : ntohs(x))
#define htoxs(x) (endianness == PC_ENDIAN ? htopcs(x) : htons(x))


struct disk_config {
    int origOutFile;
    char *fileName;
    char *pipeName;
    int flags;
};

struct net_config {
    char *net_if; /* Network interface (eth0, isdn0, etc.) on which to
		   * multicast */
    int portBase; /* Port base */
    int blockSize;
    int sliceSize;
    struct sockaddr controlMcastAddr;
    struct sockaddr dataMcastAddr;
    char *mcastAll;
    int ttl;
    struct rate_limit *rateLimit;
    /*int async;*/
    /*int pointopoint;*/
    int dir; /* 1 if TIOCOUTQ is remaining space, 
	      * 0 if TIOCOUTQ is consumed space */
    int sendbuf; /* sendbuf */
    struct timeval ref_tv;

    enum discovery {
	DSC_DOUBLING,
	DSC_REDUCING
    } discovery;

    /* int autoRate; do queue watching using TIOCOUTQ, to avoid overruns */

    int flags; /* non-capability command line flags */
    int capabilities;

    int min_slice_size;
    int default_slice_size;
    int max_slice_size;
    unsigned int rcvbuf;

    int rexmit_hello_interval; /* retransmission interval between hello's.
				* If 0, hello message won't be retransmitted
				*/
    int autostart; /* autostart after that many retransmits */

    int requestedBufSize; /* requested receiver buffer */

    /* sender-specific parameters */
    int min_clients;
    int max_client_wait;
    int min_client_wait;

    int retriesUntilDrop;

    /* receiver-specif parameters */
    int exitWait; /* How many milliseconds to wait on program exit */

    /* FEC config */
#ifdef BB_FEATURE_UDPCAST_FEC
    int fec_redundancy; /* how much fec blocks are added per group */
    int fec_stripesize; /* size of FEC group */
    int fec_stripes; /* number of FEC stripes per slice */
#endif
};

#define MAX_SLICE_SIZE 1024

#endif
