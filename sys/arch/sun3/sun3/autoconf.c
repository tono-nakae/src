/*	$NetBSD: autoconf.c,v 1.19 1994/12/20 05:30:29 gwr Exp $	*/

/*
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Setup the system to run on the current machine.
 *
 * Configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/dmap.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/isr.h>
#include <machine/pte.h>
#include <machine/pmap.h>

extern int soft1intr();

void mainbusattach __P((struct device *, struct device *, void *));
void conf_init(), swapgeneric();
void swapconf(), dumpconf();


struct mainbus_softc {
	struct device mainbus_dev;
};
	
struct cfdriver mainbuscd = 
{ NULL, "mainbus", always_match, mainbusattach, DV_DULL,
	sizeof(struct mainbus_softc), 0};

void mainbusattach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	struct cfdata *new_match;
	
	printf("\n");
	while (1) {
		new_match = config_search(NULL, self, NULL);
		if (!new_match) break;
		config_attach(self, new_match, NULL, NULL);
	}
}

int nmi_intr(arg)
	int arg;
{
	printf("nmi interrupt received\n");
	return 1;
}

void configure()
{
	int root_found;

	/* General device autoconfiguration. */
	root_found = config_rootfound("mainbus", NULL);
	if (!root_found)
		panic("configure: mainbus not found");

	/* Install non-device interrupt handlers. */
	isr_add_autovect(nmi_intr, 0, 7);
	isr_add_autovect(soft1intr, 0, 1);
	isr_cleanup();

	/* Now ready for interrupts. */
	(void)spl0();

	/* Build table for CHR-to-BLK translation, etc. */
	conf_init();

#ifdef	GENERIC
	/* Choose root and swap devices. */
	swapgeneric();
#endif
	swapconf();
	dumpconf();
}

/*
 * Configure swap space and related parameters.
 */
void
swapconf()
{
	struct swdevt *swp;
	u_int maj;
	int nblks;
	
	for (swp = swdevt; swp->sw_dev != NODEV; swp++) {
		maj = major(swp->sw_dev);
		
		if (maj > nblkdev) /* paranoid? */
			break;
		
		if (bdevsw[maj].d_psize) {
			nblks = (*bdevsw[maj].d_psize)(swp->sw_dev);
			if (nblks > 0 &&
				(swp->sw_nblks == 0 || swp->sw_nblks > nblks))
				swp->sw_nblks = nblks;
			swp->sw_nblks = ctod(dtoc(swp->sw_nblks));
		}
	}
}

int always_match(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return 1;
}

/*
 * Generic "bus" support functions.
 */
void bus_scan(parent, child, bustype)
	struct device *parent;
	void *child;
	int bustype;
{
	struct cfdata *cf = child;
	struct confargs ca;
	cfmatch_t match;

#ifdef	DIAGNOSTIC
	if (parent->dv_cfdata->cf_driver->cd_indirect)
		panic("bus_scan: indirect?");
	if (cf->cf_fstate == FSTATE_STAR)
		panic("bus_scan: FSTATE_STAR");
#endif

	ca.ca_bustype = bustype;
	ca.ca_paddr  = cf->cf_loc[0];
	ca.ca_intpri = cf->cf_loc[1];

	if ((bustype == BUS_VME16) || (bustype == BUS_VME32)) {
		ca.ca_intvec = cf->cf_loc[2];
	} else {
		ca.ca_intvec = -1;
	}

	match = cf->cf_driver->cd_match;
	if ((*match)(parent, cf, &ca) > 0) {
		config_attach(parent, cf, &ca, bus_print);
	}
}

int
bus_print(args, name)
	void *args;
	char *name;
{
	struct confargs *ca = args;

	if (ca->ca_paddr != -1)
		printf(" addr 0x%x", ca->ca_paddr);
	if (ca->ca_intpri != -1)
		printf(" level %d", ca->ca_intpri);
	if (ca->ca_intvec != -1)
		printf(" vector 0x%x", ca->ca_intvec);
	/* XXXX print flags? */
	return(QUIET);
}

/*
 * Read addr with size len (1,2,4) into val.
 * If this generates a bus error, return -1
 *
 *	Create a temporary mapping,
 *	Try the access using fu{byte,sword,word}
 *	Clean up temp. mapping
 */
extern vm_offset_t tmp_vpages[];
extern int fubyte(), fusword(), fuword();
int bus_peek(bustype, paddr, sz)
	int bustype, paddr, sz;
{
	int off, pte, rv;
	vm_offset_t pgva;
	caddr_t va;

	off = paddr & PGOFSET;
	paddr -= off;
	pte = PA_PGNUM(paddr);

#define	PG_PEEK 	PG_VALID | PG_WRITE | PG_SYSTEM | PG_NC
	switch (bustype) {
	case BUS_OBMEM:
		pte |= (PG_PEEK | PGT_OBMEM);
		break;
	case BUS_OBIO:
		pte |= (PG_PEEK | PGT_OBIO);
		break;
	case BUS_VME16:
		pte |= (PG_PEEK | PGT_VME_D16);
		break;
	case BUS_VME32:
		pte |= (PG_PEEK | PGT_VME_D32);
		break;
	default:
		return (-1);
	}
#undef	PG_PEEK

	pgva = tmp_vpages[0];
	va = (caddr_t)pgva + off;

	set_pte(pgva, pte);

	/*
	 * OK, try the access using one of the assembly routines
	 * that will set pcb_onfault and catch any bus errors.
	 */
	switch (sz) {
	case 1:
		rv = peek_byte(va);
		break;
	case 2:
		rv = peek_word(va);
		break;
	default:
		printf(" bus_peek: invalid size=%d\n", sz);
		rv = -1;
	}

	set_pte(pgva, PG_INVAL);

	return rv;
}
