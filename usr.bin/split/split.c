/*	$NetBSD: split.c,v 1.11 2003/06/10 16:57:05 bjh21 Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
__COPYRIGHT("@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)split.c	8.3 (Berkeley) 4/25/94";
#endif
__RCSID("$NetBSD: split.c,v 1.11 2003/06/10 16:57:05 bjh21 Exp $");
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFLINE	1000		/* Default num lines per file. */

static int file_open;		/* If a file open. */
static int ifd = -1, ofd = -1;	/* Input/output file descriptors. */
static char fname[MAXPATHLEN];	/* File name prefix. */
static int sfxlen = 2;		/* suffix length. */

int  main(int, char **);
static void newfile(void);
static void split1(unsigned long long);
static void split2(unsigned long long);
static void usage(void) __attribute__((__noreturn__));
static unsigned long long bigwrite __P((int, const void *, unsigned long long));

int
main(int argc, char *argv[])
{
	int ch;
	char *ep, *p;
	unsigned long long bytecnt = 0;	/* Byte count to split on. */
	unsigned long long numlines = 0;/* Line count to split on. */

	while ((ch = getopt(argc, argv, "-0123456789b:l:a:")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * Undocumented kludge: split was originally designed
			 * to take a number after a dash.
			 */
			if (numlines == 0) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					numlines = strtol(++p, &ep, 10);
				else
					numlines =
					    strtol(argv[optind] + 1, &ep, 10);
				if (numlines <= 0 || *ep)
					errx(1,
					    "%s: illegal line count.", optarg);
			}
			break;
		case '-':		/* stdin flag. */
			if (ifd != -1)
				usage();
			ifd = 0;
			break;
		case 'b':		/* Byte count. */
			if ((bytecnt = strtoull(optarg, &ep, 10)) <= 0 ||
			    (*ep != '\0' && *ep != 'k' && *ep != 'm'))
				errx(1, "%s: illegal byte count.", optarg);
			if (*ep == 'k')
				bytecnt *= 1024;
			else if (*ep == 'm')
				bytecnt *= 1024 * 1024;
			break;
		case 'l':		/* Line count. */
			if (numlines != 0)
				usage();
			if ((numlines = strtoull(optarg, &ep, 10)) <= 0 || *ep)
				errx(1, "%s: illegal line count.", optarg);
			break;
		case 'a': /* Suffix length. */
			if ((sfxlen = strtol(optarg, &ep, 10)) <= 0 || *ep)
				errx(1, "%s: illegal suffix length.", optarg);
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (*argv != NULL)
		if (ifd == -1) {		/* Input file. */
			if (strcmp(*argv, "-") == 0)
				ifd = STDIN_FILENO;
			else if ((ifd = open(*argv, O_RDONLY, 0)) < 0)
				err(1, "%s", *argv);
			++argv;
		}

	if (*argv != NULL) {
		if (strlen(*argv) + sfxlen > NAME_MAX)
			errx(EXIT_FAILURE, "Output file name too long");
		(void)strcpy(fname, *argv++);		/* File name prefix. */
	} else {
		if (1 + sfxlen > NAME_MAX)
			errx(EXIT_FAILURE, "Output file name too long");
	}

	if (*argv != NULL)
		usage();

	if (numlines == 0)
		numlines = DEFLINE;
	else if (bytecnt)
		usage();

	if (ifd == -1)				/* Stdin by default. */
		ifd = 0;

	if (bytecnt) {
		split1(bytecnt);
	} else {
		split2(numlines);
	}
	return 0;
}

/*
 * split1 --
 *	Split the input by bytes.
 */
static void
split1(unsigned long long bytecnt)
{
	unsigned long long bcnt, dist;
	ssize_t len;
	char *C;
	char bfr[MAXBSIZE];

	for (bcnt = 0;;)
		switch (len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(0);
		case -1:
			err(1, "read");
			/* NOTREACHED */
		default:
			if (!file_open) {
				newfile();
				file_open = 1;
			}
			if (bcnt + len >= bytecnt) {
				dist = bytecnt - bcnt;
				if (bigwrite(ofd, bfr, dist) != dist)
					err(1, "write");
				len -= dist;
				for (C = bfr + dist; len >= bytecnt;
				    len -= bytecnt, C += bytecnt) {
					newfile();
					if (bigwrite(ofd,
					    C, (int)bytecnt) != bytecnt)
						err(1, "write");
				}
				if (len) {
					newfile();
					if (bigwrite(ofd, C, len) != len)
						err(1, "write");
				} else
					file_open = 0;
				bcnt = len;
			} else {
				bcnt += len;
				if (bigwrite(ofd, bfr, len) != len)
					err(1, "write");
			}
		}
}

/*
 * split2 --
 *	Split the input by lines.
 */
static void
split2(unsigned long long numlines)
{
	unsigned long long lcnt, bcnt;
	ssize_t len;
	char *Ce, *Cs;
	char bfr[MAXBSIZE];

	for (lcnt = 0;;)
		switch (len = read(ifd, bfr, MAXBSIZE)) {
		case 0:
			exit(0);
		case -1:
			err(1, "read");
			/* NOTREACHED */
		default:
			if (!file_open) {
				newfile();
				file_open = 1;
			}
			for (Cs = Ce = bfr; len--; Ce++)
				if (*Ce == '\n' && ++lcnt == numlines) {
					bcnt = Ce - Cs + 1;
					if (bigwrite(ofd, Cs, bcnt) != bcnt)
						err(1, "write");
					lcnt = 0;
					Cs = Ce + 1;
					if (len)
						newfile();
					else
						file_open = 0;
				}
			if (Cs < Ce) {
				bcnt = Ce - Cs;
				if (bigwrite(ofd, Cs, bcnt) != bcnt)
					err(1, "write");
			}
		}
}

/*
 * newfile --
 *	Open a new output file.
 */
static void
newfile(void)
{
	static int fnum;
	static int defname;
	static char *fpnt;
	int quot, i;

	if (ofd == -1) {
		if (fname[0] == '\0') {
			fname[0] = 'x';
			fpnt = fname + 1;
			defname = 1;
		} else {
			fpnt = fname + strlen(fname);
			defname = 0;
		}
		ofd = fileno(stdout);
	}
	/*
	 * Hack to increase max files; original code wandered through
	 * magic characters.  Maximum files is 3 * 26 * 26 == 2028
	 */
	fpnt[sfxlen] = '\0';
	quot = fnum;
	for (i = sfxlen - 1; i >= 0; i--) {
		fpnt[i] = quot % 26 + 'a';
		quot = quot / 26;
	}
	if (quot > 0) {
		if (!defname || fname[0] == 'z')
			errx(1, "too many files.");
		++fname[0];
		fnum = 0;
	}
	++fnum;
	if (!freopen(fname, "w", stdout))
		err(1, "%s", fname);
}

static unsigned long long
bigwrite(int fd, const void *buf, unsigned long long len)
{
	const char *ptr = buf;
	unsigned long long sofar = 0;

	while (len != 0) {
		ssize_t w, nw = (len > INT_MAX) ? INT_MAX : (ssize_t)len;
		if  ((w = write(fd, ptr, nw)) == -1)
			return sofar;
		len -= w;
		ptr += w;
		sofar += w;
	}
	return sofar;
}


static void
usage(void)
{
	(void)fprintf(stderr,
"Usage: %s [-b byte_count] [-l line_count] [-a suffix_length] "
"[file [prefix]]\n", getprogname());
	exit(1);
}
