/*	$NetBSD: boot.c,v 1.9 1994/12/11 15:18:34 pk Exp $ */

/*-
 * Copyright (c) 1982, 1986, 1990, 1993
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
 *
 * 	@(#)boot.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/reboot.h>
#include <a.out.h>

#include "defs.h"

int debug;
int netif_debug;

/*
 * Boot device is derived from ROM provided information.
 */
#define LOADADDR	0x4000
#define DEFAULT_KERNEL	"netbsd"
extern char		*version;
extern struct promvec	*promvec;
char			kernel[100];
unsigned long		esym;
char			*strtab;
int			strtablen;

main(pp)
struct promvec *pp;
{
	int	io;
	char	*cp;

	printf(">> NetBSD BOOT [%s]\n", version);

	if (promvec->pv_romvec_vers >= 2)
		cp = *promvec->pv_v2bootargs.v2_bootargs;
	else
		cp = (*promvec->pv_v0bootargs)->ba_kernel;

	if (cp == 0 || *cp == 0) {
		strcpy(kernel, DEFAULT_KERNEL);
	} else {
		char	*kp = kernel;

		while (*cp && *cp != '-')
			*kp++ = *cp++;
		while (kp > kernel && *--kp == ' ');
		*++kp = '\0';
	}
	while ((io = open(kernel, 0)) < 0) {
		printf("open: %s: %s\n", kernel, strerror(errno));
		printf("boot: ");
		gets(kernel);
	}
	reset_twiddle();

	printf("Booting %s @ 0x%x\n", kernel, LOADADDR);
	copyunix(io, LOADADDR);
	_rtt();
}

/*ARGSUSED*/
copyunix(io, addr)
	register int	io;
	register char	*addr;
{
	struct exec x;
	int i;
	void (*entry)() = (void (*)())addr;

	i = read(io, (char *)&x, sizeof(x));
	if (i != sizeof(x) ||
	    N_BADMAG(x)) {
		printf("%s: Bad format\n", kernel);
		return;
	}
	reset_twiddle();
	printf("%d", x.a_text);
	if (N_GETMAGIC(x) == ZMAGIC) {
		entry = (void (*)())(addr+sizeof(struct exec));
		addr += sizeof(struct exec);
	}
	if (read(io, (char *)addr, x.a_text) != x.a_text)
		goto shread;
	addr += x.a_text;
	if (N_GETMAGIC(x) == ZMAGIC || N_GETMAGIC(x) == NMAGIC)
		while ((int)addr & __LDPGSZ)
			*addr++ = 0;
	reset_twiddle();
	printf("+%d", x.a_data);
	if (read(io, addr, x.a_data) != x.a_data)
		goto shread;
	addr += x.a_data;
	reset_twiddle();
	printf("+%d", x.a_bss);
	for (i = 0; i < x.a_bss; i++)
		*addr++ = 0;
	if (x.a_syms != 0) {
		bcopy(&x.a_syms, addr, sizeof(x.a_syms));
		addr += sizeof(x.a_syms);
		printf("+[%d+", x.a_syms);
		if (read(io, addr, x.a_syms) != x.a_syms)
			goto shread;
		addr += x.a_syms;
		reset_twiddle();

		if (read(io, &strtablen, sizeof(int)) != sizeof(int))
			goto shread;
		reset_twiddle();

		bcopy(&strtablen, addr, sizeof(int));
		if (i = strtablen) {
			i -= sizeof(int);
			addr += sizeof(int);
			if (read(io, addr, i) != i)
			    goto shread;
			reset_twiddle();
			addr += i;
		}
		printf("%d]", i);
		esym = KERNBASE +
			(((int)addr + sizeof(int) - 1) & ~(sizeof(int) - 1));
	}
	printf("=0x%x\n", addr);

#define DDB_MAGIC ( ('D'<<24) | ('D'<<16) | ('B'<<8) | ('0') )
	(*entry)(promvec, esym, DDB_MAGIC);
	return;
shread:
	printf("Short read\n");
	return;
}

static int tw_on;
static int tw_pos;
static char tw_chars[] = "|/-\\";

reset_twiddle()
{
	if (tw_on)
		putchar('\b');
	tw_on = 0;
	tw_pos = 0;
}

twiddle()
{
	if (tw_on)
		putchar('\b');
	else
		tw_on = 1;
	putchar(tw_chars[tw_pos++]);
	tw_pos %= (sizeof(tw_chars) - 1);
}

_rtt()
{
	promvec->pv_halt();
}
