/*	$NetBSD: print-cdp.c,v 1.2 2001/06/25 19:59:57 itojun Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Code by Gert Doering, SpaceNet GmbH, gert@space.net
 *
 * Reference documentation:
 *    http://www.cisco.com/univercd/cc/td/doc/product/lan/trsrb/frames.htm
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static const char rcsid[] =
    "@(#) Header: /tcpdump/master/tcpdump/print-cdp.c,v 1.8 2001/06/15 20:52:12 fenner Exp";
#else
__RCSID("$NetBSD: print-cdp.c,v 1.2 2001/06/25 19:59:57 itojun Exp $");
#endif
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/param.h>
#include <sys/time.h>

#include <netinet/in.h>

#include <ctype.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"			/* must come after interface.h */

static int cdp_print_addr(const u_char *, int);
static int cdp_print_prefixes(const u_char *, int);

void
cdp_print(const u_char *p, u_int length, u_int caplen,
	  const u_char *esrc, const u_char *edst)
{
	int i;
	int type, len;

	/* Cisco Discovery Protocol */

	if (caplen < 4) {
		(void)printf("[|cdp]");
		return;
	}

	i = 0;		/* CDP data starts at offset 0 */
	printf("CDP v%u, ttl=%us", p[i], p[i + 1]);
	i += 4;		/* skip version, TTL and chksum */

	while (i < length) {
		if (i + 4 > caplen)
			goto trunc;
		type = (p[i] <<  8) + p[i + 1];
		len  = (p[i + 2] << 8) + p[i + 3];

		if (vflag > 1)
			printf("\n\t");

		if (vflag)
			printf(" %02x/%02x", type, len);

		if (i + len > caplen)
			goto trunc;

		switch (type) {
		case 0x00:
			printf(" Goodbye");
			break;
		case 0x01:
			printf(" DevID '%.*s'", len - 4, p + i + 4);
			break;
		case 0x02:
			printf(" Addr");
			if (cdp_print_addr(p + i + 4, len - 4) < 0)
				goto trunc;
			break;
		case 0x03:
			printf(" PortID '%.*s'", len - 4, p + i + 4);
			break;
		case 0x04:
			printf(" CAP 0x%02x", (unsigned) p[i + 7]);
			break;
		case 0x05:
			if (vflag > 1)
				printf(" Version:\n%.*s", len - 4, p + i + 4);
			else
				printf(" Version: (suppressed)");
			break;
		case 0x06:
			printf(" Platform: '%.*s'", len - 4, p + i + 4);
			break;
		case 0x07:
			if (cdp_print_prefixes(p + i + 4, len - 4) < 0)
				goto trunc;
			break;
		case 0x09:		/* guess - not documented */
			printf(" VTP Management Domain: '%.*s'", len - 4,
			    p + i + 4);
			break;
		case 0x0a:		/* guess - not documented */
			printf(" Native VLAN ID: %d",
			    (p[i + 4] << 8) + p[i + 4 + 1] - 1);
			break;
		case 0x0b:		/* guess - not documented */
			printf(" Duplex: %s", p[i + 4] ? "full": "half");
			break;
		default:
			printf(" unknown field type %02x, len %d", type, len);
			break;
		}

		/* avoid infinite loop */
		if (len == 0)
			break;
		i += len;
	}

	return;

trunc:
	printf("[|cdp]");
}

static int
cdp_print_addr(const u_char * p, int l)
{
	int pl, al, num;
	const u_char *endp = p + l;

	num = (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
	p += 4;

	printf(" (%d): ", num);

	while (p < endp && num >= 0) {
		if (p + 2 > endp)
			goto trunc;
		pl = p[1];
		p += 2;

		/* special case: IPv4, protocol type=0xcc, addr. length=4 */
		if (p + 3 > endp)
			goto trunc;
		if (pl == 1 && *p == 0xcc && p[1] == 0 && p[2] == 4) {
			p += 3;

			if (p + 4 > endp)
				goto trunc;
			printf("IPv4 %u.%u.%u.%u ", p[0], p[1], p[2], p[3]);
			p += 4;
		} else {	/* generic case: just print raw data */
			if (p + pl > endp)
				goto trunc;
			printf("pt=0x%02x, pl=%d, pb=", *(p - 2), pl);
			while (pl-- > 0)
				printf(" %02x", *p++);
			al = (*p << 8) + *(p + 1);
			if (p + 2 + al > endp)
				goto trunc;
			printf(", al=%d, a=", al);
			p += 2;
			while (al-- > 0)
				printf(" %02x", *p++);
		}
		printf("  ");
		num--;
	}

	return 0;

trunc:
	return -1;
}


static int
cdp_print_prefixes(const u_char * p, int l)
{
	if (l % 5)
		goto trunc;

	printf(" IPv4 Prefixes (%d):", l / 5);

	while (l > 0) {
		printf(" %u.%u.%u.%u/%u", p[0], p[1], p[2], p[3], p[4]);
		l -= 5;
		p += 5;
	}

	return 0;

trunc:
	return -1;
}
