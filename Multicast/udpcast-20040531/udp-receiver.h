#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <pthread.h>
#include <sys/socket.h>
#include "statistics.h"

struct client_config {
    int endianness;
    int toServer;
    struct sockaddr serverAddr;
    int clientNumber;
    int isStarted;
    pthread_t thread;
    int server_is_newgen;
};

struct fifo;

#define spawnNetReceiver udpc_spawnNetReceiver
#define writer udpc_writer
#define openPipe udpcr_openPipe
#define sendDisconnect udpc_sendDisconnect
#define startReceiver udpc_startReceiver

int spawnNetReceiver(struct fifo *fifo,
		     struct client_config *client_config,
		     struct net_config *net_config,
		     receiver_stats_t stats,
		     int notifyPipeFd);
int writer(struct fifo *fifo, int fd);
int openPipe(int net,
	     int disk, 
	     struct disk_config *disk_config,
	     int *pipePid);
void sendDisconnect(int, struct client_config *);
int startReceiver(int doWarn,
		  struct disk_config *disk_config,
		  struct net_config *net_config);

#define SSEND(x) SEND(client_config->toServer, x, client_config->serverAddr)

/**
 * Receiver will passively listen to sender. Works best if sender runs
 * in async mode
 */
#define FLAG_PASSIVE 0x0010


/**
 * Do not write file synchronously
 */
#define FLAG_NOSYNC 0x0040

/*
 * Don't ask for keyboard input on receiver end.
 */
#define FLAG_NOKBD 0x0080

#endif
