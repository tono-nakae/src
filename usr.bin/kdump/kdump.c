/*	$NetBSD: kdump.c,v 1.65 2003/11/16 14:51:26 dsl Exp $	*/

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
__COPYRIGHT("@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)kdump.c	8.4 (Berkeley) 4/28/95";
#else
__RCSID("$NetBSD: kdump.c,v 1.65 2003/11/16 14:51:26 dsl Exp $");
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

#include <ctype.h>
#include <err.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "ktrace.h"
#include "setemul.h"

#include <sys/syscall.h>

int timestamp, decimal, plain, tail, maxdata = -1, numeric;
int word_size;
pid_t do_pid = -1;
const char *tracefile = NULL;
struct ktr_header ktr_header;
int emul_changed = 0;

#define eqs(s1, s2)	(strcmp((s1), (s2)) == 0)

static const char *ptrace_ops[] = {
	"PT_TRACE_ME",	"PT_READ_I",	"PT_READ_D",	"PT_READ_U",
	"PT_WRITE_I",	"PT_WRITE_D",	"PT_WRITE_U",	"PT_CONTINUE",
	"PT_KILL",	"PT_ATTACH",	"PT_DETACH",
};

static const char *linux_ptrace_ops[] = {
	"PTRACE_TRACEME",
	"PTRACE_PEEKTEXT", "PTRACE_PEEKDATA", "PTRACE_PEEKUSER",
	"PTRACE_POKETEXT", "PTRACE_POKEDATA", "PTRACE_POKEUSER",
	"PTRACE_CONT", "PTRACE_KILL", "PTRACE_SINGLESTEP",
	NULL, NULL,
	"PTRACE_GETREGS", "PTRACE_SETREGS", "PTRACE_GETFPREGS",
	"PTRACE_SETFPREGS", "PTRACE_ATTACH", "PTRACE_DETACH",
	"PTRACE_SYSCALL",
};

int	main __P((int, char **));
int	fread_tail __P((char *, int, int));
int	dumpheader __P((struct ktr_header *));
void	ioctldecode __P((u_long));
void	ktrsyscall __P((struct ktr_syscall *));
void	ktrsysret __P((struct ktr_sysret *, int));
void	ktrnamei __P((char *, int));
void	ktremul __P((char *, int, int));
void	ktrgenio __P((struct ktr_genio *, int));
void	ktrpsig __P((void *, int));
void	ktrcsw __P((struct ktr_csw *));
void	ktruser __P((struct ktr_user *, int));
void	ktrmmsg __P((struct ktr_mmsg *, int));
void	usage __P((void));
void	eprint __P((int));
void	rprint __P((register_t));
char	*ioctlname __P((long));
static const char *signame __P((long, int));
static void hexdump_buf(const void *, int, int);
static void visdump_buf(const void *, int, int);

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, ktrlen, size;
	void *m;
	int trpoints = 0;
	int trset = 0;
	const char *emul_name = "netbsd";
	int col;
	char *cp;

	while ((ch = getopt(argc, argv, "e:f:dlm:Nnp:RTt:x:")) != -1)
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
		case 'p':
			do_pid = atoi(optarg);
			break;
		case 'm':
			maxdata = atoi(optarg);
			break;
		case 'N':
			numeric++;
			break;
		case 'n':
			plain++;
			break;
		case 'R':
			timestamp = 2;	/* relative timestamp */
			break;
		case 'T':
			timestamp = 1;
			break;
		case 't':
			trset = 1;
			trpoints = getpoints(trpoints, optarg);
			if (trpoints < 0)
				errx(1, "unknown trace point in %s", optarg);
			break;
		case 'x':
			word_size = strtoul(optarg, &cp, 0);
			if (*cp != 0 || word_size & (word_size - 1) ||
			    word_size > 16 || word_size == 0)
				errx(1, "argument to -x must be 1, 2, 4, 8 or 16");
			break;
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (!trset)
		trpoints = ALL_POINTS;

	if (tracefile == NULL) {
		if (argc == 1) {
			tracefile = argv[0];
			argv++;
			argc--;
		}
		else
			tracefile = DEF_TRACEFILE;
	}

	if (argc > 0)
		usage();

	setemul(emul_name, 0, 0);
	mach_lookup_emul();

	m = malloc(size = 1024);
	if (m == NULL)
		errx(1, "malloc: %s", strerror(ENOMEM));
	if (!freopen(tracefile, "r", stdin))
		err(1, "%s", tracefile);
	while (fread_tail((char *)&ktr_header, sizeof(struct ktr_header), 1)) {
		if (trpoints & (1<<ktr_header.ktr_type)
		    && (do_pid == -1 || ktr_header.ktr_pid == do_pid))
			col = dumpheader(&ktr_header);
		else
			col = -1;
		if ((ktrlen = ktr_header.ktr_len) < 0)
			errx(1, "bogus length 0x%x", ktrlen);
		if (ktrlen > size) {
			while (ktrlen > size)
				size *= 2;
			m = realloc(m, size);
			if (m == NULL)
				errx(1, "realloc: %s", strerror(ENOMEM));
		}
		if (ktrlen && fread_tail(m, ktrlen, 1) == 0)
			errx(1, "data too short");
		if (col == -1)
			continue;

		/* update context to match currently processed record */
		ectx_sanify(ktr_header.ktr_pid);

		switch (ktr_header.ktr_type) {
		case KTR_SYSCALL:
			ktrsyscall(m);
			break;
		case KTR_SYSRET:
			ktrsysret(m, ktrlen);
			break;
		case KTR_NAMEI:
			ktrnamei(m, ktrlen);
			break;
		case KTR_GENIO:
			ktrgenio(m, ktrlen);
			break;
		case KTR_PSIG:
			ktrpsig(m, ktrlen);
			break;
		case KTR_CSW:
			ktrcsw(m);
			break;
		case KTR_EMUL:
			ktremul(m, ktrlen, size);
			break;
		case KTR_USER:
			ktruser(m, ktrlen);
			break;
		case KTR_MMSG:
			ktrmmsg(m, ktrlen);
			break;
		case KTR_EXEC_ARG:
		case KTR_EXEC_ENV:
			visdump_buf(m, ktrlen, col);
			break;
		default:
			printf("\n");
			hexdump_buf(m, ktrlen, word_size);
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

int
dumpheader(kth)
	struct ktr_header *kth;
{
	char unknown[64], *type;
	static struct timeval prevtime;
	struct timeval temp;
	int col;

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
		type = "CSW ";
		break;
	case KTR_EMUL:
		type = "EMUL";
		break;
	case KTR_USER:
		type = "USER";
		break;
	case KTR_MMSG:
		type = "MMSG";
		break;
	case KTR_EXEC_ENV:
		type = "ENV";
		break;
	case KTR_EXEC_ARG:
		type = "ARG";
		break;
	default:
		(void)sprintf(unknown, "UNKNOWN(%d)", kth->ktr_type);
		type = unknown;
	}

	col = printf("%6d %-8.*s ", kth->ktr_pid, MAXCOMLEN, kth->ktr_comm);
	if (timestamp) {
		if (timestamp == 2) {
			if (prevtime.tv_sec == 0)
				temp.tv_sec = temp.tv_usec = 0;
			else
				timersub(&kth->ktr_time, &prevtime, &temp);
			prevtime = kth->ktr_time;
		} else
			temp = kth->ktr_time;
		col += printf("%ld.%06ld ",
		    (long int)temp.tv_sec, (long int)temp.tv_usec);
	}
	col += printf("%-4s  ", type);
	return col;
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
	    dirbuf, (int) ((cmd >> 8) & 0xff), cmd & 0xff);
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
	const struct emulation *revelant = current;
	register_t *ap;

	if (((ktr->ktr_code >= revelant->nsysnames || ktr->ktr_code < 0)
	    && (mach_traps_dispatch(&ktr->ktr_code, &revelant) == 0)) ||
	    numeric)
		(void)printf("[%d]", ktr->ktr_code);
	else
		(void)printf("%s", revelant->sysnames[ktr->ktr_code]);
	ap = (register_t *)((char *)ktr + sizeof(struct ktr_syscall));
	if (argsize) {
		char c = '(';
		if (!plain) {
			char *cp;

			switch (ktr->ktr_code) {
			case SYS_ioctl:
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
				break;
			
			case SYS_ptrace:
				if (strcmp(revelant->name, "linux") == 0) {
				  if (*ap >= 0 && *ap <= 
				      sizeof(linux_ptrace_ops) /
				      sizeof(linux_ptrace_ops[0]))
					(void)printf("(%s",
					    linux_ptrace_ops[*ap]);
				  else
					(void)printf("(%ld", (long)*ap);
				} else {
				  if (*ap >= 0 && *ap <=
				    sizeof(ptrace_ops) / sizeof(ptrace_ops[0]))
					(void)printf("(%s", ptrace_ops[*ap]);
				  else
					(void)printf("(%ld", (long)*ap);
				}
				c = ',';
				ap++;
				argsize -= sizeof(register_t);
				break;

			case SYS_kill:
				if (decimal)
					(void)printf("(%ld, SIG%s",
					    (long)ap[0], signame(ap[1], 1));
				else
					(void)printf("(%#lx, SIG%s",
					    (long)ap[0], signame(ap[1], 1));
				ap += 2;
				argsize -= 2 * sizeof(register_t);
				break;

			default:
				/* No special handling */
				break;
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
ktrsysret(ktr, len)
	struct ktr_sysret *ktr;
	int len;
{
	const struct emulation *revelant;
	int error = ktr->ktr_error;
	int code = ktr->ktr_code;

	if (emul_changed) 
		revelant = previous;
	else
		revelant = current;
	emul_changed = 0;

	if ((code >= revelant->nsysnames || code < 0 || plain > 1) 
	    && (mach_traps_dispatch(&code, &revelant) == 0))
		(void)printf("[%d] ", code);
	else
		(void)printf("%s ", revelant->sysnames[code]);

	switch (error) {
	case 0:
		rprint(ktr->ktr_retval);
		if (len > offsetof(struct ktr_sysret, ktr_retval_1) &&
		    ktr->ktr_retval_1 != 0) {
			(void)printf(", ");
			rprint(ktr->ktr_retval_1);
		}
		break;

	default:
		eprint(error);
		break;
	}
	(void)putchar('\n');
}

void
rprint(register_t ret)
{
	if (!plain) {
		(void)printf("%ld", (long)ret);
		if (ret < 0 || ret > 9)
			(void)printf("/%#lx", (long)ret);
	} else {
		if (decimal)
			(void)printf("%ld", (long)ret);
		else
			(void)printf("%#lx", (long)ret);
	}
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

	if (current->errnomap) {

		/* No remapping for ERESTART and EJUSTRETURN */
		/* Kludge for linux that has negative error numbers */
		if (current->errnomap[2] > 0 && e < 0)
			goto normal;

		for (i = 0; i < current->nerrnomap; i++)
			if (e == current->errnomap[i])
				break;

		if (i == current->nerrnomap) {
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
		if (!plain)
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
	emul_changed = 1;

	(void)printf("\"%s\"\n", name);
}

static void
hexdump_buf(vdp, datalen, word_sz) 
	const void *vdp;
	int datalen;
	int word_sz;
{
	const char hex[] = "0123456789abcdef";
	char chars[16];
	char bytes[16 * 3 + 4];
	const unsigned char *dp = vdp;
	const unsigned char *datalim = dp + datalen;
	const unsigned char *line_end;
	int off, l, c;
	char *cp, *bp;
	int divmask = word_sz - 1;	/* block size in bytes */
	int gdelim = 3;			/* gap between blocks */
	int bsize = 2;			/* increment for each byte */
	int width;
#if _BYTE_ORDER == _LITTLE_ENDIAN
	int bswap = word_sz - 1;
#else
#define	bswap 0
#endif

	switch (word_sz) {
	case 2:
		gdelim = 2;
		break;
	case 1:
		divmask = 7;
		bsize = 3;
		gdelim = 1;
		break;
	default:
		break;
	}
	width = 16 * bsize + (16 / (divmask + 1)) * gdelim;
	if (word_size != 1)
		width += 2;

	for (off = 0; dp < datalim; off += l) {
		memset(bytes, ' ', sizeof bytes);
		line_end = dp + 16;
		if (line_end > datalim)
			line_end = datalim;

		for (l = 0, bp = bytes, cp = chars; dp < line_end; l++) {
			c = *dp++;
			if ((l & divmask) == 0)
				bp += gdelim;
			bp[(l ^ bswap) * bsize] = hex[c >> 4];
			bp[(l ^ bswap) * bsize + 1] = hex[c & 0xf];
			*cp++ = isgraph(c) ? c : '.';
		};

		printf("\t%3.3x  %.*s%.*s\n", off, width, bytes, l, chars);
	}
}

static void
visdump_buf(const void *vdp, int datalen, int col)
{
	const unsigned char *dp = vdp;
	char *cp;
	int width;
	char visbuf[5];
	static int screenwidth = 0;

	if (screenwidth == 0) {
		struct winsize ws;

		if (!plain && ioctl(fileno(stderr), TIOCGWINSZ, &ws) != -1 &&
		    ws.ws_col > 8)
			screenwidth = ws.ws_col;
		else
			screenwidth = 80;
	}

	(void)printf("\"");
	col++;
	for (; datalen > 0; datalen--, dp++) {
		(void)svis(visbuf, *dp, VIS_CSTYLE,
		    datalen > 1 ? *(dp + 1) : 0, "\"");
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
			width = 8 - (col & 07);
			break;
		default:
			width = strlen(cp);
		}
		if (col + width > (screenwidth - 2)) {
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
ktrgenio(ktr, len)
	struct ktr_genio *ktr;
	int len;
{
	int datalen = len - sizeof (struct ktr_genio);
	char *dp = (char *)ktr + sizeof (struct ktr_genio);

	printf("fd %d %s %d bytes\n", ktr->ktr_fd,
		ktr->ktr_rw == UIO_READ ? "read" : "wrote", datalen);
	if (maxdata == 0)
		return;
	if (maxdata > 0 && datalen > maxdata)
		datalen = maxdata;
	if (word_size) {
		hexdump_buf(dp, datalen, word_size);
		return;
	}
	(void)printf("       ");
	visdump_buf(dp, datalen, 7);
}

void
ktrpsig(v, len)
	void *v;
	int len;
{
	int signo, first;
	struct {
		struct ktr_psig ps;
		siginfo_t si;
	} *psig = v;
	siginfo_t *si = &psig->si;
	const char *code;

	(void)printf("SIG%s ", signame(psig->ps.signo, 0));
	if (psig->ps.action == SIG_DFL)
		(void)printf("SIG_DFL");
	else {
		(void)printf("caught handler=%p mask=(", psig->ps.action);
		first = 1;
		for (signo = 1; signo < NSIG; signo++) {
			if (sigismember(&psig->ps.mask, signo)) {
				if (first)
					first = 0;
				else
					(void)printf(",");
				(void)printf("%d", signo);
			}
		}
		(void)printf(")");
	}
	switch (len) {
	case sizeof(struct ktr_psig):
		if (psig->ps.code)
			printf(" code=0x%x", psig->ps.code);
		printf(psig->ps.action == SIG_DFL ? "\n" : ")\n");
		return;
	case sizeof(*psig):
		if (si->si_code == 0) {
			printf(": code=SI_USER sent by pid=%d, uid=%d)\n",
			    si->si_pid, si->si_uid); 
			return;
		}

		if (si->si_code < 0) {
			switch (si->si_code) {
			case SI_TIMER:
				printf(": code=SI_TIMER sigval %p)\n",
				    si->si_sigval.sival_ptr);
				return;
			case SI_QUEUE:
				code = "SI_QUEUE";
				break;
			case SI_ASYNCIO:
				code = "SI_ASYNCIO";
				break;
			case SI_MESGQ:
				code = "SI_MESGQ";
				break;
			default:
				code = NULL;
				break;
			}
			if (code)
				printf(": code=%s unimplemented)\n", code);
			else
				printf(": code=%d unimplemented)\n",
				    si->si_code);
			return;
		}

		code = siginfocodename(si->si_signo, si->si_code);
		switch (si->si_signo) {
		case SIGCHLD:
			printf(": code=%s child pid=%d, uid=%d, "
			    " status=%u, utime=%lu, stime=%lu)\n", 
			    code, si->si_pid,
			    si->si_uid, si->si_status,
			    (unsigned long) si->si_utime,
			    (unsigned long) si->si_stime); 
			return;
		case SIGILL:
		case SIGFPE:
		case SIGSEGV:
		case SIGBUS:
		case SIGTRAP:
			printf(": code=%s, addr=%p, trap=%d)\n",
			    code, si->si_addr, si->si_trap);
			return;
		case SIGIO:
			printf(": code=%s, fd=%d, band=%lx)\n",
			    code, si->si_fd, si->si_band);
			return;
		default:
			printf(": code=%s, errno=%d)\n",
			    code, si->si_errno);
			return;
		}
		/*NOTREACHED*/
	default:
		warnx("Unhandled size %d for ktrpsig\n", len);
		break;
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
ktruser(usr, len)
	struct ktr_user *usr;
	int len;
{
	int i;
	unsigned char *dta;

	printf("\"%.*s: %d, ", KTR_USER_MAXIDLEN, usr->ktr_id, len);
	dta = (unsigned char *)usr;
	for(i=sizeof(struct ktr_user); i < len; i++)
		printf("%02x", (unsigned int) dta[i]);
	printf("\"\n");
}

void
ktrmmsg(mmsg, len)
	struct ktr_mmsg *mmsg;
	int len;
{
	const char *service_name;
	char *reply;
	int id;

	id = mmsg->ktr_id;
	if ((id / 100) % 2) {  /* Message reply */
		reply = " reply";
		id -= 100;
	} else {
		reply = "";
	}

	if ((service_name = mach_service_name(id)) != NULL)
		printf("%s%s\n", service_name, reply);
	else 
		printf("unknown service%s [%d]\n", reply, mmsg->ktr_id);

	hexdump_buf(mmsg, len, word_size ? word_size : 4);
}

static const char *
signame(long sig, int xlat)
{
	static char buf[64];
	if (sig == 0)
		return " 0";
	else if (sig < 0 || sig >= NSIG) {
		(void)snprintf(buf, sizeof(buf), "*unknown %ld*", sig);
		return buf;
	} else
		return sys_signame[(xlat && current->signalmap != NULL) ?
		    current->signalmap[sig] : sig];
}

void
usage()
{

	(void)fprintf(stderr, "usage: kdump [-dlNnRT] [-e emulation] "
	   "[-f file] [-m maxdata] [-p pid]\n             [-t trstr] "
	   "[-x [size]] [file]\n");
	exit(1);
}
