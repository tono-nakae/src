/*	$NetBSD: getportproto.c,v 1.7 2010/04/17 21:00:09 darrenr Exp $	*/

/*
 * Copyright (C) 2002-2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: getportproto.c,v 1.2.4.5 2009/12/27 06:58:06 darrenr Exp
 */

#include <ctype.h>
#include "ipf.h"

int getportproto(name, proto)
char *name;
int proto;
{
	struct servent *s;
	struct protoent *p;

	if (ISDIGIT(*name)) {
		int number;
		char *s;

		for (s = name; *s != '\0'; s++)
			if (!ISDIGIT(*s))
				return -1;

		number = atoi(name);
		if (number < 0 || number > 65535)
			return -1;
		return htons(number);
	}

	p = getprotobynumber(proto);
	s = getservbyname(name, p ? p->p_name : NULL);
	if (s != NULL)
		return s->s_port;
	return -1;
}
