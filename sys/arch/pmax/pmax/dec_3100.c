/*	$NetBSD: dec_3100.c,v 1.2 1998/03/26 06:32:37 thorpej Exp $	*/

/*
 * Copyright (c) 1998 Jonathan Stone.  All rights reserved.
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
 *	This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, The Mach Operating System project at
 * Carnegie-Mellon University and Ralph Campbell.
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
 *	@(#)machdep.c	8.3 (Berkeley) 1/12/94
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/locore.h>
#include <machine/autoconf.h>		/* intr_arg_t */
#include <machine/sysconf.h>		/* intr_arg_t */

#include <mips/mips_param.h>		/* hokey spl()s */
#include <mips/mips/mips_mcclock.h>	/* mcclock CPUspeed estimation */

#include <pmax/pmax/clockreg.h>
#include <pmax/pmax/turbochannel.h> 
#include <pmax/pmax/pmaxtype.h> 
#include <pmax/pmax/trap.h>		/* mboard-specific interrupt fns */
#include <pmax/pmax/cons.h>

#include <pmax/pmax/machdep.h>		/* XXXjrs replace with vectors */

#include <pmax/pmax/kn01var.h>
#include <pmax/pmax/kn01.h>


struct ethercom;
struct ifmedia;
struct ifmediareq;
struct ifnet;

#include <sys/socket.h>		/* struct socket, for... */
#include <net/if.h>		/* struct if socket, for... */
#include <net/if_ether.h>	/* ethercom */
#include <net/if_media.h>	/* ifmedia requests for am7990 */
#include <dev/ic/am7990var.h>	/* all this to get lance intr */

#include "dc_ds.h"
#include "le_pmax.h"
#include "sii.h"


void		dec_3100_init __P((void));
void		dec_3100_os_init __P((void));
void		dec_3100_bus_reset __P((void));

void		dec_3100_enable_intr 
		   __P ((u_int slotno, int (*handler) __P((intr_arg_t sc)),
			 intr_arg_t sc, int onoff));
int		dec_3100_intr __P((u_int mask, u_int pc, 
			      u_int statusReg, u_int causeReg));

void		dec_3100_cons_init __P((void));
void		dec_3100_device_register __P((struct device *, void *));

static void	dec_3100_errintr __P((void));

/*
 * Fill in platform struct. 
 */
void
dec_3100_init()
{

	platform.iobus = "ibus";

	platform.os_init = dec_3100_os_init;
	platform.bus_reset = dec_3100_bus_reset;
	platform.cons_init = dec_3100_cons_init;
	platform.device_register = dec_3100_device_register;

	strcpy(cpu_model, "DECstation 2100 or 3100 (PMAX)");

	dec_3100_os_init();
}


void
dec_3100_os_init()
{
	/*
	 * Set up interrupt handling and I/O addresses.
	 */
	mips_hardware_intr = dec_3100_intr;
	tc_enable_interrupt = dec_3100_enable_intr; /*XXX*/
	Mach_splbio = cpu_spl0;
	Mach_splnet = cpu_spl1;
	Mach_spltty = cpu_spl2;
	Mach_splimp = splhigh; /*XXX Mach_spl1(), if not for malloc()*/
	Mach_splclock = cpu_spl3;
	Mach_splstatclock = cpu_spl3;

	mcclock_addr = (volatile struct chiptime *)
		MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);
	mc_cpuspeed(mcclock_addr, MIPS_INT_MASK_3);
}


/*
 * Initalize the memory system and I/O buses.
 */
void
dec_3100_bus_reset()
{
	/* nothing to do */
	(void)wbflush();
}

void
dec_3100_cons_init()
{
}


void
dec_3100_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	panic("dec_3100_device_register unimplemented");
}


/*
 * Enable an interrupt from a slot on the KN01 internal bus.
 *
 * The 4.4bsd kn01 interrupt handler hard-codes r3000 CAUSE register
 * bits to particular device interrupt handlers.  We may choose to store
 * function and softc pointers at some future point.
 */
void
dec_3100_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	/*
	 */
	if (on)  {
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
}



/*
 *  The pmax (3100) has no option bus. Each device is wired to
 * a separate interrupt.  For historical reasons, we call interrupt
 * routines directly, if they're enabled.
 */

#if NLE > 0
int leintr __P((void *));
#endif
#if NSII > 0
int siiintr __P((void *));
#endif
#if NDC_DS > 0
int dcintr __P((void *));
#endif

/*
 * Handle pmax (DECstation 2100/3100) interrupts.
 */
int
dec_3100_intr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	extern struct cfdriver sii_cd;		/* XXX XXX XXX */
	extern struct cfdriver dc_cd;		/* XXX XXX XXX */
	register volatile struct chiptime *c = 
	    (volatile struct chiptime *)MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);
	struct clockframe cf;
	int temp;

	/* handle clock interrupts ASAP */
	if (mask & MIPS_INT_MASK_3) {
		temp = c->regc;	/* XXX clear interrupt bits */
		cf.pc = pc;
		cf.sr = statusReg;
		hardclock(&cf);
		intrcnt[HARDCLOCK]++;

		/* keep clock interrupts enabled when we return */
		causeReg &= ~MIPS_INT_MASK_3;
	}

	/* If clock interrupts were enabled, re-enable them ASAP. */
	splx(MIPS_SR_INT_ENA_CUR | (statusReg & MIPS_INT_MASK_3));

#if NSII > 0
	if (mask & MIPS_INT_MASK_0) {
		intrcnt[SCSI_INTR]++;
		siiintr(sii_cd.cd_devs[0]);
	}
#endif /* NSII */

#if NLE_PMAX > 0
	if (mask & MIPS_INT_MASK_1) {
		/* 
		 * tty interrupts were disabled by the splx() call
		 * that re-enables clock interrupts.  A slip or ppp driver
		 * manipulating if queues should have called splimp(),
		 * which would mask out MIPS_INT_MASK_1.
		 */
		am7990_intr(tc_slot_info[1].sc);
		intrcnt[LANCE_INTR]++;
	}
#endif /* NLE_PMAX */

#if NDC_DS > 0
	if (mask & MIPS_INT_MASK_2) {
		dcintr(dc_cd.cd_devs[0]);
		intrcnt[SERIAL0_INTR]++;
	}
#endif /* NDC_DS */

	if (mask & MIPS_INT_MASK_4) {
		dec_3100_errintr();
		intrcnt[ERROR_INTR]++;
	}
	return ((statusReg & ~causeReg & MIPS_HARD_INT_MASK) |
		MIPS_SR_INT_ENA_CUR);
}

/*
 * Handle memory errors.
 */
static void
dec_3100_errintr()
{
	volatile u_short *sysCSRPtr =
		(u_short *)MIPS_PHYS_TO_KSEG1(KN01_SYS_CSR);
	u_short csr;

	csr = *sysCSRPtr;

	if (csr & KN01_CSR_MERR) {
		printf("Memory error at 0x%x\n",
			*(unsigned *)MIPS_PHYS_TO_KSEG1(KN01_SYS_ERRADR));
		panic("Mem error interrupt");
	}
	*sysCSRPtr = (csr & ~KN01_CSR_MBZ) | 0xff;
}
