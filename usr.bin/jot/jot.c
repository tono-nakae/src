/*	$NetBSD: jot.c,v 1.20 2008/02/27 22:47:32 dsl Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)jot.c	8.1 (Berkeley) 6/6/93";
#endif
__RCSID("$NetBSD: jot.c,v 1.20 2008/02/27 22:47:32 dsl Exp $");
#endif /* not lint */

/*
 * jot - print sequential or random data
 *
 * Author:  John Kunze, Office of Comp. Affairs, UCB
 */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define	REPS_DEF	100
#define	BEGIN_DEF	1
#define	ENDER_DEF	100
#define	STEP_DEF	1

#define	is_default(s)	(strcmp((s), "-") == 0)

double	begin;
double	ender;
double	step;
long	reps;
int	randomize;
int	infinity;
int	boring;
int	prec = -1;
int	dox;
int	chardata;
int	nofinalnl;
const char *sepstring = "\n";
char	format[BUFSIZ];

static void	getargs(int, char *[]);
static void	getformat(void);
static int	getprec(char *);
static void	putdata(double, long);
static void	usage(void) __dead;

int
main(int argc, char *argv[])
{
	double	x, y;
	long	i;

	getargs(argc, argv);
	if (randomize) {
		x = (ender + dox - begin) * (ender > begin ? 1 : -1);
		srandom((unsigned long) step);
		for (i = 1; i <= reps || infinity; i++) {
			y = (double) random() / INT_MAX;
			putdata(y * x + begin, reps - i);
		}
	} else {
		for (i = 1, x = begin; i <= reps || infinity; i++, x += step)
			putdata(x, reps - i);
	}
	if (!nofinalnl)
		putchar('\n');
	exit(0);
}

static void
getargs(int argc, char *argv[])
{
	unsigned int	mask = 0;
	int		n = 0;
	char *ep;

	for (;;) {
		switch (getopt(argc, argv, "b:cnp:rs:w:")) {
		default:
			usage();
		case -1:
			break;
		case 'c':
			chardata = 1;
			continue;
		case 'n':
			nofinalnl = 1;
			continue;
		case 'p':
			prec = strtol(optarg, &ep, 0);
			if (*ep != 0 || prec < 0)
				errx(EXIT_FAILURE, "Bad precision value");
			continue;
		case 'r':
			randomize = 1;
			continue;
		case 's':
			sepstring = optarg;
			continue;
		case 'b':
			boring = 1;
			/* FALLTHROUGH */
		case 'w':
			strlcpy(format, optarg, sizeof(format));
			continue;
		}
		break;
	}
	argc -= optind;
	argv += optind;

	switch (argc) {	/* examine args right to left, falling thru cases */
	case 4:
		if (!is_default(argv[3])) {
			if (!sscanf(argv[3], "%lf", &step))
				errx(EXIT_FAILURE, "Bad step value:  %s",
				    argv[3]);
			mask |= 01;
		}
	case 3:
		if (!is_default(argv[2])) {
			if (!sscanf(argv[2], "%lf", &ender))
				ender = argv[2][strlen(argv[2])-1];
			mask |= 02;
			if (prec < 0)
				n = getprec(argv[2]);
		}
	case 2:
		if (!is_default(argv[1])) {
			if (!sscanf(argv[1], "%lf", &begin))
				begin = argv[1][strlen(argv[1])-1];
			mask |= 04;
			if (prec < 0)
				prec = getprec(argv[1]);
			if (n > prec)		/* maximum precision */
				prec = n;
		}
	case 1:
		if (!is_default(argv[0])) {
			if (!sscanf(argv[0], "%ld", &reps))
				errx(EXIT_FAILURE, "Bad reps value:  %s",
				    argv[0]);
			mask |= 010;
		}
		break;
	case 0:
		usage();
		break;
	default:
		errx(EXIT_FAILURE,
		    "Too many arguments.  What do you mean by %s?", argv[4]);
	}
	getformat();
	while (mask) {	/* 4 bit mask has 1's where last 4 args were given */
		switch (mask) {	/* fill in the 0's by default or computation */
		case 001:
			reps = REPS_DEF;
			mask = 011;
			break;
		case 002:
			reps = REPS_DEF;
			mask = 012;
			break;
		case 003:
			reps = REPS_DEF;
			mask = 013;
			break;
		case 004:
			reps = REPS_DEF;
			mask = 014;
			break;
		case 005:
			reps = REPS_DEF;
			mask = 015;
			break;
		case 006:
			reps = REPS_DEF;
			mask = 016;
			break;
		case 007:
			if (randomize) {
				reps = REPS_DEF;
				mask = 0;
				break;
			}
			if (step == 0.0) {
				reps = 0;
				mask = 0;
				break;
			}
			reps = (ender - begin + step) / step;
			if (reps <= 0)
				errx(EXIT_FAILURE, "Impossible stepsize");
			mask = 0;
			break;
		case 010:
			begin = BEGIN_DEF;
			mask = 014;
			break;
		case 011:
			begin = BEGIN_DEF;
			mask = 015;
			break;
		case 012:
			step = (randomize ? time(NULL) * getpid() : STEP_DEF);
			mask = 013;
			break;
		case 013:
			if (randomize)
				begin = BEGIN_DEF;
			else if (reps == 0)
				errx(EXIT_FAILURE,
				    "Must specify begin if reps == 0");
			begin = ender - reps * step + step;
			mask = 0;
			break;
		case 014:
			step = (randomize ? time(NULL) * getpid() : STEP_DEF);
			mask = 015;
			break;
		case 015:
			if (randomize)
				ender = ENDER_DEF;
			else
				ender = begin + reps * step - step;
			mask = 0;
			break;
		case 016:
			if (randomize)
				step = time(NULL) * getpid();
			else if (reps == 0)
				errx(EXIT_FAILURE,
				    "Infinite sequences cannot be bounded");
			else if (reps == 1)
				step = 0.0;
			else
				step = (ender - begin) / (reps - 1);
			mask = 0;
			break;
		case 017:		/* if reps given and implied, */
			if (!randomize && step != 0.0) {
				long t = (ender - begin + step) / step;
				if (t <= 0)
					errx(EXIT_FAILURE,
					    "Impossible stepsize");
				if (t < reps)		/* take lesser */
					reps = t;
			}
			mask = 0;
			break;
		default:
			errx(EXIT_FAILURE, "bad mask");
		}
	}
	if (reps == 0)
		infinity = 1;

	if (prec == -1)
		prec = 0;
}

static void
putdata(double x, long notlast)
{
	long	d = floor(x);

	if (boring)				/* repeated word */
		printf("%s", format);
	else if (dox)				/* scalar */
		printf(format, d);
	else					/* real */
		printf(format, x);
	if (notlast != 0)
		fputs(sepstring, stdout);
}

__dead static void
usage(void)
{
	(void)fprintf(stderr, "usage: %s [-cnr] [-b word] [-p precision] "
	    "[-s string] [-w word] [reps [begin [end [step | seed]]]]\n",
	    getprogname());
	exit(1);
}

static int
getprec(char *num_str)
{

	num_str = strchr(num_str, '.');
	if (num_str == NULL)
		return 0;
	return strspn(num_str + 1, "0123456789");
}

static void
getformat(void)
{
	char	*p;
	size_t	sz;

	if (boring)				/* no need to bother */
		return;
	for (p = format; *p; p++) {		/* look for '%' */
		if (*p == '%') {
			if (*(p+1) != '%')
				break;
			p++;		/* leave %% alone */
		}
	}
	sz = sizeof(format) - strlen(format) - 1;
	if (!*p) {
		if (chardata || prec == 0) {
			if (snprintf(p, sz, "%%%s", chardata ? "c" : "ld") >= sz)
				errx(EXIT_FAILURE, "-w word too long");
			dox = 1;
		} else {
			if (snprintf(p, sz, "%%.%df", prec) >= (int)sz)
				errx(EXIT_FAILURE, "-w word too long");
		}
	} else if (!*(p+1)) {
		if (sz <= 0)
			errx(EXIT_FAILURE, "-w word too long");
		strcat(format, "%");		/* cannot end in single '%' */
	} else {
		p++;				/* skip leading % */
		for(; *p && !isalpha((unsigned char)*p); p++) {
			/* allow all valid printf(3) flags, but deny '*' */
			if (!strchr("0123456789#-+. ", *p))
				break;
		}
		/* Allow 'l' prefix, but no other. */
		if (*p == 'l')
			p++;
		switch (*p) {
		case 'f': case 'e': case 'g': case '%':
		case 'E': case 'G':
			break;
		case 's':
			errx(EXIT_FAILURE,
			    "cannot convert numeric data to strings");
			break;
		case 'd': case 'o': case 'x': case 'u':
		case 'D': case 'O': case 'X': case 'U':
		case 'c': case 'i':
			dox = 1;
			break;
		default:
			errx(EXIT_FAILURE, "unknown or invalid format `%s'",
			    format);
		}
		/* Need to check for trailing stuff to print */
		for (; *p; p++)		/* look for '%' */
			if (*p == '%') {
				if (*(p+1) != '%')
					break;
				p++;		/* leave %% alone */
			}
		if (*p)
			errx(EXIT_FAILURE, "unknown or invalid format `%s'",
			    format);
	}
}
