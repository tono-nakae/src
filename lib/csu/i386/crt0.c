/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *	$Id: crt0.c,v 1.19 1995/06/02 21:30:47 pk Exp $
 */


#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "%W% (Erasmus) %G%";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <stdlib.h>

int		_callmain __P((void));
int		main __P((int, char **, char **));
static char	*_strrchr __P((char *, char));
extern void	start __P((void)) asm("start");

char			**environ;
int			errno;
static char		empty[1];
char			*__progname = empty;
extern unsigned char	etext;
extern unsigned char	eprol asm ("eprol");


#ifdef DYNAMIC
#include <sys/syscall.h>
#include <a.out.h>
#ifndef N_GETMAGIC
#define N_GETMAGIC(x)	((x).a_magic)
#endif
#ifndef N_BSSADDR
#define N_BSSADDR(x)	(N_DATADDR(x)+(x).a_data)
#endif
#include <sys/mman.h>
#ifdef sun
#define MAP_COPY	MAP_PRIVATE
#define MAP_ANON	0
#endif
#include <link.h>

extern struct _dynamic	_DYNAMIC;
static struct ld_entry	*ld_entry;
static void		__do_dynamic_link __P((void));
extern int		__syscall __P((int, ...));
#ifdef DEBUG
static char		*_getenv __P((char *));
static int		_strncmp __P((char *, char *, int));
#endif

#ifdef sun
#define LDSO	"/usr/lib/ld.so"
#endif
#ifdef BSD
#define LDSO	"/usr/libexec/ld.so"
#endif

#endif /* DYNAMIC */

/*
 * We need these system calls, but can't use library stubs
 */
#define _exit(v)		__syscall(SYS_exit, (v))
#define open(name, f, m)	__syscall(SYS_open, (name), (f), (m))
#define close(fd)		__syscall(SYS_close, (fd))
#define read(fd, s, n)		__syscall(SYS_read, (fd), (s), (n))
#define write(fd, s, n)		__syscall(SYS_write, (fd), (s), (n))
#define dup(fd)			__syscall(SYS_dup, (fd))
#define dup2(fd, fdnew)		__syscall(SYS_dup2, (fd), (fdnew))
#ifdef sun
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall(SYS_mmap, (addr), (len), (prot), _MAP_NEW|(flags), (fd), (off))
#else
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall(SYS___syscall, (quad_t)SYS_mmap, (addr), (len), (prot), (flags), \
	(fd), 0, (off_t)(off))
#endif

#define _FATAL(str) \
	write(2, str, sizeof(str)), \
	_exit(1);


void
start()
{
	struct kframe {
		int	kargc;
		char	*kargv[1];	/* size depends on kargc */
		char	kargstr[1];	/* size varies */
		char	kenvstr[1];	/* size varies */
	};
	/*
	 *	ALL REGISTER VARIABLES!!!
	 */
	register struct kframe *kfp;
	register char **argv, *ap;
	extern void monstartup __P((u_long, u_long));
	extern void _mcleanup __P((void));

#ifdef lint
	kfp = 0;
	initcode = initcode = 0;
#else /* not lint */
	/* just above the saved frame pointer */
	asm ("lea 4(%%ebp), %0" : "=r" (kfp) );
#endif /* not lint */
	argv = &kfp->kargv[0];
	environ = argv + kfp->kargc + 1;

	if (ap = argv[0])
		if ((__progname = _strrchr(ap, '/')) == NULL)
			__progname = ap;
		else
			++__progname;

#ifdef DYNAMIC
	/* ld(1) convention: if DYNAMIC = 0 then statically linked */
#ifdef stupid_gcc
	if (&_DYNAMIC)
#else
	if ( ({volatile caddr_t x = (caddr_t)&_DYNAMIC; x; }) )
#endif
		__do_dynamic_link();
#endif /* DYNAMIC */

asm("eprol:");

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&eprol, (u_long)&etext);
#endif MCRT0

asm ("__callmain:");		/* Defined for the benefit of debuggers */
	exit(main(kfp->kargc, argv, environ));
}

#ifdef DYNAMIC
static void
__do_dynamic_link ()
{
	struct crt_ldso	crt;
	struct exec	hdr;
	char		*ldso;
	int		(*entry) __P((int, struct crt_ldso *));

#ifdef DEBUG
	/* Provision for alternate ld.so - security risk! */
	if (!(ldso = _getenv("LDSO")))
#endif
		ldso = LDSO;

	crt.crt_ldfd = open(ldso, 0, 0);
	if (crt.crt_ldfd == -1) {
		_FATAL("No ld.so\n");
	}

	/* Read LDSO exec header */
	if (read(crt.crt_ldfd, &hdr, sizeof hdr) < sizeof hdr) {
		_FATAL("Failure reading ld.so\n");
	}
	if (N_GETMAGIC(hdr) != ZMAGIC && N_GETMAGIC(hdr) != QMAGIC) {
		_FATAL("Bad magic: ld.so\n");
	}

	/* We use MAP_ANON */
	crt.crt_dzfd = -1;

	/* Map in ld.so */
	crt.crt_ba = mmap(0, hdr.a_text,
			PROT_READ|PROT_EXEC,
			MAP_COPY,
			crt.crt_ldfd, N_TXTOFF(hdr));
	if (crt.crt_ba == -1) {
		_FATAL("Cannot map ld.so\n");
	}

#ifdef BSD
/* !!!
 * This is gross, ld.so is a ZMAGIC a.out, but has `sizeof(hdr)' for
 * an entry point and not at PAGSIZ as the N_*ADDR macros assume.
 */
#undef N_DATADDR
#undef N_BSSADDR
#define N_DATADDR(x)	((x).a_text)
#define N_BSSADDR(x)	((x).a_text + (x).a_data)
#endif

	/* Map in data segment of ld.so writable */
	if (mmap(crt.crt_ba+N_DATADDR(hdr), hdr.a_data,
			PROT_READ|PROT_WRITE,
			MAP_FIXED|MAP_COPY,
			crt.crt_ldfd, N_DATOFF(hdr)) == -1) {
		_FATAL("Cannot map ld.so\n");
	}

	/* Map bss segment of ld.so zero */
	if (hdr.a_bss && mmap(crt.crt_ba+N_BSSADDR(hdr), hdr.a_bss,
			PROT_READ|PROT_WRITE,
			MAP_FIXED|MAP_ANON|MAP_COPY,
			crt.crt_dzfd, 0) == -1) {
		_FATAL("Cannot map ld.so\n");
	}

	crt.crt_dp = &_DYNAMIC;
	crt.crt_ep = environ;
	crt.crt_bp = (caddr_t)_callmain;
	crt.crt_prog = __progname;

	entry = (int (*) __P((int, struct crt_ldso *)))
		(crt.crt_ba + sizeof hdr);
	if ((*entry)(CRT_VERSION_BSD_3, &crt) == -1) {
		_FATAL("ld.so failed\n");
	}

	ld_entry = _DYNAMIC.d_entry;
	return;
}

/*
 * DL stubs
 */

void *
dlopen(name, mode)
char	*name;
int	mode;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlopen)(name, mode);
}

int
dlclose(fd)
void	*fd;
{
	if (ld_entry == NULL)
		return -1;

	return (ld_entry->dlclose)(fd);
}

void *
dlsym(fd, name)
void	*fd;
char	*name;
{
	if (ld_entry == NULL)
		return NULL;

	return (ld_entry->dlsym)(fd, name);
}

int
dlctl(fd, cmd, arg)
void	*fd, *arg;
int	cmd;
{
	if (ld_entry == NULL)
		return -1;

	return (ld_entry->dlctl)(fd, cmd, arg);
}

/*
 * Support routines
 */

#ifdef DEBUG
static int
_strncmp(s1, s2, n)
	register char *s1, *s2;
	register int n;
{

	if (n == 0)
		return (0);
	do {
		if (*s1 != *s2++)
			return (*(unsigned char *)s1 - *(unsigned char *)--s2);
		if (*s1++ == 0)
			break;
	} while (--n != 0);
	return (0);
}

static char *
_getenv(name)
	register char *name;
{
	extern char **environ;
	register int len;
	register char **P, *C;

	for (C = name, len = 0; *C && *C != '='; ++C, ++len);
	for (P = environ; *P; ++P)
		if (!_strncmp(*P, name, len))
			if (*(C = *P + len) == '=') {
				return(++C);
			}
	return (char *)0;
}
#endif

	asm("	___syscall:");
	asm("		popl %ecx");
	asm("		popl %eax");
	asm("		pushl %ecx");
	asm("		int $0x80");
	asm("		pushl %ecx");
	asm("		jc 1f");
	asm("		ret");
	asm("	1:");
	asm("		movl $-1,%eax");
	asm("		ret");

#endif /* DYNAMIC */

static char *
_strrchr(p, ch)
register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif
