/*	$NetBSD: s_scalbln.c,v 1.3 2013/04/27 16:43:13 joerg Exp $	*/

/*-
 * Copyright (c) 2004 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#if 0
__FBSDID("$FreeBSD: src/lib/msun/src/s_scalbln.c,v 1.2 2005/03/07 04:57:50 das Exp $");
#else
__RCSID("$NetBSD: s_scalbln.c,v 1.3 2013/04/27 16:43:13 joerg Exp $");
#endif

#include "namespace.h"

#include <limits.h>
#include <math.h>

__weak_alias(scalbln, _scalbln)
double
scalbln (double x, long n)
{
	int in;

	in = (int)n;
	if (in != n) {
		if (n > 0)
			in = INT_MAX;
		else
			in = INT_MIN;
	}
	return (scalbn(x, in));
}

__weak_alias(scalblnf, _scalblnf)
float
scalblnf (float x, long n)
{
	int in;

	in = (int)n;
	if (in != n) {
		if (n > 0)
			in = INT_MAX;
		else
			in = INT_MIN;
	}
	return (scalbnf(x, in));
}

#ifndef _LP64
__weak_alias(scalblnl, _scalblnl)

long double
scalblnl (long double x, long n)
{
	int in;

	in = (int)n;
	if (in != n) {
		if (n > 0)
			in = INT_MAX;
		else
			in = INT_MIN;
	}
	return (scalbnl(x, (int)n));
}
#endif
