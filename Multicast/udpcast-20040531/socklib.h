#ifndef SOCKLIB_H
#define SOCKLIB_H

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#define RECEIVER_PORT(x) (x)
#define SENDER_PORT(x) ((x)+1)

#define loseSendPacket udpc_loseSendPacket
#define loseRecvPacket udpc_loseRecvPacket
#define setWriteLoss udpc_setWriteLoss
#define setReadLoss udpc_setReadLoss
#define setReadSwap udpc_setReadSwap
#define srandomTime udpc_srandomTime
#define RecvMsg udpc_RecvMsg
#define doAutoRateLimit udpc_doAutoRateLimit
#define makeSockAddr udpc_makeSockAddr
#define getMyAddress udpc_getMyAddress
#define getBroadCastAddress udpc_getBroadCastAddress
#define getMcastAllAddress udpc_getMcastAllAddress
#define doSend udpc_doSend
#define doReceive udpc_doReceive
#define printMyIp udpc_printMyIp
#define makeSocket udpc_makeSocket
#define setSocketToBroadcast udpc_setSocketToBroadcast
#define setTtl udpc_setTtl
#define mcastListen udpc_mcastListen
#define setMcastDestination udpc_setMcastDestination
#define getCurrentQueueLength udpc_getCurrentQueueLength
#define getSendBuf udpc_getSendBuf
#define setSendBuf udpc_setSendBuf
#define getRcvBuf udpc_getRcvBuf
#define setRcvBuf udpc_setRcvBuf
#define getPort udpc_getPort
#define setPort udpc_setPort
#define getIpString udpc_getIpString
#define ipIsEqual udpc_ipIsEqual
#define ipIsZero udpc_ipIsZero
#define clearIp udpc_clearIp
#define setIpFromString udpc_setIpFromString
#define copyIpFrom udpc_copyIpFrom
#define getDefaultMcastAddress udpc_getDefaultMcastAddress
#define copyToMessage udpc_copyToMessage
#define copyFromMessage udpc_copyFromMessage
#define isAddressEqual udpc_isAddressEqual
#define parseSize udpc_parseSize
#define parseSpeed udpc_parseSpeed

#ifdef LOSSTEST
int loseSendPacket(void);
void loseRecvPacket(int s);
void setWriteLoss(char *l);
void setReadLoss(char *l);
void setReadSwap(char *l);
void srandomTime(int printSeed);
int RecvMsg(int s, struct msghdr *msg, int flags);
#endif


void doAutoRateLimit(int sock, int dir, int qsize, int size);

int makeSockAddr(char *hostname, short port, struct sockaddr *addr);

int getMyAddress(int s, char *ifname, struct sockaddr *addr);
int getBroadCastAddress(int s, char *ifname, struct sockaddr *addr, 
			short port);
int getMcastAllAddress(int s, char *ifname, struct sockaddr *addr, 
		       char *address, short port);


int doSend(int s, void *message, size_t len, struct sockaddr *to);
int doReceive(int s, void *message, size_t len,
	      struct sockaddr *from, int portBase);

void printMyIp(char *ifname, int s);


int makeSocket(char **ifnamep, int port);

int setSocketToBroadcast(int sock);
int setTtl(int sock, int ttl);

int mcastListen(int, char *,struct sockaddr *);
int setMcastDestination(int,char *,struct sockaddr *);

int getCurrentQueueLength(int sock);
int getSendBuf(int sock);
void setSendBuf(int sock, unsigned int bufsize);
unsigned int getRcvBuf(int sock);
void setRcvBuf(int sock, unsigned int bufsize);


#define SEND(s, msg, to) \
	doSend(s, &msg, sizeof(msg), &to)

#define RECV(s, msg, from, portBase ) \
	doReceive((s), &msg, sizeof(msg), &from, (portBase) )

#define BCAST_CONTROL(s, msg) \
	doSend(s, &msg, sizeof(msg), &net_config->controlMcastAddr)

unsigned short getPort(struct sockaddr *addr);
void setPort(struct sockaddr *addr, unsigned short port);
char *getIpString(struct sockaddr *addr, char *buffer);
int ipIsEqual(struct sockaddr *left, struct sockaddr *right);
int ipIsZero(struct sockaddr *ip);

void clearIp(struct sockaddr *addr);
void setIpFromString(struct sockaddr *addr, char *ip);

void copyIpFrom(struct sockaddr *dst, struct sockaddr *src);

void getDefaultMcastAddress(int sock, char *net_if, struct sockaddr *mcast);

void copyToMessage(unsigned char *dst, struct sockaddr *src);
void copyFromMessage(struct sockaddr *dst, unsigned char *src);

int isAddressEqual(struct sockaddr *a, struct sockaddr *b);

unsigned long parseSize(char *sizeString);
unsigned long parseSpeed(char *speedString);
#endif
