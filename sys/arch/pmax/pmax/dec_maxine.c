/*	$NetBSD: dec_maxine.c,v 1.8 1999/03/25 01:17:52 simonb Exp $	*/

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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: dec_maxine.c,v 1.8 1999/03/25 01:17:52 simonb Exp $");

#include <sys/types.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/locore.h>		/* wbflush() */
#include <machine/autoconf.h>		/* intr_arg_t */
#include <machine/sysconf.h>

#include <mips/mips_param.h>		/* hokey spl()s */
#include <mips/mips/mips_mcclock.h>	/* mcclock CPUspeed estimation */

/* all these to get ioasic_base */
#include <sys/device.h>			/* struct cfdata for.. */
#include <dev/tc/tcvar.h>		/* tc type definitions for.. */
#include <dev/tc/ioasicreg.h>		/* ioasic interrrupt masks */
#include <dev/tc/ioasicvar.h>		/* ioasic_base */

#include <pmax/pmax/clockreg.h>
#include <pmax/pmax/turbochannel.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/machdep.h>		/* XXXjrs replace with vectors */

#include <pmax/pmax/maxine.h>		/* baseboard addresses (constants) */
#include <pmax/pmax/dec_kn02_subr.h>	/* 3min/maxine memory errors */

/*
 * Forward declarations
 */
void		dec_maxine_init __P((void));
void		dec_maxine_os_init __P((void));
void		dec_maxine_bus_reset __P((void));
void		dec_maxine_mach_init __P((void));

void		dec_maxine_enable_intr
		   __P ((u_int slotno, int (*handler) __P((intr_arg_t sc)),
			 intr_arg_t sc, int onoff));
int		dec_maxine_intr __P((u_int mask, u_int pc,
			      u_int statusReg, u_int causeReg));

void		dec_maxine_device_register __P((struct device *, void *));
void		dec_maxine_cons_init __P((void));


/*
 * local declarations
 */
u_long xine_tc3_imask;

/*
 * Fill in platform struct.
 */
void
dec_maxine_init()
{

	platform.iobus = "tcbus";

	platform.os_init = dec_maxine_os_init;
	platform.bus_reset = dec_maxine_bus_reset;
	platform.cons_init = dec_maxine_cons_init;
	platform.device_register = dec_maxine_device_register;

	dec_maxine_os_init();

	sprintf(cpu_model, "Personal DECstation 5000/%d (MAXINE)", cpu_mhz);
}


/*
 * Set up OS level stuff: spls, etc.
 */
void
dec_maxine_os_init()
{
	/* clear any memory errors from probes */
	*(volatile u_int*)MIPS_PHYS_TO_KSEG1(XINE_REG_TIMEOUT) = 0;
	wbflush();

	ioasic_base = MIPS_PHYS_TO_KSEG1(XINE_SYS_ASIC);
	mips_hardware_intr = dec_maxine_intr;
	tc_enable_interrupt = dec_maxine_enable_intr;

	/* On the MAXINE ioasic interrupts at level 3. */
	Mach_splbio = Mach_spl3;
	Mach_splnet = Mach_spl3;
	Mach_spltty = Mach_spl3;
	Mach_splimp = Mach_spl3;

	/*
	 * Note priority inversion of ioasic and clock:
	 * clock interrupts are at hw priority 1, and when blocking
	 * clock interrups we we must block hw priority 3
	 * (bio,net,tty) also.
	 *
	 * XXX hw priority 2 is used for memory errors, we
	 * should not disable memory errors during clock interrupts!
	 */
	Mach_splclock = cpu_spl3;
	Mach_splstatclock = cpu_spl3;
	mcclock_addr = (volatile struct chiptime *)
		MIPS_PHYS_TO_KSEG1(XINE_SYS_CLOCK);
	mc_cpuspeed(mcclock_addr, MIPS_INT_MASK_1);

	/*
	 * Initialize interrupts.
	 */
	*(u_int *)IOASIC_REG_IMSK(ioasic_base) = XINE_IM0;
	*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
}


/*
 * Initalize the memory system and I/O buses.
 */
void
dec_maxine_bus_reset()
{
	/*
	 * Reset interrupts, clear any errors from newconf probes
	 */

	*(volatile u_int*)MIPS_PHYS_TO_KSEG1(XINE_REG_TIMEOUT) = 0;
	wbflush();

	*(volatile u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
	wbflush();
}


void
dec_maxine_cons_init()
{
}

void
dec_maxine_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	panic("dec_maxine_device_register unimplemented");
}



/*
 *  Enable/Disable interrupts from a TURBOchannel slot.
 *
 *	We pretend we actually have 11 slots even if we really have
 *	only 2: TCslots 0-1 maps to slots 0-1, TCslot 2 is used for
 *	the system (TCslot3), TCslot3 maps to slots 3-10
 *	 (see pmax/tc/ds-asic-conf.c).
 *	Note that all these interrupts come in via the IMR.
 */
void
dec_maxine_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register unsigned mask;

	switch (slotno) {
	case 0:			/* a real slot, but  */
		mask = XINE_INTR_TC_0;
		break;
	case 1:			/* a real slot, but */
		mask = XINE_INTR_TC_1;
		break;
	case XINE_FLOPPY_SLOT:
		mask = XINE_INTR_FLOPPY;
		break;
	case XINE_SCSI_SLOT:
		mask = (IOASIC_INTR_SCSI | IOASIC_INTR_SCSI_PTR_LOAD |
			IOASIC_INTR_SCSI_OVRUN | IOASIC_INTR_SCSI_READ_E);
		break;
	case XINE_LANCE_SLOT:
		mask = IOASIC_INTR_LANCE;
		break;
	case XINE_SCC0_SLOT:
		mask = XINE_INTR_SCC_0;
		break;
	case XINE_DTOP_SLOT:
		mask = XINE_INTR_DTOP_RX;
		break;
	case XINE_ISDN_SLOT:
		mask = (XINE_INTR_ISDN | IOASIC_INTR_ISDN_OVRUN |
			IOASIC_INTR_ISDN_TXLOAD | IOASIC_INTR_ISDN_RXLOAD);
		break;
	case XINE_ASIC_SLOT:
		mask = XINE_INTR_ASIC;
		break;
	default:
		return;/* ignore */
	}

	if (on) {
		xine_tc3_imask |= mask;
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		xine_tc3_imask &= ~mask;
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
	*(u_int *)IOASIC_REG_IMSK(ioasic_base) = xine_tc3_imask;
	wbflush();
}


/*
 * Maxine hardware interrupts. (Personal DECstation 5000/xx)
 */
int
dec_maxine_intr(mask, pc, statusReg, causeReg)
	unsigned mask;
	unsigned pc;
	unsigned statusReg;
	unsigned causeReg;
{
	register u_int intr;
	register volatile struct chiptime *c =
	    (volatile struct chiptime *) MIPS_PHYS_TO_KSEG1(XINE_SYS_CLOCK);
	volatile u_int *imaskp = (volatile u_int *)
		MIPS_PHYS_TO_KSEG1(XINE_REG_IMSK);
	volatile u_int *intrp = (volatile u_int *)
		MIPS_PHYS_TO_KSEG1(XINE_REG_INTR);
	u_int old_mask;
	struct clockframe cf;
	int temp;

	old_mask = *imaskp & xine_tc3_imask;
	*imaskp = xine_tc3_imask;

	if (mask & MIPS_INT_MASK_4)
		prom_haltbutton();

	/* handle clock interrupts ASAP */
	if (mask & MIPS_INT_MASK_1) {
		temp = c->regc;	/* XXX clear interrupt bits */
		cf.pc = pc;
		cf.sr = statusReg;
		latched_cycle_cnt =
		    *(u_long*)(MIPS_PHYS_TO_KSEG1(XINE_REG_FCTR));
		hardclock(&cf);
		intrcnt[HARDCLOCK]++;
		/* keep clock interrupts enabled when we return */
		causeReg &= ~MIPS_INT_MASK_1;
	}

	/* If clock interrups were enabled, re-enable them ASAP. */
	splx(MIPS_SR_INT_ENA_CUR | (statusReg & MIPS_INT_MASK_1));

	if (mask & MIPS_INT_MASK_3) {
		intr = *intrp;
		/* masked interrupts are still observable */
		intr &= old_mask;

		if ((intr & XINE_INTR_SCC_0)) {
			if (tc_slot_info[XINE_SCC0_SLOT].intr)
				(*(tc_slot_info[XINE_SCC0_SLOT].intr))
				(tc_slot_info[XINE_SCC0_SLOT].sc);
			else
				printf ("can't handle scc interrupt\n");
			intrcnt[SERIAL0_INTR]++;
		}

		if (intr & IOASIC_INTR_SCSI_PTR_LOAD) {
			*intrp &= ~IOASIC_INTR_SCSI_PTR_LOAD;
#ifdef notdef
			asc_dma_intr();
#endif
		}

		if (intr & (IOASIC_INTR_SCSI_OVRUN | IOASIC_INTR_SCSI_READ_E))
			*intrp &= ~(IOASIC_INTR_SCSI_OVRUN | IOASIC_INTR_SCSI_READ_E);

		if (intr & IOASIC_INTR_LANCE_READ_E)
			*intrp &= ~IOASIC_INTR_LANCE_READ_E;

		if (intr & XINE_INTR_DTOP_RX) {
			if (tc_slot_info[XINE_DTOP_SLOT].intr)
				(*(tc_slot_info[XINE_DTOP_SLOT].intr))
				(tc_slot_info[XINE_DTOP_SLOT].sc);
			else
				printf ("can't handle dtop interrupt\n");
			intrcnt[DTOP_INTR]++;
		}

		if (intr & XINE_INTR_FLOPPY) {
			if (tc_slot_info[XINE_FLOPPY_SLOT].intr)
				(*(tc_slot_info[XINE_FLOPPY_SLOT].intr))
				(tc_slot_info[XINE_FLOPPY_SLOT].sc);
		else
			printf ("can't handle floppy interrupt\n");
			intrcnt[FLOPPY_INTR]++;
		}

		if (intr & XINE_INTR_TC_0) {
			if (tc_slot_info[0].intr)
				(*(tc_slot_info[0].intr))
				(tc_slot_info[0].sc);
			else
				printf ("can't handle tc0 interrupt\n");
			intrcnt[SLOT0_INTR]++;
		}

		if (intr & XINE_INTR_TC_1) {
			if (tc_slot_info[1].intr)
				(*(tc_slot_info[1].intr))
				(tc_slot_info[1].sc);
			else
				printf ("can't handle tc1 interrupt\n");
			intrcnt[SLOT1_INTR]++;
		}

		if (intr & XINE_INTR_ISDN) {
			if (tc_slot_info[XINE_ISDN_SLOT].intr)
				(*(tc_slot_info[XINE_ISDN_SLOT].intr))
				(tc_slot_info[XINE_ISDN_SLOT].sc);
			else
				printf ("can't handle isdn interrupt\n");
				intrcnt[ISDN_INTR]++;
		}

		if (intr & IOASIC_INTR_LANCE) {
			if (tc_slot_info[XINE_LANCE_SLOT].intr)
				(*(tc_slot_info[XINE_LANCE_SLOT].intr))
				(tc_slot_info[XINE_LANCE_SLOT].sc);
			else
				printf ("can't handle lance interrupt\n");

			intrcnt[LANCE_INTR]++;
		}

		if (intr & IOASIC_INTR_SCSI) {
			if (tc_slot_info[XINE_SCSI_SLOT].intr)
				(*(tc_slot_info[XINE_SCSI_SLOT].intr))
				(tc_slot_info[XINE_SCSI_SLOT].sc);
			else
				printf ("can't handle scsi interrupt\n");
			intrcnt[SCSI_INTR]++;
		}
	}
	if (mask & MIPS_INT_MASK_2)
		kn02ba_errintr();
	return ((statusReg & ~causeReg & MIPS_HARD_INT_MASK) |
		MIPS_SR_INT_ENA_CUR);
}
