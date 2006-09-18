/*	$NetBSD: main.c,v 1.22 2006/09/18 19:46:21 christos Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.2 (Berkeley) 4/20/95";
#else
__RCSID("$NetBSD: main.c,v 1.22 2006/09/18 19:46:21 christos Exp $");
#endif
#endif /* not lint */

#define EXTERN
#include "rcv.h"
#undef EXTERN

#include "extern.h"

#ifdef USE_READLINE
#include "complete.h"
#endif

int	main(int, char **);

/*
 * Mail -- a mail program
 *
 * Startup -- interface with user.
 */

jmp_buf	hdrjmp;


/*
 * Break up a white-space or comma delimited name list so that aliases
 * can get expanded.  Without this, the CC: or BCC: list is broken too
 * late for alias expansion to occur.
 */
struct name *
lexpand(char *str, int ntype)
{
	char *list;
	struct name *np = NULL;
	char *word, *p;

	list = strdup(str);
	if (list == NULL)
		err(EXIT_FAILURE, "strdup failed: %s", str);

	word = list;
	for (word = list ; *word ; word = p) {
		while (*word == ' ' || *word == '\t')
			continue;
		for (p = word;
		     *p && *p != ' ' && *p != '\t' && *p != ',';
		      p++ )
			continue;
		if (*p)
			*p++ = '\0';
		np = cat(np, nalloc(word, ntype));
	}

	free(list);
	return np;
}

int
main(int argc, char *argv[])
{
	int i;
	struct name *to, *cc, *bcc, *smopts;
	char *subject;
	const char *ef;
	char nosrc = 0;
	sig_t prevint;
	const char *rc;

	/*
	 * Set up a reasonable environment.
	 * Figure out whether we are being run interactively,
	 * start the SIGCHLD catcher, and so forth.
	 */
	(void)signal(SIGCHLD, sigchild);
	if (isatty(0))
		assign("interactive", "");
	image = -1;
	/*
	 * Now, determine how we are being used.
	 * We successively pick off - flags.
	 * If there is anything left, it is the base of the list
	 * of users to mail to.  Argp will be set to point to the
	 * first of these users.
	 */
	ef = NULL;
	to = NULL;
	cc = NULL;
	bcc = NULL;
	smopts = NULL;
	subject = NULL;
	Bflag = 0;
	while ((i = getopt(argc, argv, "~BEINT:b:c:dfins:u:v")) != -1)
	{
		switch (i) {
		case 'T':
			/*
			 * Next argument is temp file to write which
			 * articles have been read/deleted for netnews.
			 */
			Tflag = optarg;
			if ((i = creat(Tflag, 0600)) < 0) {
				warn("%s", Tflag);
				exit(1);
			}
			(void)close(i);
			break;
		case 'u':
			/*
			 * Next argument is person to pretend to be.
			 */
			myname = optarg;
			(void)unsetenv("MAIL");
			break;
		case 'i':
			/*
			 * User wants to ignore interrupts.
			 * Set the variable "ignore"
			 */
			assign("ignore", "");
			break;
		case 'd':
			debug++;
			break;
		case 's':
			/*
			 * Give a subject field for sending from
			 * non terminal
			 */
			subject = optarg;
			break;
		case 'f':
			/*
			 * User is specifying file to "edit" with Mail,
			 * as opposed to reading system mailbox.
			 * If no argument is given after -f, we read his
			 * mbox file.
			 *
			 * getopt() can't handle optional arguments, so here
			 * is an ugly hack to get around it.
			 */
			if ((argv[optind]) && (argv[optind][0] != '-'))
				ef = argv[optind++];
			else
				ef = "&";
			break;
		case 'n':
			/*
			 * User doesn't want to source /usr/lib/Mail.rc
			 */
			nosrc++;
			break;
		case 'N':
			/*
			 * Avoid initial header printing.
			 */
			assign("noheader", "");
			break;
		case 'v':
			/*
			 * Send mailer verbose flag
			 */
			assign("verbose", "");
			break;
		case 'I':
		case '~':
			/*
			 * We're interactive
			 */
			assign("interactive", "");
			break;
		case 'c':
			/*
			 * Get Carbon Copy Recipient list
			 */
			cc = cat(cc, lexpand(optarg, GCC));
			break;
		case 'b':
			/*
			 * Get Blind Carbon Copy Recipient list
			 */
			bcc = cat(bcc, lexpand(optarg, GBCC));

			break;
		case 'B':
			/*
			 * Suppress the output of the "To:" line to allow
			 * sendmail to apply the NoRecipientAction option.
			 */
			Bflag = 1;
			break;
		case 'E':
			/*
			 * Don't send empty files.
			 */
			assign("dontsendempty", "");
			break;
		case '?':
			(void)fputs("\
Usage: mail [-EiInv] [-s subject] [-c cc-addr] [-b bcc-addr] to-addr ...\n\
            [- sendmail-options ...]\n\
       mail [-EiInNv] -f [name]\n\
       mail [-EiInNv] [-u user]\n",
				stderr);
			exit(1);
		}
	}
	for (i = optind; (argv[i]) && (*argv[i] != '-'); i++)
		to = cat(to, nalloc(argv[i], GTO));
	for (; argv[i]; i++)
		smopts = cat(smopts, nalloc(argv[i], GSMOPTS));
	/*
	 * Check for inconsistent arguments.
	 */
	if (to == NULL && (subject != NULL || cc != NULL || bcc != NULL))
		errx(1, "You must specify direct recipients with -s, -c, or -b.");
	if (ef != NULL && to != NULL) {
		errx(1, "Cannot give -f and people to send to.");
	}
	tinit();
	setscreensize();
	input = stdin;
	rcvmode = !to;
	spreserve();
	if (!nosrc)
		load(_PATH_MASTER_RC);
	/*
	 * Expand returns a savestr, but load only uses the file name
	 * for fopen, so it's safe to do this.
	 */
	if ((rc = getenv("MAILRC")) == 0)
		rc = "~/.mailrc";
	load(expand(rc));

#ifdef USE_READLINE
	/* this is after loading the MAILRC so we can use value() */
	init_readline();
#endif

	if (!rcvmode) {
		(void)mail(to, cc, bcc, smopts, subject);
		/*
		 * why wait?
		 */
		exit(senderr);
	}
	/*
	 * Ok, we are reading mail.
	 * Decide whether we are editing a mailbox or reading
	 * the system mailbox, and open up the right stuff.
	 */
	if (ef == NULL)
		ef = "%";
	if (setfile(ef) < 0)
		exit(1);		/* error already reported */
	if (setjmp(hdrjmp) == 0) {
		if ((prevint = signal(SIGINT, SIG_IGN)) != SIG_IGN)
			(void)signal(SIGINT, hdrstop);
		if (value("quiet") == NULL)
			(void)printf("Mail version %s.  Type ? for help.\n",
				version);
		announce();
		(void)fflush(stdout);
		(void)signal(SIGINT, prevint);
	}
	commands();
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	quit();
	return 0;
}

/*
 * Interrupt printing of the headers.
 */
void
/*ARGSUSED*/
hdrstop(int signo)
{

	(void)fflush(stdout);
	(void)fprintf(stderr, "\nInterrupt\n");
	longjmp(hdrjmp, 1);
}

/*
 * Compute what the screen size for printing headers should be.
 * We use the following algorithm for the height:
 *	If baud rate < 1200, use  9
 *	If baud rate = 1200, use 14
 *	If baud rate > 1200, use 24 or ws_row
 * Width is either 80 or ws_col;
 */
void
setscreensize(void)
{
	struct termios tbuf;
	struct winsize ws;
	speed_t ospeed;

	if (ioctl(1, TIOCGWINSZ, &ws) < 0)
		ws.ws_col = ws.ws_row = 0;
	if (tcgetattr(1, &tbuf) < 0)
		ospeed = 9600;
	else
		ospeed = cfgetospeed(&tbuf);
	if (ospeed < 1200)
		screenheight = 9;
	else if (ospeed == 1200)
		screenheight = 14;
	else if (ws.ws_row != 0)
		screenheight = ws.ws_row;
	else
		screenheight = 24;
	if ((realscreenheight = ws.ws_row) == 0)
		realscreenheight = 24;
	if ((screenwidth = ws.ws_col) == 0)
		screenwidth = 80;
}
