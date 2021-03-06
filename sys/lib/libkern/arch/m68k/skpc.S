/*	$NetBSD: skpc.S,v 1.9 2013/07/18 22:14:48 matt Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *
 *	from: Utah Hdr: locore.s 1.58 91/04/22
 *	@(#)locore.s	7.11 (Berkeley) 5/9/91
 */

#include "DEFS.h"

/*
 * Emulate fancy VAX string operations:
 *	skpc(mask, count, startc)
 */
ENTRY(skpc)
	movl	8(%sp),%d0	| get length
	jeq	Lskdone		| nothing to do, return
	movb	7(%sp),%d1	| mask to use
	movl	12(%sp),%a0	| where to start
#ifndef __mcoldfire__
	subqw	#1,%d0		| adjust for dbcc
#endif
Lskloop:
	cmpb	(%a0)+,%d1	| compare with mask
#ifdef __mcoldfire__
	jne	Lskdone		| keep going til no more or zero
	subql	#1,%d0
	jne	Lskloop
#else
	dbne	%d0,Lskloop	| keep going til no more or zero
	addqw	#1,%d0		| overshot by one
#endif
Lskdone:
	rts
