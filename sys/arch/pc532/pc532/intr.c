/*	$NetBSD: intr.c,v 1.3 1995/07/04 12:19:07 paulus Exp $  */

/*
 * Copyright (c) 1994 Matthias Pfaller.
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
 *	This product includes software developed by Matthias Pfaller.
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
 *	$Id: intr.c,v 1.3 1995/07/04 12:19:07 paulus Exp $
 */

#define DEFINE_SPLX
#include <sys/param.h>
#include <sys/vmmeter.h>
#include <sys/systm.h>
#include <net/netisr.h>
#include <machine/psl.h>

#define INTS	32
struct iv ivt[INTS];
static int next_sir = 16;
unsigned int imask[5] = {0xffffffff};
unsigned int Cur_pl = 0xffffffff, idisabled, sirpending, astpending;

static void softnet();
static void badhard(struct intrframe *);
static void badsoft(void *);

/*
 * Initialize the interrupt system.
 */
void
intr_init()
{
	int i;
	for (i = 0; i < 16; i++)
		ivt[i].iv_vec = badhard;

	for (i = 16; i < 32; i++) {
		ivt[i].iv_vec = badsoft;
		ivt[i].iv_arg = (void *)i;
	}

	intr_establish(SOFTINT, softclock, NULL, "softclock", IPL_CLOCK, 0);
	intr_establish(SOFTINT, softnet,   NULL, "softnet",   IPL_NET,   0);

	imask[IPL_BIO]   |= SIR_CLOCKMASK;
	imask[IPL_NET]   |= SIR_NETMASK;
	imask[IPL_CLOCK] |= SIR_CLOCKMASK;
}

/*
 * Handle pending software interrupts.
 */
void
check_sir()
{
	register unsigned int cirpending, mask;
	register struct iv *iv;

	di();
	/* Mask off SIR 0, so we are no longer at spl0.
	   This will prevent recursive calls to check_sir */
	Cur_pl |= 0x10000;
	while (cirpending = sirpending) {
		sirpending = 0;
		ei();
		for (iv = ivt + 16, mask = 0x10000;
		     cirpending & -mask; mask <<= 1, iv++) {
			if ((cirpending & mask) != 0) {
				cnt.v_soft++;
				iv->iv_cnt++;
				iv->iv_vec(iv->iv_arg);
			}
		}
		di();
	}
	/* Reenable SIR 0 and return to spl0 */
	Cur_pl &= ~0x10000;
	ei();
	return;
}

/*
 * Establish an interrupt. If intr is set to SOFTINT, a software interrupt
 * is allocated.
 */
int
intr_establish(int intr, void (*vector)(), void *arg, char *use,
		int level, int mode)
{
	if (intr == SOFTINT) {
		if (next_sir >= INTS)
			panic("No software interrupts left");
		di();
		intr = next_sir++;
	} else {
		if (ivt[intr].iv_vec != badhard) {
			printf("Interrupt %d already allocated\n", intr);
			return(-1);
		}
		di();
		switch (mode) {
		case RAISING_EDGE:
			ICUW(TPL)  |=  (1 << intr);
			ICUW(ELTG) &= ~(1 << intr);
			break;
		case FALLING_EDGE:
			ICUW(TPL)  &= ~(1 << intr);
			ICUW(ELTG) &= ~(1 << intr);
			break;
		case HIGH_LEVEL:
			ICUW(TPL)  |=  (1 << intr);
			ICUW(ELTG) |=  (1 << intr);
			break;
		case LOW_LEVEL:
			ICUW(TPL)  &= ~(1 << intr);
			ICUW(ELTG) |=  (1 << intr);
			break;
		default:
			panic("Unknown interrupt mode");
		}
	}
	ivt[intr].iv_vec = vector;
	ivt[intr].iv_arg = arg;
	ivt[intr].iv_cnt = 0;
	ivt[intr].iv_use = use;
	ei();
	if (level > IPL_ZERO)
		imask[level] |= 1 << intr;
#include "sl.h"
#include "ppp.h"
#if NSL > 0 || NPPP > 0
	/* In the presence of SLIP or PPP, splimp > spltty. */
	imask[IPL_NET] |= imask[IPL_TTY];
#endif
	imask[IPL_ZERO] &= ~(1 << intr);
	return(intr);
}

/*
 * Network software interrupt routine
 */
static void
softnet()
{
	register int isr;

	di(); isr = netisr; netisr = 0; ei();
	if (isr == 0) return;
#ifdef INET
#include "ether.h"
#if NETHER > 0
	if (isr & (1 << NETISR_ARP)) arpintr();
#endif
	if (isr & (1 << NETISR_IP)) ipintr();
#endif
#ifdef IMP
	if (isr & (1 << NETISR_IMP)) impintr();
#endif
#ifdef NS
	if (isr & (1 << NETISR_NS)) nsintr();
#endif
#ifdef ISO
	if (isr & (1 << NETISR_ISO)) clnlintr();
#endif
#ifdef CCITT
	if (isr & (1 << NETISR_CCITT)) ccittintr();
#endif
#include "ppp.h"
#if NPPP > 0
	if (isr & (1 << NETISR_PPP)) pppintr();
#endif
}

/*
 * Default hardware interrupt handler
 */
static void
badhard(struct intrframe *frame)
{
	static int bad_count = 0;
	di();
	bad_count++;
	if (bad_count < 5)
   		printf("Unknown hardware interrupt: vec=%d pc=0x%08x psr=0x%04x cpl=0x%08x\n",
		      frame->if_vec, frame->if_pc, frame->if_psr, frame->if_pl);
	if (bad_count == 5)
		printf("Too many unknown hardware interrupts, quitting reporting them.\n");
	ei();
}

/*
 * Default software interrupt handler
 */
static void
badsoft(void *n)
{
	static int bad_count = 0;
	bad_count++;
	if (bad_count < 5)
		printf("Unknown software interrupt: vec=%d\n", (int)n);
	
	if (bad_count == 5)
		printf("Too many unknown software interrupts, quitting reporting them.\n");
}
