/*	$NetBSD: h_tools.c,v 1.3 2006/03/18 17:09:35 jmmv Exp $	*/

/*
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Julio M. Merino Vidal, developed as part of Google's Summer of Code
 * 2005 program.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Helper tools for several tests.  These are kept in a single file due
 * to the limitations of bsd.prog.mk to build a single program in a
 * given directory.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* --------------------------------------------------------------------- */

static int getfh_main(int, char **);
static int rename_main(int, char **);
static int sockets_main(int, char **);
static int statvfs_main(int, char **);

/* --------------------------------------------------------------------- */

int
getfh_main(int argc, char **argv)
{
	int error;
	fhandle_t fh;

	if (argc < 2)
		return EXIT_FAILURE;

	error = getfh(argv[1], &fh);
	if (error != 0) {
		perror("getfh");
		return EXIT_FAILURE;
	}

	error = write(STDOUT_FILENO, &fh, sizeof(fh));
	if (error == -1) {
		perror("write");
		return EXIT_FAILURE;
	}

	return 0;
}

/* --------------------------------------------------------------------- */

int
rename_main(int argc, char **argv)
{

	if (argc < 3)
		return EXIT_FAILURE;

	return rename(argv[1], argv[2]);
}

/* --------------------------------------------------------------------- */

int
sockets_main(int argc, char **argv)
{
	int error, fd;
	struct sockaddr_un addr;

	if (argc < 2)
		return EXIT_FAILURE;

	fd = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (fd == -1) {
		perror("socket");
		return EXIT_FAILURE;
	}

	(void)strlcpy(addr.sun_path, argv[1], sizeof(addr.sun_path));
	addr.sun_family = PF_UNIX;

	error = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (error == -1) {
		perror("connect");
		return EXIT_FAILURE;
	}

	close(fd);

	return EXIT_SUCCESS;
}

/* --------------------------------------------------------------------- */

int
statvfs_main(int argc, char **argv)
{
	int error;
	struct statvfs buf;

	if (argc < 2)
		return EXIT_FAILURE;

	error = statvfs(argv[1], &buf);
	if (error != 0) {
		perror("statvfs");
		return EXIT_FAILURE;
	}

	(void)printf("f_bsize=%lu\n", buf.f_bsize);
	(void)printf("f_blocks=%" PRId64 "\n", buf.f_blocks);
	(void)printf("f_bfree=%" PRId64 "\n", buf.f_bfree);
	(void)printf("f_files=%" PRId64 "\n", buf.f_files);

	return EXIT_SUCCESS;
}

/* --------------------------------------------------------------------- */

int
main(int argc, char **argv)
{
	int error;

	if (argc < 2)
		return EXIT_FAILURE;

	argc -= 1;
	argv += 1;

	if (strcmp(argv[0], "getfh") == 0)
		error = getfh_main(argc, argv);
	else if (strcmp(argv[0], "rename") == 0)
		error = rename_main(argc, argv);
	else if (strcmp(argv[0], "sockets") == 0)
		error = sockets_main(argc, argv);
	else if (strcmp(argv[0], "statvfs") == 0)
		error = statvfs_main(argc, argv);
	else
		error = EXIT_FAILURE;

	return error;
}
