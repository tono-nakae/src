/*	$NetBSD: crt0.c,v 1.2 1996/10/18 05:36:47 thorpej Exp $	*/

/*
 * Copyright (C) 1995 Wolfgang Solfrank.
 * Copyright (C) 1995 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

extern void exit();
int _callmain();

#include <sys/param.h>

#include "common.h"

extern inline unsigned long ntohl(unsigned long);

extern void	start() asm("start");

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
	register char **targv;
	register char **argv;
	register int *ptr;
    
	/* just above the saved frame pointer */

	asm ("mov %0, ip" : "=r" (kfp) );

	for (argv = targv = &kfp->kargv[0]; *targv++; /* void */)

	if (targv >= (char **)(*argv))
		--targv;
	environ = targv;

	if (argv[0])
		if ((__progname = _strrchr(argv[0], '/')) == NULL)
			__progname = argv[0];
		else
			++__progname;

#ifdef	DYNAMIC
	/* ld(1) convention: if DYNAMIC = 0 then statically linked */
#ifdef	stupid_gcc
	if (&_DYNAMIC)
#else
	if ( ({volatile caddr_t x = (caddr_t)&_DYNAMIC; x; }) )
#endif
		__load_rtld(&_DYNAMIC);
#endif	/* DYNAMIC */

asm("eprol:");

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&eprol, (u_long)&etext);
#endif

asm ("__callmain:");		/* Defined for the benefit of debuggers */

	exit(main(kfp->kargc, argv, environ));
}

extern inline unsigned long
ntohl(l)
unsigned long l;
{
	return ((l&0xff) << 24)
	       |((l&0xff00) << 8)
	       |((l&0xff0000) >> 8)
	       |((l&0xff000000) >> 24);
}

#ifdef	DYNAMIC
	asm("	___syscall:");
	asm("		swi	0");
	asm("		mvncs	r0, #0");
	asm("		mov	r15, r14");
#endif	DYNAMIC

#include "common.c"

#ifdef MCRT0
asm ("	.text");
asm ("_eprol:");
#endif
