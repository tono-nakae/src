/*	$NetBSD: kdump.c,v 1.27 2000/04/10 07:58:30 jdolecek Exp $	*/

/*-
 * Copyright (c) 1988, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)kdump.c	8.4 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: kdump.c,v 1.27 2000/04/10 07:58:30 jdolecek Exp $");
#endif
#endif /* not lint */

#include <sys/param.h>
#define _KERNEL
#include <sys/errno.h>
#undef _KERNEL
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/ktrace.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>

#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "ktrace.h"

int timestamp, decimal, fancy = 1, tail, maxdata;
const char *tracefile = DEF_TRACEFILE;
struct ktr_header ktr_header;

#define eqs(s1, s2)	(strcmp((s1), (s2)) == 0)

#include <sys/syscall.h>

#include "../../sys/compat/netbsd32/netbsd32_syscall.h"
#include "../../sys/compat/freebsd/freebsd_syscall.h"
#include "../../sys/compat/hpux/hpux_syscall.h"
#include "../../sys/compat/ibcs2/ibcs2_syscall.h"
#include "../../sys/compat/linux/linux_syscall.h"
#include "../../sys/compat/osf1/osf1_syscall.h"
#include "../../sys/compat/sunos/sunos_syscall.h"
#include "../../sys/compat/svr4/svr4_syscall.h"
#include "../../sys/compat/ultrix/ultrix_syscall.h"

#define KTRACE
#include "../../sys/kern/syscalls.c"

#include "../../sys/compat/netbsd32/netbsd32_syscalls.c"
#include "../../sys/compat/freebsd/freebsd_syscalls.c"
#include "../../sys/compat/hpux/hpux_syscalls.c"
#include "../../sys/compat/ibcs2/ibcs2_syscalls.c"
#include "../../sys/compat/linux/linux_syscalls.c"
#include "../../sys/compat/osf1/osf1_syscalls.c"
#include "../../sys/compat/sunos/sunos_syscalls.c"
#include "../../sys/compat/svr4/svr4_syscalls.c"
#include "../../sys/compat/ultrix/ultrix_syscalls.c"

#include "../../sys/compat/hpux/hpux_errno.c"
#include "../../sys/compat/svr4/svr4_errno.c"
#include "../../sys/compat/ibcs2/ibcs2_errno.c"
#include "../../sys/compat/linux/common/linux_errno.c"
#undef KTRACE

struct emulation {
	const char *name;	/* Emulation name */
	char **sysnames;	/* Array of system call names */
	int  nsysnames;		/* Number of */
	int  *errno;		/* Array of error number mapping */
	int  nerrno;		/* number of elements in array */
};

struct emulation_ctx {
	pid_t	pid;
	struct emulation *emulation;
	struct emulation_ctx *next;
};

#define NELEM(a) (sizeof(a) / sizeof(a[0]))

static struct emulation emulations[] = {
#define EMUL_NETBSD_IDX 0
	{   "netbsd",	       syscallnames,         SYS_MAXSYSCALL,
	        NULL,		        0 },
	{   "netbsd32", netbsd32_syscallnames,	     SYS_MAXSYSCALL,
	        NULL,		        0 },
	{  "freebsd",  freebsd_syscallnames, FREEBSD_SYS_MAXSYSCALL,
	        NULL,		        0 },
	{     "hpux",	  hpux_syscallnames,    HPUX_SYS_MAXSYSCALL,
	  native_to_hpux_errno,  NELEM(native_to_hpux_errno)  },
	{    "ibcs2",    ibcs2_syscallnames,   IBCS2_SYS_MAXSYSCALL,
	 native_to_ibcs2_errno,  NELEM(native_to_ibcs2_errno) },
	{    "linux",    linux_syscallnames,   LINUX_SYS_MAXSYSCALL,
	 native_to_linux_errno,  NELEM(native_to_linux_errno) },
	{     "osf1",     osf1_syscallnames,    OSF1_SYS_MAXSYSCALL,
	        NULL,		        0 },
	{    "sunos",    sunos_syscallnames,   SUNOS_SYS_MAXSYSCALL,
	        NULL,		        0 },
	{     "svr4",     svr4_syscallnames,    SVR4_SYS_MAXSYSCALL,
	  native_to_svr4_errno,  NELEM(native_to_svr4_errno)  },
	{   "ultrix",   ultrix_syscallnames,  ULTRIX_SYS_MAXSYSCALL,
	        NULL,			0 },
	{       NULL,		       NULL,		          0,
	        NULL,			0 }
};

struct emulation *current, *default_emul;

struct emulation_ctx *current_ctx;
struct emulation_ctx *emul_ctx = NULL;

static const char *ptrace_ops[] = {
	"PT_TRACE_ME",	"PT_READ_I",	"PT_READ_D",	"PT_READ_U",
	"PT_WRITE_I",	"PT_WRITE_D",	"PT_WRITE_U",	"PT_CONTINUE",
	"PT_KILL",	"PT_ATTACH",	"PT_DETACH",
};

int	main __P((int, char **));
int	fread_tail __P((char *, int, int));
void	dumpheader __P((struct ktr_header *));
void	ioctldecode __P((u_long));
void	ktrsyscall __P((struct ktr_syscall *));
void	ktrsysret __P((struct ktr_sysret *));
void	ktrnamei __P((char *, int));
void	ktremul __P((char *, int, int));
void	ktrgenio __P((struct ktr_genio *, int));
void	ktrpsig __P((struct ktr_psig *));
void	ktrcsw __P((struct ktr_csw *));
void	usage __P((void));
void	eprint __P((int));
char	*ioctlname __P((long));

static struct emulation_ctx *ectx_find __P((pid_t));
static void	ectx_update __P((pid_t, struct emulation *));
static void	ectx_sanify __P((pid_t));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, ktrlen, size;
	void *m;
	int trpoints = ALL_POINTS;
	const char *emul_name = "netbsd";

	while ((ch = getopt(argc, argv, "e:f:dlm:nRTt:")) != -1)
		switch (ch) {
		case 'e':
			emul_name = strdup(optarg); /* it's safer to copy it */
			break;
		case 'f':
			tracefile = optarg;
			break;
		case 'd':
			decimal = 1;
			break;
		case 'l':
			tail = 1;
			break;
		case 'm':
			maxdata = atoi(optarg);
			break;
		case 'n':
			fancy = 0;
			break;
		case 'R':
			timestamp = 2;	/* relative timestamp */
			break;
		case 'T':
			timestamp = 1;
			break;
		case 't':
			trpoints = getpoints(optarg);
			if (trpoints < 0)
				errx(1, "unknown trace point in %s", optarg);
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc > 1)
		usage();

	setemul(emul_name, 0, 0);
	default_emul = current;

	m = malloc(size = 1024);
	if (m == NULL)
		errx(1, "malloc: %s", strerror(ENOMEM));
	if (!freopen(tracefile, "r", stdin))
		err(1, "%s", tracefile);
	while (fread_tail((char *)&ktr_header, sizeof(struct ktr_header), 1)) {
		if (trpoints & (1<<ktr_header.ktr_type))
			dumpheader(&ktr_header);
		if ((ktrlen = ktr_header.ktr_len) < 0)
			errx(1, "bogus length 0x%x", ktrlen);
		if (ktrlen > size) {
			while(ktrlen > size) size *= 2;
			m = (void *)realloc(m, size);
			if (m == NULL)
				errx(1, "realloc: %s", strerror(ENOMEM));
		}
		if (ktrlen && fread_tail(m, ktrlen, 1) == 0)
			errx(1, "data too short");
		if ((trpoints & (1<<ktr_header.ktr_type)) == 0)
			continue;

		/* update context to match currently processed record */
		ectx_sanify(ktr_header.ktr_pid);

		switch (ktr_header.ktr_type) {
		case KTR_SYSCALL:
			ktrsyscall((struct ktr_syscall *)m);
			break;
		case KTR_SYSRET:
			ktrsysret((struct ktr_sysret *)m);
			break;
		case KTR_NAMEI:
			ktrnamei(m, ktrlen);
			break;
		case KTR_GENIO:
			ktrgenio((struct ktr_genio *)m, ktrlen);
			break;
		case KTR_PSIG:
			ktrpsig((struct ktr_psig *)m);
			break;
		case KTR_CSW:
			ktrcsw((struct ktr_csw *)m);
			break;
		case KTR_EMUL:
			ktremul(m, ktrlen, size);
			break;
		}
		if (tail)
			(void)fflush(stdout);
	}
	return (0);
}

int
fread_tail(buf, size, num)
	char *buf;
	int num, size;
{
	int i;

	while ((i = fread(buf, size, num, stdin)) == 0 && tail) {
		(void)sleep(1);
		clearerr(stdin);
	}
	return (i);
}

void
dumpheader(kth)
	struct ktr_header *kth;
{
	char unknown[64], *type;
	static struct timeval prevtime;
	struct timeval temp;

	switch (kth->ktr_type) {
	case KTR_SYSCALL:
		type = "CALL";
		break;
	case KTR_SYSRET:
		type = "RET ";
		break;
	case KTR_NAMEI:
		type = "NAMI";
		break;
	case KTR_GENIO:
		type = "GIO ";
		break;
	case KTR_PSIG:
		type = "PSIG";
		break;
	case KTR_CSW:
		type = "CSW";
		break;
	case KTR_EMUL:
		type = "EMUL";
		break;
	default:
		(void)sprintf(unknown, "UNKNOWN(%d)", kth->ktr_type);
		type = unknown;
	}

	(void)printf("%6d %-8.*s ", kth->ktr_pid, MAXCOMLEN, kth->ktr_comm);
	if (timestamp) {
		if (timestamp == 2) {
			timersub(&kth->ktr_time, &prevtime, &temp);
			prevtime = kth->ktr_time;
		} else
			temp = kth->ktr_time;
		(void)printf("%ld.%06ld ",
		    (long int)temp.tv_sec, (long int)temp.tv_usec);
	}
	(void)printf("%s  ", type);
}

void
ioctldecode(cmd)
	u_long cmd;
{
	char dirbuf[4], *dir = dirbuf;

	if (cmd & IOC_IN)
		*dir++ = 'W';
	if (cmd & IOC_OUT)
		*dir++ = 'R';
	*dir = '\0';

	printf(decimal ? ",_IO%s('%c',%ld" : ",_IO%s('%c',%#lx",
	    dirbuf, (cmd >> 8) & 0xff, cmd & 0xff);
	if ((cmd & IOC_VOID) == 0)
		printf(decimal ? ",%ld)" : ",%#lx)", (cmd >> 16) & 0xff);
	else
		printf(")");
}

void
ktrsyscall(ktr)
	struct ktr_syscall *ktr;
{
	int argsize = ktr->ktr_argsize;
	register_t *ap;

	if (ktr->ktr_code >= current->nsysnames || ktr->ktr_code < 0)
		(void)printf("[%d]", ktr->ktr_code);
	else
		(void)printf("%s", current->sysnames[ktr->ktr_code]);
	ap = (register_t *)((char *)ktr + sizeof(struct ktr_syscall));
	if (argsize) {
		char c = '(';
		if (fancy) {
			if (ktr->ktr_code == SYS_ioctl) {
				char *cp;
				if (decimal)
					(void)printf("(%ld", (long)*ap);
				else
					(void)printf("(%#lx", (long)*ap);
				ap++;
				argsize -= sizeof(register_t);
				if ((cp = ioctlname(*ap)) != NULL)
					(void)printf(",%s", cp);
				else
					ioctldecode(*ap);
				c = ',';
				ap++;
				argsize -= sizeof(register_t);
			} else if (ktr->ktr_code == SYS_ptrace) {
				if (*ap >= 0 && *ap <=
				    sizeof(ptrace_ops) / sizeof(ptrace_ops[0]))
					(void)printf("(%s", ptrace_ops[*ap]);
				else
					(void)printf("(%ld", (long)*ap);
				c = ',';
				ap++;
				argsize -= sizeof(register_t);
			}
		}
		while (argsize) {
			if (decimal)
				(void)printf("%c%ld", c, (long)*ap);
			else
				(void)printf("%c%#lx", c, (long)*ap);
			c = ',';
			ap++;
			argsize -= sizeof(register_t);
		}
		(void)putchar(')');
	}
	(void)putchar('\n');
}

void
ktrsysret(ktr)
	struct ktr_sysret *ktr;
{
	register_t ret = ktr->ktr_retval;
	int error = ktr->ktr_error;
	int code = ktr->ktr_code;

	if (code >= current->nsysnames || code < 0)
		(void)printf("[%d] ", code);
	else
		(void)printf("%s ", current->sysnames[code]);

	switch (error) {
	case 0:
		if (fancy) {
			(void)printf("%ld", (long)ret);
			if (ret < 0 || ret > 9)
				(void)printf("/%#lx", (long)ret);
		} else {
			if (decimal)
				(void)printf("%ld", (long)ret);
			else
				(void)printf("%#lx", (long)ret);
		}
		break;

	default:
		eprint(error);
		break;
	}
	(void)putchar('\n');

}

/*
 * We print the original emulation's error numerically, but we
 * translate it to netbsd to print it symbolically.
 */
void
eprint(e)
	int e;
{
	int i = e;

	if (current->errno) {

		/* No remapping for ERESTART and EJUSTRETURN */
		/* Kludge for linux that has negative error numbers */
		if (current->errno[2] > 0 && e < 0)
			goto normal;

		for (i = 0; i < current->nerrno; i++)
			if (e == current->errno[i])
				break;

		if (i == current->nerrno) {
			printf("-1 unknown errno %d", e);
			return;
		}
	}

normal:
	switch (i) {
	case ERESTART:
		(void)printf("RESTART");
		break;

	case EJUSTRETURN:
		(void)printf("JUSTRETURN");
		break;

	default:
		(void)printf("-1 errno %d", e);
		if (fancy)
			(void)printf(" %s", strerror(i));
	}
}

void
ktrnamei(cp, len)
	char *cp;
	int len;
{

	(void)printf("\"%.*s\"\n", len, cp);
}

void
ktremul(name, len, bufsize)
	char *name;
	int len, bufsize;
{
	if (len >= bufsize)
		len = bufsize - 1;

	name[len] = '\0';
	setemul(name, ktr_header.ktr_pid, 1);

	(void)printf("\"%s\"\n", name);
}

void
ktrgenio(ktr, len)
	struct ktr_genio *ktr;
	int len;
{
	int datalen = len - sizeof (struct ktr_genio);
	char *dp = (char *)ktr + sizeof (struct ktr_genio);
	char *cp;
	int col = 0;
	int width;
	char visbuf[5];
	static int screenwidth = 0;

	if (screenwidth == 0) {
		struct winsize ws;

		if (fancy && ioctl(fileno(stderr), TIOCGWINSZ, &ws) != -1 &&
		    ws.ws_col > 8)
			screenwidth = ws.ws_col;
		else
			screenwidth = 80;
	}
	printf("fd %d %s %d bytes\n", ktr->ktr_fd,
		ktr->ktr_rw == UIO_READ ? "read" : "wrote", datalen);
	if (maxdata && datalen > maxdata)
		datalen = maxdata;
	(void)printf("       \"");
	col = 8;
	for (; datalen > 0; datalen--, dp++) {
		(void) vis(visbuf, *dp, VIS_CSTYLE, *(dp+1));
		cp = visbuf;
		/*
		 * Keep track of printables and
		 * space chars (like fold(1)).
		 */
		if (col == 0) {
			(void)putchar('\t');
			col = 8;
		}
		switch(*cp) {
		case '\n':
			col = 0;
			(void)putchar('\n');
			continue;
		case '\t':
			width = 8 - (col&07);
			break;
		default:
			width = strlen(cp);
		}
		if (col + width > (screenwidth-2)) {
			(void)printf("\\\n\t");
			col = 8;
		}
		col += width;
		do {
			(void)putchar(*cp++);
		} while (*cp);
	}
	if (col == 0)
		(void)printf("       ");
	(void)printf("\"\n");
}

void
ktrpsig(psig)
	struct ktr_psig *psig;
{
	int signo, first;

	(void)printf("SIG%s ", sys_signame[psig->signo]);
	if (psig->action == SIG_DFL)
		(void)printf("SIG_DFL\n");
	else {
		(void)printf("caught handler=0x%lx mask=(",
		    (u_long)psig->action);
		first = 1;
		for (signo = 1; signo < NSIG; signo++) {
			if (sigismember(&psig->mask, signo)) {
				if (first)
					first = 0;
				else
					(void)printf(",");
				(void)printf("%d", signo);
			}
		}
		(void)printf(") code=0x%x\n", psig->code);
	}
}

void
ktrcsw(cs)
	struct ktr_csw *cs;
{

	(void)printf("%s %s\n", cs->out ? "stop" : "resume",
	    cs->user ? "user" : "kernel");
}

void
usage()
{

	(void)fprintf(stderr,
"usage: kdump [-dnlRT] [-e emulation] [-f trfile] [-m maxdata] [-t [cnis]]\n");
	exit(1);
}

void
setemul(name, pid, update_ectx)
	const char *name;
	pid_t pid;
	int update_ectx;
{
	int i;
	struct emulation *match = NULL;

	for (i = 0; emulations[i].name != NULL; i++) {
		if (strcmp(emulations[i].name, name) == 0) {
			match = &emulations[i];
			break;
		}
	}

	if (!match) {
		warnx("Emulation `%s' unknown", name);
		return;
	}

	if (update_ectx)
		ectx_update(pid, match);

	current = match;
}

/*
 * Emulation context list is very simple chained list, not even hashed.
 * We expect the number of separate traced contexts/processes to be
 * fairly low, so it's not worth it to optimize this.
 */

/*
 * Find an emulation context appropriate for the given pid.
 */
static struct emulation_ctx *
ectx_find(pid)
	pid_t pid;
{
	struct emulation_ctx *ctx;

	for(ctx = emul_ctx; ctx != NULL; ctx = ctx->next) {
		if (ctx->pid == pid)
			return ctx;
	}

	return NULL;
}

/*
 * Update emulation context for given pid, or create new if no context
 * for this pid exists.
 */
static void
ectx_update(pid, emul)
	pid_t pid;
	struct emulation *emul;
{
	struct emulation_ctx *ctx;


	if ((ctx = ectx_find(pid)) != NULL) {
		/* found and entry, ensure the emulation is right (exec!) */
		ctx->emulation = emul;
		return;
	}
	
	ctx = (struct emulation_ctx *)malloc(sizeof(struct emulation_ctx));
	ctx->pid = pid;
	ctx->emulation = emul;
	
	/* put the entry on start of emul_ctx chain */
	ctx->next = emul_ctx;
	emul_ctx = ctx;
}

/*
 * Ensure current emulation context is correct for given pid.
 */
static void
ectx_sanify(pid)
	pid_t pid;
{
	struct emulation_ctx *ctx;

	if ((ctx = ectx_find(pid)) != NULL && ctx->emulation != current)
		current = ctx->emulation;
	else
		current = default_emul;
}

