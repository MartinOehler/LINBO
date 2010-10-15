#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "log.h"
#include "fifo.h"
#include "socklib.h"
#include "udpcast.h"
#include "udpc-protoc.h"
#include "udp-sender.h"
#include "rate-limit.h"
#include "participants.h"
#include "statistics.h"

/**
 * This file contains the code to set up the connection
 */

static int doTransfer(int sock, 
		      participantsDb_t db,
		      struct disk_config *disk_config,
		      struct net_config *net_config);


static int isPointToPoint(participantsDb_t db, int flags) {
    if(flags & FLAG_POINTOPOINT)
	return 1;
    if(flags & (FLAG_NOPOINTOPOINT | FLAG_ASYNC))
	return 0;
    return udpc_nrParticipants(db) == 1;
}


static int sendConnectionReply(participantsDb_t db,
			       int sock,
			       struct net_config *config,
			       struct sockaddr *client, 
			       int capabilities,
			       unsigned int rcvbuf) {
    struct connectReply reply;

    if(rcvbuf == 0)
	rcvbuf = 65536;

    if(capabilities & CAP_BIG_ENDIAN) {
	reply.opCode = htons(CMD_CONNECT_REPLY);
	reply.clNr = 
	    htonl(udpc_addParticipant(db,
				      client, 
				      capabilities,
				      rcvbuf,
				      config->flags & FLAG_POINTOPOINT));
	reply.blockSize = htonl(config->blockSize);
    } else if(capabilities & CAP_LITTLE_ENDIAN) {
	reply.opCode = htopcs(CMD_CONNECT_REPLY);
	reply.clNr = 
	    htopcl(udpc_addParticipant(db,
				       client, 
				       capabilities,
				       rcvbuf,
				       config->flags & FLAG_POINTOPOINT));
	reply.blockSize = htopcl(config->blockSize);
    }
    reply.reserved = 0;

    if(config->flags & FLAG_POINTOPOINT) {
	copyIpFrom(&config->dataMcastAddr, client);
    }

    /* new parameters: always big endian */
    reply.capabilities = ntohl(config->capabilities);
    copyToMessage(reply.mcastAddr,&config->dataMcastAddr);
    /*reply.mcastAddress = mcastAddress;*/
    doRateLimit(config->rateLimit, sizeof(reply));
    if(SEND(sock, reply, *client) < 0) {
	perror("reply add new client");
	return -1;
    }
    return 0;
}

static void sendHello(struct net_config *net_config, int sock) {
    struct hello hello;
    /* send hello message */
    hello.opCode = htopcs(CMD_HELLO);
    hello.reserved = 0;
    hello.capabilities = htonl(net_config->capabilities);
    copyToMessage(hello.mcastAddr,&net_config->dataMcastAddr);
    hello.blockSize = htons(net_config->blockSize);
    doRateLimit(net_config->rateLimit, sizeof(hello));
    BCAST_CONTROL(sock, hello);
}

/* Returns 1 if we should start because of clientWait, 0 otherwise */
static int checkClientWait(participantsDb_t db, 
			   struct net_config *net_config,
			   time_t *firstConnected)
{
    time_t now;
    if (!nrParticipants(db) || !firstConnected || !*firstConnected)
	return 0; /* do not start: no receivers */

    now = time(0);
    /*
     * If we have a max_client_wait, start the transfer after first client
     * connected + maxSendWait
     */
    if(net_config->max_client_wait &&
       (now >= *firstConnected + net_config->max_client_wait))
	return 1; /* send-wait passed: start */

    /*
     * Otherwise check to see if the minimum of clients
     *  have checked in.
     */
    else if (nrParticipants(db) >= net_config->min_clients &&
	/*
	 *  If there are enough clients and there's a min wait time, we'll
	 *  wait around anyway until then.
	 *  Otherwise, we always transfer
	 */
	(!net_config->min_client_wait || 
	 now >= *firstConnected + net_config->min_client_wait))
	    return 1;
    else
	return 0;
}

/* *****************************************************
 * Receive and process a localization enquiry by a client
 * Params:
 * fd		- file descriptor for network socket on which to receiver 
 *		client requests
 * db		- participant database
 * disk_config	- disk configuration
 * net_config	- network configuration
 * keyboardFd	- keyboard filedescriptor (-1 if keyboard inaccessible,
 *		or configured away)
 * tries	- how many hello messages have been sent?
 * firstConnected - when did the first client connect?
 */
static int mainDispatcher(int fd, 
			  participantsDb_t db,
			  struct disk_config *disk_config,
			  struct net_config *net_config,
			  int keyboardFd, int *tries,
			  time_t *firstConnected)
{
    struct sockaddr client;
    union message fromClient;
    fd_set read_set;
    int ret;
    int msgLength;
    int startNow=0;
    int maxFd=0;

    maxFd = keyboardFd;
    if(fd > maxFd)
	maxFd = fd;

    if ((udpc_nrParticipants(db) || (net_config->flags &  FLAG_ASYNC)) &&
	!(net_config->flags &  FLAG_NOKBD))
	udpc_flprintf("Ready. Press any key to start sending data.\n");
 
    if (firstConnected && !*firstConnected && udpc_nrParticipants(db))
	*firstConnected = time(0);

    while(!startNow) {
	struct timeval tv;
	struct timeval *tvp;
	int nr_desc;

	FD_ZERO(&read_set);
	if(keyboardFd >= 0){
	    FD_SET(keyboardFd, &read_set);
	}
	FD_SET(fd, &read_set);

	if(net_config->rexmit_hello_interval) {
	    tv.tv_usec = (net_config->rexmit_hello_interval % 1000)*1000;
	    tv.tv_sec = net_config->rexmit_hello_interval / 1000;
	    tvp = &tv;
	} else if(firstConnected && nrParticipants(db)) {
	    tv.tv_usec = 0;
	    tv.tv_sec = 2;
	    tvp = &tv;
	} else
	    tvp = 0;
	nr_desc = select(maxFd+1,  &read_set, 0, 0,  tvp);
	if(nr_desc < 0) {
	    perror("select");
	    return -1;
	}
	if(nr_desc > 0)
	    /* key pressed, or receiver activity */
	    break;

	if(net_config->rexmit_hello_interval) {
	    /* retransmit hello message */
	    sendHello(net_config, fd);
	    (*tries)++;
	    if(net_config->autostart != 0 && *tries > net_config->autostart)
		startNow=1;
	}
		  
	if(firstConnected)
	    startNow = 
		startNow || checkClientWait(db, net_config, firstConnected);
    }

    if(keyboardFd != -1 && FD_ISSET(keyboardFd, &read_set)) {
	char ch;
	read(keyboardFd, &ch, 1);
	if (ch == 'q') {
	    exit(0);
	}
	startNow = 1;
    }

    if(!FD_ISSET(fd, &read_set))
	return startNow;

    BZERO(fromClient); /* Zero it out in order to cope with short messages
			* from older versions */

    msgLength = RECV(fd, fromClient, client, net_config->portBase);
    if(msgLength < 0) {
	perror("problem getting data from client");
	return 0; /* don't panic if we get weird messages */
    }

    if(net_config->flags & FLAG_ASYNC)
	return 0;

    switch(ntohs(fromClient.opCode)) {
	case CMD_CONNECT_REQ:
	    sendConnectionReply(db, fd,
				net_config,
				&client, 
				CAP_BIG_ENDIAN |
				ntohl(fromClient.connectReq.capabilities),
				ntohl(fromClient.connectReq.rcvbuf));
	    return startNow;
	case CMD_GO:
	    return 1;
	case CMD_DISCONNECT:
	    ret = udpc_lookupParticipant(db, &client);
	    if (ret >= 0)
		udpc_removeParticipant(db, ret);
	    return startNow;
	default:
	    break;
    }


    switch(pctohs(fromClient.opCode)) {
	case CMD_CONNECT_REQ:
	    if(msgLength > 4) {
		/* Redundant connect req */
		return startNow;
	    }
	    sendConnectionReply(db, fd,	net_config, &client, 
				CAP_LITTLE_ENDIAN |
				ntohl(fromClient.connectReq.capabilities),
				ntohl(fromClient.connectReq.rcvbuf));
	    return startNow;
	case CMD_GO:
	    return 1;
	case CMD_DISCONNECT:
	    ret = udpc_lookupParticipant(db, &client);
	    if (ret >= 0)
		udpc_removeParticipant(db, ret);
	    return startNow;
	default:
	    break;
    }

    udpc_flprintf("Unexpected command %04x\n",
		  (unsigned short) fromClient.opCode);

    return startNow;
}

int startSender(struct disk_config *disk_config,
		struct net_config *net_config)
{
    struct termios oldtio;
    char ipBuffer[16];
    int tries;
    int r; /* return value for maindispatch. If 1, start transfer */
    time_t firstConnected = 0;
    time_t *firstConnectedP;
    int keyboardFd;

    participantsDb_t db;

    /* make the socket and print banner */
    int sock = makeSocket(&net_config->net_if,
			    SENDER_PORT(net_config->portBase));

    if(net_config->requestedBufSize)
	setSendBuf(sock, net_config->requestedBufSize);

#ifndef __CYGWIN__
    if(net_config->flags & FLAG_AUTORATE) {
	int q = getCurrentQueueLength(sock);
	if(q == 0) {
	    net_config->dir = 0;
	    net_config->sendbuf = getSendBuf(sock);
	} else {
	    net_config->dir = 1;
	    net_config->sendbuf = q;
	}
    }
#endif

    if(net_config->ttl == 1 && net_config->mcastAll == NULL) {
	getBroadCastAddress(sock, 
			    net_config->net_if,
			    &net_config->controlMcastAddr,
			    RECEIVER_PORT(net_config->portBase));
	setSocketToBroadcast(sock);
    } else {
	getMcastAllAddress(sock, 
			   net_config->net_if,
			   &net_config->controlMcastAddr,
			   net_config->mcastAll,
			   RECEIVER_PORT(net_config->portBase));
	setMcastDestination(sock, net_config->net_if,
			    &net_config->controlMcastAddr);
	setTtl(sock, net_config->ttl);
    }


    if(!(net_config->flags & FLAG_POINTOPOINT) &&
       ipIsZero(&net_config->dataMcastAddr)) {
	getDefaultMcastAddress(sock, net_config->net_if, 
			       &net_config->dataMcastAddr);
	udpc_flprintf("Using mcast address %s\n",
		      getIpString(&net_config->dataMcastAddr, ipBuffer));
	setMcastDestination(sock, net_config->net_if, 
			    &net_config->dataMcastAddr);
    }

    if(net_config->flags & FLAG_POINTOPOINT) {
	clearIp(&net_config->dataMcastAddr);
    }

    setPort(&net_config->dataMcastAddr, RECEIVER_PORT(net_config->portBase));

    udpc_flprintf("%sUDP sender for %s at ", 
		  disk_config->pipeName == NULL ? "" : "Compressed ",
		  disk_config->fileName == NULL ? "(stdin)" :
		  disk_config->fileName);
    printMyIp(net_config->net_if, sock);
    udpc_flprintf(" on %s \n", net_config->net_if);
    udpc_flprintf("Broadcasting control to %s\n",
		  getIpString(&net_config->controlMcastAddr, ipBuffer));

    net_config->capabilities = SENDER_CAPABILITIES;
    if(net_config->flags & FLAG_ASYNC)
	net_config->capabilities |= CAP_ASYNC;

    sendHello(net_config, sock);
    db = udpc_makeParticipantsDb();
    tries = 0;

    keyboardFd = -1;
    if(!(net_config->flags & FLAG_NOKBD)) {
	if(disk_config->fileName != NULL) {
	    keyboardFd = 0;
	} else {
	    keyboardFd = open("/dev/tty", O_RDONLY);
	    if(keyboardFd < 0) {
		fprintf(stderr, "Could not open keyboard: %s\n",
			strerror(errno));
	    }
	}
    }
	
    if(keyboardFd != -1)
	/* switch keyboard into character-at-a-time mode */
	setRaw(keyboardFd, &oldtio);

    if(net_config->min_clients || net_config->min_client_wait ||
       net_config->max_client_wait)
	firstConnectedP = &firstConnected;
    else
	firstConnectedP = NULL;	

    while(!(r=mainDispatcher(sock, db, disk_config, net_config,
			     keyboardFd, &tries, firstConnectedP))) {}
    if(keyboardFd != -1)
	restoreTerm(keyboardFd, &oldtio);	
    if(r == 1)
	doTransfer(sock, db, disk_config, net_config);
    free(db);
    return 0;
}

/*
 * Do the actual data transfer
 */
static int doTransfer(int sock, 
		      participantsDb_t db,
		      struct disk_config *disk_config,
		      struct net_config *net_config)
{
    int i;
    int ret;
    unsigned int endianness=PC_ENDIAN;
    struct fifo fifo;
    sender_stats_t stats;
    int in;
    int origIn;
    int pid;
    int isPtP = isPointToPoint(db, net_config->flags);

    if((net_config->flags & FLAG_POINTOPOINT) &&
       udpc_nrParticipants(db) != 1) {
	udpc_fatal(1,
		   "pointopoint mode set, and %d participants instead of 1\n",
		   udpc_nrParticipants(db));
    }

    net_config->rcvbuf=0;

    for(i=0; i<MAX_CLIENTS; i++)
	if(udpc_isParticipantValid(db, i)) {
	    unsigned int pRcvBuf = udpc_getParticipantRcvBuf(db, i);
	    if(isPtP)
		copyIpFrom(&net_config->dataMcastAddr, 
			   udpc_getParticipantIp(db,i));
	    net_config->capabilities &= 
		udpc_getParticipantCapabilities(db, i);
	    if(pRcvBuf != 0 && 
	       (net_config->rcvbuf == 0 || net_config->rcvbuf > pRcvBuf))
		net_config->rcvbuf = pRcvBuf;
	}


    udpc_flprintf("Starting transfer: %08x\n", net_config->capabilities);

    if(net_config->capabilities & CAP_BIG_ENDIAN)
	endianness = NET_ENDIAN;

    if(! (net_config->capabilities & CAP_NEW_GEN)) {
       net_config->dataMcastAddr = net_config->controlMcastAddr;
       net_config->flags &= ~(FLAG_SN | FLAG_ASYNC);
    }
    if(net_config->flags & FLAG_BCAST)
       net_config->dataMcastAddr = net_config->controlMcastAddr;

    origIn = openFile(disk_config);
    stats = allocSenderStats(origIn);
    in = openPipe(disk_config, origIn, &pid);
    udpc_initFifo(&fifo, net_config->blockSize);
    ret = spawnNetSender(&fifo, sock, endianness, net_config, db, stats);
    localReader(disk_config, &fifo, in);

    /* if we have a pipe, now wait for that too */
    if(pid) {
	waitForProcess(pid, "Pipe");
    }

    pthread_join(fifo.thread, NULL);    
    udpc_flprintf("Transfer complete.\007\n");

    /* remove all participants */
    for(i=0; i < MAX_CLIENTS; i++) {
	udpc_removeParticipant(db, i);
    }
    udpc_flprintf("\n");
    return 0;
}
