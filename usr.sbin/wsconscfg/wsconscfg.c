/* $NetBSD: wsconscfg.c,v 1.1 1999/01/13 17:15:44 drochner Exp $ */

/*
 * Copyright (c) 1999
 *	Matthias Drochner.  All rights reserved.
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
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <err.h>

#include <dev/wscons/wsconsio.h>

#define DEFDEV "/dev/ttyEcfg"

static void usage __P((void));
int main __P((int, char**));

static void
usage()
{
	extern char *__progname;

	(void)fprintf(stderr,
		"Usage: %s [-f wsdev] [-d [-F]] [-t type] [-e emul] vt\n",
		      __progname);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *wsdev;
	int c, delete, idx, wsfd, res;
	struct wsdisplay_addscreendata asd;
	struct wsdisplay_delscreendata dsd;

	wsdev = DEFDEV;
	delete = 0;
	asd.screentype = 0;
	asd.emul = 0;
	dsd.flags = 0;

	while ((c = getopt(argc, argv, "f:dt:e:F")) != -1) {
		switch (c) {
		case 'f':
			wsdev = optarg;
			break;
		case 'd':
			delete++;
			break;
		case 't':
			asd.screentype = optarg;
			break;
		case 'e':
			asd.emul = optarg;
			break;
		case 'F':
			dsd.flags |= WSDISPLAY_DELSCR_FORCE;
			break;
		case '?':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	if (sscanf(argv[0], "%d", &idx) != 1)
		errx(1, "invalid index");

	wsfd = open(wsdev, O_RDWR, 0);
	if (wsfd < 0)
		err(2, "open ws");

	if (delete) {
		dsd.idx = idx;
		res = ioctl(wsfd, WSDISPLAYIO_DELSCREEN, &dsd);
		if (res < 0)
			err(3, "WSDISPLAYIO_DELSCREEN");
	} else {
		asd.idx = idx;
		res = ioctl(wsfd, WSDISPLAYIO_ADDSCREEN, &asd);
		if (res < 0)
			err(3, "WSDISPLAYIO_ADDSCREEN");
	}

	return (0);
}
