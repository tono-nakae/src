/*	$NetBSD: bcopy.S,v 1.5 2013/07/18 22:42:50 matt Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by J.T. Conklin.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	RCSID("from: @(#)bcopy.s	5.1 (Berkeley) 5/12/90")
#else
	RCSID("$NetBSD: bcopy.S,v 1.5 2013/07/18 22:42:50 matt Exp $")
#endif
#endif /* LIBC_SCCS and not lint */


#ifdef MEMCOPY
#define	XCOPY	memcpy
#elif defined(MEMMOVE)
#define	XCOPY	memmove
#else
#define	XCOPY	bcopy
#endif

ENTRY(XCOPY)
#if defined(MEMCOPY) || defined(MEMMOVE)
	movl	4(%sp),%a1		| dest address
	movl	8(%sp),%a0		| src address
#else
	movl	4(%sp),%a0		| src address
	movl	8(%sp),%a1		| dest address
#endif
	movl	12(%sp),%d1		| count

	cmpl	%a1,%a0			| src after dest?
	jlt	Lbcback			| yes, must copy backwards

	/* 
	 * It isn't worth the overhead of aligning to {long}word boundries
	 * if the string is too short.
	 */
	cmpl	#8,%d1
	jlt	Lbcfbyte

#ifdef	__mc68010__
	/*
	 * The 68010 cannot access a word or long on an odd boundary,
	 * period.  If the source and the destination addresses aren't
	 * of the same evenness, we're forced to do a bytewise copy.
	 */
	movl	%a0,%d0
	addl	%a1,%d0
	btst	#0,%d0
	jne	Lbcfbyte
#endif	/* __mc68010__ */
	
	/* word align */
	movl	%a1,%d0
	btst	#0,%d0		| if (dst & 1)
	jeq	Lbcfalgndw	| 
	movb	(%a0)+,(%a1)+	|	*(char *)dst++ = *(char *) src++
	subql	#1,%d1		|	len--
Lbcfalgndw:
	/* long word align */
	btst	#1,%d0		| if (dst & 2)
	jeq	Lbcfalgndl
	movw	(%a0)+,(%a1)+	|	*(short *)dst++ = *(short *) dst++
	subql	#2,%d1		|	len -= 2
Lbcfalgndl:
	/* copy by 8 longwords */
	movel	%d1,%d0
	lsrl	#5,%d0		| cnt = len / 32
	jeq	Lbcflong	| if (cnt)
	andl	#31,%d1		|	len %= 32
	subql	#1,%d0		|	set up for dbf
Lbcf32loop:
	movl	(%a0)+,(%a1)+	|	copy 8 long words
	movl	(%a0)+,(%a1)+
	movl	(%a0)+,(%a1)+
	movl	(%a0)+,(%a1)+
	movl	(%a0)+,(%a1)+
	movl	(%a0)+,(%a1)+
	movl	(%a0)+,(%a1)+
	movl	(%a0)+,(%a1)+
#ifndef __mcoldfire__
	dbf	%d0,Lbcf32loop	|	till done
	clrw	%d0
#endif
	subql	#1,%d0
	jcc	Lbcf32loop

Lbcflong:
	/* copy by longwords */
	movel	%d1,%d0
	lsrl	#2,%d0		| cnt = len / 4
	jeq	Lbcfbyte	| if (cnt)
	subql	#1,%d0		|	set up for dbf
Lbcflloop:
	movl	(%a0)+,(%a1)+	|	copy longwords
#ifdef __mcoldfire__
	subql	#1,%d0		|	decrement
	jcc	Lbcflloop	|	til done
#else
	dbf	%d0,Lbcflloop	|	til done
#endif
	andl	#3,%d1		|	len %= 4
	jeq	Lbcdone

	subql	#1,%d1		| set up for dbf
Lbcfbloop:
	movb	(%a0)+,(%a1)+	| copy bytes
Lbcfbyte:
#ifdef __mcoldfire__
	subql	#1,%d0		|	decrement
	jcc	Lbcfbloop	|	til done
#else
	dbf	%d1,Lbcfbloop	| till done
#endif
Lbcdone:
#if defined(MEMCOPY) || defined(MEMMOVE)
	movl	4(%sp),%d0	| dest address
#if defined(__SVR4_ABI__)
	moveal	%d0,%a0
#endif
#endif
	rts


Lbcback:
	addl	%d1,%a0		| src pointer to end
	addl	%d1,%a1		| dest pointer to end

	/* 
	 * It isn't worth the overhead of aligning to {long}word boundries
	 * if the string is too short.
	 */
	cmpl	#8,%d1
	jlt	Lbcbbyte

#ifdef	__mc68010__
	/*
	 * The 68010 cannot access a word or long on an odd boundary,
	 * period.  If the source and the destination addresses aren't
	 * of the same evenness, we're forced to do a bytewise copy.
	 */
	movl	%a0,%d0
	addl	%a1,%d0
	btst	#0,%d0
	jne	Lbcbbyte
#endif	/* __mc68010__ */
	
	/* word align */
	movl	%a1,%d0
	btst	#0,%d0		| if (dst & 1)
	jeq	Lbcbalgndw	| 
	movb	-(%a0),-(%a1)	|	*(char *)dst-- = *(char *) src--
	subql	#1,%d1		|	len--
Lbcbalgndw:
	/* long word align */
	btst	#1,%d0		| if (dst & 2)
	jeq	Lbcbalgndl
	movw	-(%a0),-(%a1)	|	*(short *)dst-- = *(short *) dst--
	subql	#2,%d1		|	len -= 2
Lbcbalgndl:
	/* copy by 8 longwords */
	movel	%d1,%d0
	lsrl	#5,%d0		| cnt = len / 32
	jeq	Lbcblong	| if (cnt)
	andl	#31,%d1		|	len %= 32
	subql	#1,%d0		|	set up for dbf
Lbcb32loop:
	movl	-(%a0),-(%a1)	|	copy 8 long words
	movl	-(%a0),-(%a1)
	movl	-(%a0),-(%a1)
	movl	-(%a0),-(%a1)
	movl	-(%a0),-(%a1)
	movl	-(%a0),-(%a1)
	movl	-(%a0),-(%a1)
	movl	-(%a0),-(%a1)
#ifndef __mcoldfire__
	dbf	%d0,Lbcb32loop	|	till done
	clrw	%d0
#endif
	subql	#1,%d0
	jcc	Lbcb32loop
	
Lbcblong:
	/* copy by longwords */
	movel	%d1,%d0
	lsrl	#2,%d0		| cnt = len / 4
	jeq	Lbcbbyte	| if (cnt)
	subql	#1,%d0		|	set up for dbf
Lbcblloop:
	movl	-(%a0),-(%a1)	|	copy longwords
#ifdef __mcoldfire__
	subql	#1,%d0		|	decrement
	jcc	Lbcblloop	|	til done
#else
	dbf	%d0,Lbcblloop	|	til done
#endif
	andl	#3,%d1		|	len %= 4
	jeq	Lbcdone

	subql	#1,%d1		| set up for dbf
Lbcbbloop:
	movb	-(%a0),-(%a1)	| copy bytes
Lbcbbyte:
#ifdef __mcoldfire__
	subql	#1,%d0		| decrement
	jcc	Lbcbbloop	| til done
#else
	dbf	%d1,Lbcbbloop	| till done
#endif

#if defined(MEMCOPY) || defined(MEMMOVE)
	movl	4(%sp),%d0	| dest address
#if defined(__SVR4_ABI__)
	moveal	%d0,%a0
#endif
#endif
	rts
END(XCOPY)
