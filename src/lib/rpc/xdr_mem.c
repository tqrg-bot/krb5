/* @(#)xdr_mem.c	2.1 88/07/29 4.0 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)xdr_mem.c 1.19 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 *
 */


#include <gssrpc/types.h>
#include <gssrpc/xdr.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

static bool_t	xdrmem_getlong(XDR *, long *);
static bool_t	xdrmem_putlong(XDR *, long *);
static bool_t	xdrmem_getbytes(XDR *, caddr_t, unsigned int);
static bool_t	xdrmem_putbytes(XDR *, caddr_t, unsigned int);
static unsigned int	xdrmem_getpos(XDR *);
static bool_t	xdrmem_setpos(XDR *, unsigned int);
static rpc_int32 *	xdrmem_inline(XDR *, int);
static void	xdrmem_destroy(XDR *);

static struct	xdr_ops xdrmem_ops = {
	xdrmem_getlong,
	xdrmem_putlong,
	xdrmem_getbytes,
	xdrmem_putbytes,
	xdrmem_getpos,
	xdrmem_setpos,
	xdrmem_inline,
	xdrmem_destroy
};

/*
 * The procedure xdrmem_create initializes a stream descriptor for a
 * memory buffer.  
 */
void
xdrmem_create(xdrs, addr, size, op)
	register XDR *xdrs;
	caddr_t addr;
	unsigned int size;
	enum xdr_op op;
{

	xdrs->x_op = op;
	xdrs->x_ops = &xdrmem_ops;
	xdrs->x_private = xdrs->x_base = addr;
	xdrs->x_handy = (size > INT_MAX) ? INT_MAX : size; /* XXX */
}

static void
xdrmem_destroy(xdrs)
	XDR *xdrs;
{
}

static bool_t
xdrmem_getlong(xdrs, lp)
	register XDR *xdrs;
	long *lp;
{

	if (xdrs->x_handy < sizeof(rpc_int32))
		return (FALSE);
	else
		xdrs->x_handy -= sizeof(rpc_int32);
	*lp = (long)ntohl(*((rpc_u_int32 *)(xdrs->x_private)));
	xdrs->x_private = (char *)xdrs->x_private + sizeof(rpc_int32);
	return (TRUE);
}

static bool_t
xdrmem_putlong(xdrs, lp)
	register XDR *xdrs;
	long *lp;
{

	if (xdrs->x_handy < sizeof(rpc_int32))
		return (FALSE);
	else
		xdrs->x_handy -= sizeof(rpc_int32);
	*(rpc_int32 *)xdrs->x_private = (rpc_int32)htonl((rpc_u_int32)(*lp));
	xdrs->x_private = (char *)xdrs->x_private + sizeof(rpc_int32);
	return (TRUE);
}

static bool_t
xdrmem_getbytes(xdrs, addr, len)
	register XDR *xdrs;
	caddr_t addr;
	register unsigned int len;
{

	if (xdrs->x_handy < len)
		return (FALSE);
	else
		xdrs->x_handy -= len;
	memmove(addr, xdrs->x_private, len);
	xdrs->x_private = (char *)xdrs->x_private + len;
	return (TRUE);
}

static bool_t
xdrmem_putbytes(xdrs, addr, len)
	register XDR *xdrs;
	caddr_t addr;
	register unsigned int len;
{

	if (xdrs->x_handy < len)
		return (FALSE);
	else
		xdrs->x_handy -= len;
	memmove(xdrs->x_private, addr, len);
	xdrs->x_private = (char *)xdrs->x_private + len;
	return (TRUE);
}

static unsigned int
xdrmem_getpos(xdrs)
	register XDR *xdrs;
{
/*
 * 11/3/95 - JRG - Rather than recast everything for 64 bit, just convert
 * pointers to longs, then cast to int.
 */
	return (unsigned int)((unsigned long)xdrs->x_private - (unsigned long)xdrs->x_base);
}

static bool_t
xdrmem_setpos(xdrs, pos)
	register XDR *xdrs;
	unsigned int pos;
{
	register caddr_t newaddr = xdrs->x_base + pos;
	register caddr_t lastaddr = (char *) xdrs->x_private + xdrs->x_handy;

	if ((long)newaddr > (long)lastaddr)
		return (FALSE);
	xdrs->x_private = newaddr;
	xdrs->x_handy = (int)((long)lastaddr - (long)newaddr);
	return (TRUE);
}

static rpc_int32 *
xdrmem_inline(xdrs, len)
	register XDR *xdrs;
	int len;
{
	rpc_int32 *buf = 0;

	if (len >= 0 && xdrs->x_handy >= len) {
		xdrs->x_handy -= len;
		buf = (rpc_int32 *) xdrs->x_private;
		xdrs->x_private = (char *)xdrs->x_private + len;
	}
	return (buf);
}
