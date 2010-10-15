#ifndef UDP_SENDER_H
#define UDP_SENDER_H

#include "udp-sender.h"
#include "udpcast.h"
#include "participants.h"
#include "statistics.h"

extern FILE *udpc_log;

struct fifo;

#define log udpc_log
#define openFile udpc_openFile
#define openPipe udpcs_openPipe
#define localReader udpc_localReader
#define spawnNetSender udpc_spawnNetSender
#define startSender udpc_startSender
#define doSend udpc_doSend

int openFile(struct disk_config *config);
int openPipe(struct disk_config *config, int in, int *pid);
int localReader(struct disk_config *config, struct fifo *fifo, int in);

int spawnNetSender(struct fifo *fifo,
		   int sock,
		   int endianness,
		   struct net_config *config,
		   participantsDb_t db,
		   sender_stats_t stats);
int startSender(struct disk_config *disk_config,
		struct net_config *net_config);


#define BCAST_DATA(s, msg) \
	doSend(s, &msg, sizeof(msg), &net_config->dataMcastAddr)


/**
 * "switched network" mode: server already starts sending next slice before
 * first one is acknowledged. Do not use on old coax networks
 */
#define FLAG_SN    0x0001

/**
 * Asynchronous mode: do not any confirmation at all from clients.
 * Useful in situations where no return channel is available
 */
#define FLAG_ASYNC 0x0002


/**
 * Point-to-point transmission mode: use unicast in the (frequent)
 * special case where there is only one receiver.
 */
#define FLAG_POINTOPOINT 0x0004


/**
 * Do automatic rate limitation by monitoring socket's send buffer
 * size. Not very useful, as this still doesn't protect against the
 * switch dropping packets because its queue (which might be slightly slower)
 * overruns
 */
#define FLAG_AUTORATE 0x0008

#ifdef BB_FEATURE_UDPCAST_FEC
/**
 * Forward error correction
 */
#define FLAG_FEC 0x0010
#endif

/**
 * Use broadcast rather than multicast (useful for cards that don't support
 * multicast correctly
 */
#define FLAG_BCAST 0x0020

/**
 * Never use point-to-point, even if only one receiver
 */
#define FLAG_NOPOINTOPOINT 0x0040


/*
 * Don't ask for keyboard input on sender end.
 */
#define FLAG_NOKBD 0x0080

#endif
