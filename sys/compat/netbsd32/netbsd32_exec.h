/*	$NetBSD: netbsd32_exec.h,v 1.1 1998/08/26 10:20:35 mrg Exp $	*/

/*
 * Copyright (c) 1998 Matthew R. Green
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_SPARC32_EXEC_H_
#define	_SPARC32_EXEC_H_

#include <compat/sparc32/sparc32.h>

/* from <sys/exec_aout.h> */
/*
 * Header prepended to each a.out file.
 * only manipulate the a_midmag field via the
 * N_SETMAGIC/N_GET{MAGIC,MID,FLAG} macros below.
 */
struct sparc32_exec {
	sparc32_u_long	a_midmag;	/* htonl(flags<<26 | mid<<16 | magic) */
	sparc32_u_long	a_text;		/* text segment size */
	sparc32_u_long	a_data;		/* initialized data size */
	sparc32_u_long	a_bss;		/* uninitialized data size */
	sparc32_u_long	a_syms;		/* symbol table size */
	sparc32_u_long	a_entry;	/* entry point */
	sparc32_u_long	a_trsize;	/* text relocation size */
	sparc32_u_long	a_drsize;	/* data relocation size */
};

int exec_sparc32_makecmds __P((struct proc *, struct exec_package *));

#endif /* !_SPARC32_EXEC_H_ */
