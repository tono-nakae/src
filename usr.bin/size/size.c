/*
 * Copyright (c) 1988 Regents of the University of California.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1988 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)size.c	4.7 (Berkeley) 6/1/90";*/
static char rcsid[] = "$Id: size.c,v 1.3 1993/08/01 18:08:26 mycroft Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <a.out.h>
#include <stdio.h>

int exval;

main(int argc, char *argv[])
{
	if (argc < 2) {
		sizeit("a.out", 0);
		exit(exval);
	}

	for (++argv; *argv; ++argv)
		sizeit(*argv, argc > 2);
	exit(exval);
}

sizeit(char *name, int doname)
{
	static int first = 1;
	struct exec head;
	u_long total;
	int fd;

	if ((fd = open(name, O_RDONLY, 0)) < 0) {
		fprintf(stderr, "size: ");
		perror(name);
		exval = 1;
		return;
	}
	if (read(fd, (char *)&head, sizeof(head)) != sizeof(head) ||
	    N_BADMAG(head)) {
		fprintf(stderr, "size: %s: not in a.out format.\n", name);
		exval = 1;
		return;
	}
	(void)close(fd);
	if (first) {
		first = 0;
		printf("text\tdata\tbss\tdec\thex\n");
	}
	total = head.a_text + head.a_data + head.a_bss;
	printf("%lu\t%lu\t%lu\t%lu\t%lx", head.a_text, head.a_data,
	    head.a_bss, total, total);
	if (doname)
		printf("\t%s", name);
	printf("\n");
}
