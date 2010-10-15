#include "grub.h"
#include "types.h"
#include "nic.h"
#include "rpc.h"

#define START_OPORT 700		/* mountd usually insists on secure ports */
#define OPORT_SWEEP 200		/* make sure we don't leave secure range */

static unsigned long rpc_id;
static int oport = START_OPORT;

static int await_rpc(int ival, void *ptr,
	unsigned short ptype, struct iphdr *ip, struct udphdr *udp)
{
	struct rpc_pkg *rpc;
	if (!udp) 
		return 0;
	if (arptable[ARP_CLIENT].ipaddr.s_addr != ip->dest.s_addr)
		return 0;
	if (ntohs(udp->dest) != ival)
		return 0;
	if (nic.packetlen < ETH_HLEN + sizeof(struct iphdr) + sizeof(struct udphdr) + 8)
		return 0;
	rpc = (struct rpc_pkg *)&nic.packet[ETH_HLEN];
	if (*(unsigned long *)ptr != ntohl(rpc->rbody.xid))
		return 0;
	if (MSG_REPLY != ntohl(rpc->rbody.mtype))
		return 0;
	return 1;
}

/**************************************************************************
RPC_ADD_CREDENTIALS - Add RPC authentication/verifier entries
**************************************************************************/
long *add_auth_unix(struct opaque_auth *_auth)
{
	int hl;
	long *p;

	/* Here's the executive summary on authentication requirements of the
	 * various NFS server implementations:  Linux accepts both AUTH_NONE
	 * and AUTH_UNIX authentication (also accepts an empty hostname field
	 * in the AUTH_UNIX scheme).  *BSD refuses AUTH_NONE, but accepts
	 * AUTH_UNIX (also accepts an empty hostname field in the AUTH_UNIX
	 * scheme).  To be safe, use AUTH_UNIX and pass the hostname if we have
	 * it (if the BOOTP/DHCP reply didn't give one, just use an empty
	 * hostname).  */

	hl = RNDUP(hostnamelen);

	/* Provide an AUTH_UNIX credential.  */
	_auth[0].flavor = htonl(AUTH_UNIX);		/* AUTH_UNIX */
	_auth[0].length = htonl(hl+20);		/* auth length */
	p = _auth[0].data;
	*p++ = htonl(0);		/* stamp */
	*p++ = htonl(hostnamelen);	/* hostname string */
	if (hostnamelen != hl) {
		*(p + (hl / BYTES_PER_XDR_UNIT - 1)) = 0; /* add zero padding */
	}
	memcpy(p, hostname, hostnamelen);
	p += hl / BYTES_PER_XDR_UNIT;
	*p++ = 0;			/* uid */
	*p++ = 0;			/* gid */
	*p++ = 0;			/* auxiliary gid list */

	/* Provide an AUTH_NONE verifier.  */
	_auth = (struct opaque_auth *)p;
	_auth->flavor = 0;			/* AUTH_NONE */
	_auth->length = 0;			/* auth length */

	return _auth->data;
}

long *add_auth_none(struct opaque_auth *_auth)
{
#ifdef RPC_DEBUG
	grub_printf("add_auth_none(auth:%d)\n", _auth);
#endif /* RPC_DEBUG */
	_auth[0].flavor = htonl(AUTH_NULL); /* cred */
	_auth[0].length = htonl(0);
	_auth[1].flavor = htonl(AUTH_NULL); /* verf */
	_auth[1].length = htonl(0);
#ifdef RPC_DEBUG
	grub_printf("add_auth_none OK, returned %d\n", _auth[1].data);
#endif /* RPC_DEBUG */
	return _auth[1].data;
}

AUTH *__authnone_create(AUTH *_auth)
{
	_auth->add_auth = &add_auth_none;
	return _auth;
}
/**
 * auth_destroy
 * 
 * Anything is static, do nothing here
 **/
void auth_destroy(AUTH *_auth)
{
	return;
}

static int32_t *xdr_inline(XDR *xdrs, int len)
{
	int32_t *retval;

	if (((char *)xdrs->x_private + xdrs->x_handy) <
	    ((char *)xdrs->x_base + len ))
		return NULL;
	retval = (int32_t *)xdrs->x_base;
	xdrs->x_base = (char *)xdrs->x_base + len;
	xdrs->x_handy -= len;

	return retval;
}

/** 
 * clntudp_call
 * 
 * Call the remote procedure _procnum_ in remote program _prognum_ of version
 * _versnum_, which UDP address is _sin_addr_ and port is _sin_port_.
 **/
static int 
clntudp_call(CLIENT *clnt, u_long procnum, xdrproc_t inproc, char *in,
	     xdrproc_t outproc, char *out, struct timeval tout)
{
	struct rpc_pkg buf, *reply;



	unsigned long xid;
	int retries;
	long timeout, tout_ticks;
       	struct XDR xdrs;
	/* Must be static */
	static int tokens = 0;

#ifdef RPC_DEBUG
	grub_printf("clntudp_call(procnum: %d)\n buf at %d\n", procnum, &buf);
#endif /* RPC_DEBUG */
	
	tout_ticks = tout.tv_sec * TICKS_PER_SEC
		+ tout.tv_usec * TICKS_PER_SEC / 1000;
	rx_qdrain();
	
	xid = rpc_id++;
	buf.cbody.xid = htonl(xid);
	buf.cbody.mtype = htonl(MSG_CALL);
	buf.cbody.rpcvers = htonl(2);	/* use RPC version 2 */
	buf.cbody.prog = htonl(clnt->prognum);
	buf.cbody.vers = htonl(clnt->versnum);
	buf.cbody.proc = htonl(procnum);

	/*
	 * We use _x_base_ to store the heap top and _x_private_ for
	 * the heap end. _x_handy is the max size of the heap.
	 */
	xdrs.x_base = xdrs.x_private 
		= (caddr_t)clnt->cl_auth->add_auth(buf.cbody.auth);
	xdrs.x_handy = (char *)&buf + sizeof(buf) - (char *)xdrs.x_base;
#ifdef RPC_DEBUG
	grub_printf("x_base at %d, x_private at %d, x_handy is %d\n",
		    xdrs.x_base, xdrs.x_private, xdrs.x_handy);
	grub_printf("Encoding...");
#endif
	/* Encoding... */
	xdrs.x_op = XDR_ENCODE;
	if (!inproc(&xdrs, in)){
		/* Unable to encode */
		return RPC_CANTENCODEARGS;
	}
#ifdef RPC_DEBUG
	grub_printf("OK.\n");
	grub_printf("x_base at %d, x_private at %d, x_handy is %d\n",
		    xdrs.x_base, xdrs.x_private, xdrs.x_handy);
#endif
	/*
	 * Try to implement something similar to a window protocol in
	 * terms of response to losses. On successful receive, increment
	 * the number of tokens by 1 (cap at 256). On failure, halve it.
	 * When the number of tokens is >= 2, use a very short timeout.
	 */
	for (retries = 0; retries < MAX_RPC_RETRIES; retries++) {
		if (tokens >= 2)
			timeout = TICKS_PER_SEC/2;
		else
			timeout = rfc2131_sleep_interval(tout_ticks, retries);
		udp_transmit(arptable[clnt->server].ipaddr.s_addr, clnt->sport,
			     clnt->port, (char *)xdrs.x_base - (char *)&buf, &buf);
		if (await_reply(await_rpc, clnt->sport, &xid, timeout)) {
			if (tokens < 256)
				tokens++;
			reply = (struct rpc_pkg *)&nic.packet[ETH_HLEN];
			if (reply->rbody.stat || reply->rbody.verifier 
			    || reply->rbody.astatus) {
				if (reply->rbody.stat) {
					/* MSG_DENIED */
					/* TODO: specify the error */
					return RPC_AUTHERROR;
				}
				if (reply->rbody.astatus) {
					/* RPC couldn't decode parameters */
					return RPC_CANTDECODEARGS;
				}
			}
			xdrs.x_base = xdrs.x_private = (char *)reply->rbody.data;
			xdrs.x_handy = nic.packetlen - ((char *)xdrs.x_base - (char *)reply);
#ifdef RPC_DEBUG
			grub_printf("x_base at %d, x_private at %d, x_handy is %d\n",
				    xdrs.x_base, xdrs.x_private, xdrs.x_handy);
			grub_printf("Decoding...");
#endif
			xdrs.x_op = XDR_DECODE;
			if (!outproc(&xdrs, out))
				return RPC_CANTDECODERES;
#ifdef RPC_DEBUG
			grub_printf("OK.\n");
			grub_printf("x_base at %d, x_private at %d, x_handy is %d\n",
				    xdrs.x_base, xdrs.x_private, xdrs.x_handy);
#endif
			return RPC_SUCCESS;
		}
	        else
			tokens >>= 1;
	}
	return RPC_CANTRECV;
}

/**
 * xdr_mapping
 *
 * An XDR proc wirten by hand.
 **/
static bool_t xdr_mapping (XDR *xdrs, mappping *objp)
{
	register int32_t *buf;

	if (xdrs->x_op == XDR_ENCODE) {
		buf = XDR_INLINE (xdrs, 4 * BYTES_PER_XDR_UNIT);
		/* It may never happened in GRUB :-) */
		if (buf == NULL)
			return FALSE;
		IXDR_PUT_U_LONG(buf, objp->prog);
		IXDR_PUT_U_LONG(buf, objp->vers);
		IXDR_PUT_U_LONG(buf, objp->prot);
		IXDR_PUT_U_LONG(buf, objp->port);
		return TRUE;
	} else if (xdrs->x_op == XDR_DECODE) {
		buf = XDR_INLINE (xdrs, 4 * BYTES_PER_XDR_UNIT);
		/* It may never happened in GRUB :-) */
		if (buf == NULL)
			return FALSE;
		objp->prog = IXDR_GET_U_LONG(buf);
		objp->vers = IXDR_GET_U_LONG(buf);
		objp->prot = IXDR_GET_U_LONG(buf);
		objp->port = IXDR_GET_U_LONG(buf);
		return TRUE;
	}
	return TRUE;
}

bool_t xdr_u_int (XDR *xdrs, u_int *objp)
{
	register int32_t *buf;

	if (xdrs->x_op == XDR_ENCODE) {
		buf = XDR_INLINE (xdrs, BYTES_PER_XDR_UNIT);
		/* It may never happened in GRUB :-) */
		if (buf == NULL)
			return FALSE;
		IXDR_PUT_U_LONG(buf, *objp);
		return TRUE;
	} else if (xdrs->x_op == XDR_DECODE) {
		buf = XDR_INLINE (xdrs, BYTES_PER_XDR_UNIT);
		/* It may never happened in GRUB :-) */
		if (buf == NULL)
			return FALSE;
		*objp = IXDR_GET_U_LONG(buf);
		return TRUE;
	}
	return TRUE;
}

/**
 * __pmapudp_getport
 *
 * Get RPC program _prognum_ of version _versnum_ port through
 * portmap. The portmap server is specified by _pmapsrv_.
 **/
static
u_short __pmapudp_getport(int pmapsrv, u_long prognum,
			  u_long versnum)
{
	u_int retval;
	CLIENT clnt_s, *clnt;
	AUTH auth_s, *_auth;
	struct mapping map;
	struct timeval tout = { 25, 0 };

#ifdef RPC_DEBUG
	grub_printf("__pmapudp_getport(pmapsrv:%d, prognum:%d, versnum:%d)\n", 
	       pmapsrv, prognum, versnum);
#endif /* RPC_DEBUG */

	/* 
	 * Initialize the RPC parameter, which will be handled by
	 * xdr_mapping
	 */
	map.prog = prognum;
	map.vers = versnum;
	map.prot = IPPROTO_UDP;
	map.port = 0;
	/* We use AUTH_NONE */
	_auth = __authnone_create(&auth_s);
	if (_auth == NULL)
		return 0;
	clnt = __clntudp_create
		(&clnt_s, _auth, pmapsrv, PMAP_PORT, PMAP_PROG, PMAP_VERS);
	if (clnt == NULL)
		return 0;
	if (clnt_call (clnt, PMAPROC_GETPORT, (xdrproc_t)xdr_mapping,
		       (caddr_t)&map, (xdrproc_t)xdr_u_int, (caddr_t)&retval,
		       tout) != RPC_SUCCESS) {
		return -1;
	}
	return retval;
}

/**
 * __clntudp_create
 *
 * Config the _clnt_ to the RPC server specified by _server_ and
 * _port_. The RPC program is _prognum_ with version _versnum_. If
 * _port_ == 0, use portmap to get it.
 **/
CLIENT *__clntudp_create(CLIENT *clnt, AUTH *_auth, int server, u_short port,
			 u_long prognum, u_long versnum)
{
	unsigned long t;
	int sport;
#ifdef RPC_DEBUG
	grub_printf("__clntudp_create(server:%d, port:%d, prognum:%d, versnum:%d)\n",
	       server, port, prognum, versnum);
#endif /* RPC_DEBUG */
	sport = oport++;
	if (oport > START_OPORT + OPORT_SWEEP){
		oport = START_OPORT;
	}
	if (port == 0)
		port = __pmapudp_getport(server, prognum, versnum);
	if (port == 0){
		/* If dynamic, we must free the clnt here */
		return NULL;
	}
	/* Initial the CLIENT structure. */
	clnt->cl_auth = _auth;
	clnt->server = server;
	clnt->sport = sport;
	clnt->prognum = prognum;
	clnt->versnum = versnum;
	clnt->port = port;

	t = currticks();
	rpc_id = t ^ (t << 8) ^ (t << 16);
#ifdef RPC_DEBUG
	grub_printf("__clntudp_create OK. Port is %d, rpc_id is %d.\n",
		    port, rpc_id);
#endif /* RPC_DEBUG */
	return clnt;
}
