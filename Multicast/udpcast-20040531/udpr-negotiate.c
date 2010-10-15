#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "log.h"
#include "socklib.h"
#include "udpcast.h"
#include "udpc-protoc.h"
#include "fifo.h"
#include "udp-receiver.h"
#include "util.h"
#include "produconsum.h"
#include "statistics.h"

#ifndef O_BINARY
# define O_BINARY 0
#endif

static int sendConnectReq(struct client_config *client_config,
			  struct net_config *net_config,
			  int haveServerAddress) {
    struct connectReq connectReq;
    int endianness = client_config->endianness;    

    if(net_config->flags & FLAG_PASSIVE)
	return 0;

    connectReq.opCode = htoxs(CMD_CONNECT_REQ);
    connectReq.reserved = 0;
    connectReq.capabilities = htonl(RECEIVER_CAPABILITIES);
    connectReq.rcvbuf = htonl(getRcvBuf(client_config->toServer));
    if(haveServerAddress)
      return SSEND(connectReq);
    else
      return BCAST_CONTROL(client_config->toServer, connectReq);
}

static int sendGo(struct client_config *client_config) {
    struct go go;
    int endianness = client_config->endianness;
    go.opCode = htoxs(CMD_GO);
    go.reserved = 0;
    return SSEND(go);
}

static void sendDisconnectWrapper(int exitStatus,
				  void *args) {    
    sendDisconnect(exitStatus, (struct client_config *) args);
}

void sendDisconnect(int exitStatus,
		    struct client_config *client_config) {    
    int endianness = client_config->endianness;
    struct disconnect disconnect;
    disconnect.opCode = htoxs(CMD_DISCONNECT);
    disconnect.reserved = 0;
    SSEND(disconnect);
    if (exitStatus == 0)
	udpc_flprintf("Transfer complete.\007\n");
}


struct startTransferArgs {
    int fd;
    int pipeFd;
    struct client_config *client_config;
    int doWarn;
};


/* FIXME: should cancel thread if start comes from peer */
static void *startTransfer(void *args0)
{

    struct startTransferArgs *args = (struct startTransferArgs *) args0;
    struct client_config *client_config = args->client_config;
    int fd = args->fd;
    char ch;
    struct termios oldtio;
    int n=1;
    int maxFd=args->pipeFd;
    fd_set read_set;
    int nr_desc;

    setRaw(fd, &oldtio);
    udpc_flprintf("Press any key to start receiving data!\n");
    if (args->doWarn)
	udpc_flprintf("WARNING: This will overwrite the hard disk of this machine\n");

    if(fd > maxFd)
      maxFd = fd;
    FD_ZERO(&read_set);
    FD_SET(args->pipeFd, &read_set);
    FD_SET(fd, &read_set);
    do {
      nr_desc = select(maxFd+1,  &read_set, 0, 0,  NULL);
    } while(nr_desc < 0);
    if(FD_ISSET(fd, &read_set))
       n = read(fd, &ch, 1);
    restoreTerm(fd, &oldtio);

    if(client_config->isStarted || n <= 0)
	return NULL; /* already started, or end of file reached on input */
    udpc_flprintf("Sending go signal %d %s %d\n", n, strerror(errno), fd);
    if (sendGo(client_config) < 0) {
	perror("Send go");
    }
    return NULL;
}


static void spawnStartTransferListener(struct client_config *client_config,
				       int doWarn, int pipeFd)
{
    struct startTransferArgs *args = MALLOC(struct startTransferArgs);
    pthread_t sendGoThread;
    pthread_attr_t sendGoAttr;
    args->fd = 0;
    args->pipeFd = pipeFd;
    args->client_config = client_config;
    args->doWarn = doWarn;
    pthread_attr_init(&sendGoAttr);
    pthread_attr_setdetachstate(&sendGoAttr, PTHREAD_CREATE_DETACHED);
    pthread_create(&sendGoThread, &sendGoAttr, 
		   &startTransfer, args);
}

int startReceiver(int doWarn,
		  struct disk_config *disk_config,
		  struct net_config *net_config)
{
    char ipBuffer[16];
    union serverControlMsg Msg;
    int connectReqSent=0;
    struct client_config client_config;
    int outFile=1;
    int pipedOutFile;
    struct sockaddr myIp;
    int pipePid = 0;
    int origOutFile;
    int haveServerAddress;

    client_config.server_is_newgen = 0;
    if(disk_config->fileName != NULL) {
	int oflags = O_CREAT | O_WRONLY;
	if(!(disk_config->flags & FLAG_NOSYNC)) {
	    oflags |= O_SYNC;
	}
	outFile = open(disk_config->fileName, oflags | O_BINARY, 0644);
	if(outFile < 0) {
#ifdef NO_BB
	    extern int errno;
#endif
	    udpc_fatal(1, "open outfile %s: %s\n",
		       disk_config->fileName, strerror(errno));
	}
    }

    client_config.toServer = makeSocket(&net_config->net_if,
					RECEIVER_PORT(net_config->portBase));
    if(net_config->requestedBufSize)
	setRcvBuf(client_config.toServer, net_config->requestedBufSize);
    if(net_config->ttl == 1 && net_config->mcastAll == NULL) {
	getBroadCastAddress(client_config.toServer, 
			    net_config->net_if,
			    &net_config->controlMcastAddr,
			    SENDER_PORT(net_config->portBase));
	setSocketToBroadcast(client_config.toServer);
    } else {
	getMcastAllAddress(client_config.toServer, 
			   net_config->net_if,
			   &net_config->controlMcastAddr,
			   net_config->mcastAll,
			   SENDER_PORT(net_config->portBase));
	setMcastDestination(client_config.toServer, net_config->net_if,
			    &net_config->controlMcastAddr);
	setTtl(client_config.toServer, net_config->ttl);
    }
    clearIp(&net_config->dataMcastAddr);
    udpc_flprintf("%sUDP receiver for %s at ", 
		  disk_config->pipeName == NULL ? "" :  "Compressed ",
		  disk_config->fileName == NULL ? "(stdout)":disk_config->fileName);
    printMyIp(net_config->net_if, client_config.toServer);
    udpc_flprintf(" on %s\n", net_config->net_if);

    connectReqSent = 0;
    haveServerAddress = 0;

    client_config.endianness = PC_ENDIAN;
    if (sendConnectReq(&client_config, net_config, haveServerAddress) < 0) {
	perror("sendto to locate server");
	exit(1);
    }

    client_config.endianness = NET_ENDIAN;
    client_config.clientNumber= 0; /*default number for asynchronous transfer*/
    /*flprintf("Endian = NET\n");*/
    while(1) {
	// int len;
	int msglen;

	if (!connectReqSent) {
	    if (sendConnectReq(&client_config, net_config,
			       haveServerAddress) < 0) {
		perror("sendto to locate server");
		exit(1);
	    }
	    connectReqSent = 1;
	}

	haveServerAddress=0;

	// len = sizeof(server);
	msglen=RECV(client_config.toServer, 
		    Msg, client_config.serverAddr, net_config->portBase);
	if (msglen < 0) {
	    perror("recvfrom to locate server");
	    exit(1);
	}
	
	if(getPort(&client_config.serverAddr) != 
	   SENDER_PORT(net_config->portBase))
	    /* not from the right port */
	    continue;

	switch(ntohs(Msg.opCode)) {
	    case CMD_CONNECT_REPLY:
		client_config.endianness = NET_ENDIAN;
		/*flprintf("Endian = NET\n");*/
		client_config.clientNumber = ntohl(Msg.connectReply.clNr);
		net_config->blockSize = ntohl(Msg.connectReply.blockSize);

		udpc_flprintf("received message, cap=%08lx\n",
			      (long) ntohl(Msg.connectReply.capabilities));
		if(ntohl(Msg.connectReply.capabilities) & CAP_NEW_GEN) {
		    client_config.server_is_newgen = 1;
		    copyFromMessage(&net_config->dataMcastAddr,
				    Msg.connectReply.mcastAddr);
		}
		if (client_config.clientNumber == -1) {
		    udpc_fatal(1, "Too many clients already connected\n");
		}
		goto break_loop;

	    case CMD_HELLO:
		client_config.endianness = NET_ENDIAN;
		/*flprintf("Endian = NET\n");*/
		connectReqSent = 0;
		if(ntohl(Msg.hello.capabilities) & CAP_NEW_GEN) {
		    client_config.server_is_newgen = 1;
		    copyFromMessage(&net_config->dataMcastAddr,
				    Msg.hello.mcastAddr);
		    net_config->blockSize = pctohl(Msg.hello.blockSize);
		    if(ntohl(Msg.hello.capabilities) & CAP_ASYNC)
			net_config->flags |= FLAG_PASSIVE;
		    if(net_config->flags & FLAG_PASSIVE)
			goto break_loop;
		}
		haveServerAddress=1;
		continue;
	    case CMD_CONNECT_REQ:
		client_config.endianness = NET_ENDIAN;
		/*flprintf("Endian = NET\n");*/
		continue;
	    default:
		break;
	}


	switch(pctohs(Msg.opCode)) {
	    case CMD_CONNECT_REPLY:
		client_config.endianness = PC_ENDIAN;
		/*flprintf("Endian = PC\n");*/
		client_config.clientNumber = pctohl(Msg.connectReply.clNr);
		net_config->blockSize = pctohl(Msg.connectReply.blockSize);
		if (client_config.clientNumber == -1) {
		    udpc_fatal(1, "Too many clients already connected\n");
		}
		goto break_loop;

	    case CMD_HELLO:
		if(msglen > 4)
		    client_config.endianness = NET_ENDIAN;
		else
		    client_config.endianness = PC_ENDIAN;
		/*flprintf("Endian = %s\n",
		  client_config.endianness == NET_ENDIAN ? "NET" : "PC");*/
		if(ntohl(Msg.hello.capabilities) & CAP_NEW_GEN) {
		    client_config.server_is_newgen = 1;
		    copyFromMessage(&net_config->dataMcastAddr,
				    Msg.hello.mcastAddr);
		    net_config->blockSize = ntohs(Msg.hello.blockSize);
		    if(ntohl(Msg.hello.capabilities) & CAP_ASYNC)
			net_config->flags |= FLAG_PASSIVE;
		    if(net_config->flags & FLAG_PASSIVE)
			goto break_loop;
		}
		haveServerAddress=1;
		connectReqSent = 0;
		continue;
	
	    case CMD_CONNECT_REQ:
		client_config.endianness = PC_ENDIAN;
		/*flprintf("Endian = PC\n");*/
		continue;
	    default:
		break;
	}

	udpc_fatal(1, 
		   "Bad server reply %04x. Other transfer in progress?\n",
		   (unsigned short) ntohs(Msg.opCode));
    }

 break_loop:
    udpc_flprintf("Connected as #%d to %s\n", 
		  client_config.clientNumber, 
		  getIpString(&client_config.serverAddr, ipBuffer));

    getMyAddress(client_config.toServer, net_config->net_if, &myIp);

    if(!ipIsZero(&net_config->dataMcastAddr)  &&
       !ipIsEqual(&net_config->dataMcastAddr, &myIp)) {
	udpc_flprintf("Listening to multicast on %s\n",
		      getIpString(&net_config->dataMcastAddr, ipBuffer));
	mcastListen(client_config.toServer,
		    net_config->net_if,
		    &net_config->dataMcastAddr);
    }


    origOutFile = outFile;
    pipedOutFile = openPipe(client_config.toServer, outFile, disk_config,
			    &pipePid);
#ifndef __CYGWIN__
    on_exit(sendDisconnectWrapper, &client_config);
#endif
    {
	struct fifo fifo;
	void *returnValue;
	int myPipe[2];
	receiver_stats_t stats = allocReadStats(origOutFile);
	
	udpc_initFifo(&fifo, net_config->blockSize);

	fifo.data = pc_makeProduconsum(fifo.dataBufSize, "receive");

	client_config.isStarted = 0;

	pipe(myPipe);
	spawnNetReceiver(&fifo,&client_config, net_config, stats, myPipe[1]);
	if(!(net_config->flags & (FLAG_PASSIVE|FLAG_NOKBD)))
	    spawnStartTransferListener(&client_config, doWarn, myPipe[0]);
	writer(&fifo, pipedOutFile);
	if(pipePid)
	    close(pipedOutFile);
	pthread_join(client_config.thread, &returnValue);

	/* if we have a pipe, now wait for that too */
	if(pipePid) {
	    waitForProcess(pipePid, "Pipe");
	}
	fsync(origOutFile);
	displayReceiverStats(stats);

    }
    return 0;
}
