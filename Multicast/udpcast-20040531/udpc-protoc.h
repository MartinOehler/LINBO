#ifndef UDPC_PROTOC_H
#define UDPC_PROTOC_H

#include "udpcast.h"

#define MAX_BLOCK_SIZE 1456
#define MAX_FEC_INTERLEAVE 256

/**
 * This file describes the UDPCast protocol
 */
enum opCode {    
    /* Client to server */

    CMD_OK,	     /* all is ok, no need to retransmit anything */
    CMD_RETRANSMIT,  /* client asks for some data to be retransmitted */
    CMD_GO,	     /* client tells server to start */
    CMD_CONNECT_REQ, /* client tries to find out server's address */
    CMD_DISCONNECT,  /* client wants to disconnect itself */

    /* Server to client */
    CMD_HELLO,	     /* server says he's up */
    CMD_REQACK,	     /* server request acknowledgments from client */
    CMD_CONNECT_REPLY, /* client tries to find out server's address */

    CMD_DATA,        /* a block of data */
#ifdef BB_FEATURE_UDPCAST_FEC
    CMD_FEC,	     /* a forward-error-correction block */
#endif
};

union message {
    unsigned short opCode;
    struct ok {
	unsigned short opCode;
	short reserved;
	int sliceNo;
    } ok;

    struct retransmit {
	unsigned short opCode;
	short reserved;
	int sliceNo;
	int rxmit;
	unsigned char map[MAX_SLICE_SIZE / BITS_PER_CHAR];
    } retransmit;

    struct connectReq {
	unsigned short opCode;
	short reserved;
	int capabilities;
	unsigned int rcvbuf;
    } connectReq;

    struct go {
	unsigned short opCode;
	short reserved;
    } go;

    struct disconnect {
	unsigned short opCode;
	short reserved;
    } disconnect;
};



struct connectReply {
    unsigned short opCode;
    short reserved;
    int clNr;
    int blockSize;
    int capabilities;
    unsigned char mcastAddr[16]; /* provide enough place for IPV6 */
};

struct hello {
    unsigned short opCode;
    short reserved;
    int capabilities;
    unsigned char mcastAddr[16]; /* provide enough place for IPV6 */
    short blockSize;
};

union serverControlMsg {
    unsigned short opCode;
    short reserved;
    struct hello hello;
    struct connectReply connectReply;

};


struct dataBlock {
    unsigned short opCode;
    short reserved;
    int sliceNo;
    unsigned short blockNo;
    unsigned short reserved2;
    int bytes;
};

struct fecBlock {
    unsigned short opCode;
    short stripes;
    int sliceNo;
    unsigned short blockNo;
    unsigned short reserved2;
    int bytes;
};

struct reqack {
    unsigned short opCode;
    short reserved;
    int sliceNo;
    int bytes;
    int rxmit;
};

union serverDataMsg {
    unsigned short opCode;
    struct reqack reqack;
    struct dataBlock dataBlock;
    struct fecBlock fecBlock;
};

/* ============================================
 * Capabilities
 */

/* Does the client support the new CMD_DATA command, which carries
 * capabilities mask?
 * "new generation" client:
 *   - capabilities word included in hello/connectReq commands
 *   - client multicast capable
 *   - client can receive ASYNC and SN
 */
#define CAP_NEW_GEN 0x0001

/* Use multicast instead of Broadcast for data */
/*#define CAP_MULTICAST 0x0002*/

#ifdef BB_FEATURE_UDPCAST_FEC
/* Forward error correction */
#define CAP_FEC 0x0004
#endif

/* Supports big endians (a.k.a. network) */
#define CAP_BIG_ENDIAN 0x0008

/* Support little endians (a.k.a. PC) */
#define CAP_LITTLE_ENDIAN 0x0010

/* This transmission is asynchronous (no client reply) */
#define CAP_ASYNC 0x0020

/* Server currently supports CAPABILITIES and MULTICAST */
#define SENDER_CAPABILITIES ( \
	CAP_NEW_GEN | \
	CAP_LITTLE_ENDIAN | \
	CAP_BIG_ENDIAN)


#define RECEIVER_CAPABILITIES ( \
	CAP_NEW_GEN | \
	CAP_LITTLE_ENDIAN | \
	CAP_BIG_ENDIAN)

#define NET_ENDIAN 0
#define PC_ENDIAN 1

#endif
