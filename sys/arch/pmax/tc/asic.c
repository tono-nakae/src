/*	$NetBSD: asic.c,v 1.27 1998/04/19 10:18:20 jonathan Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Keith Bostic, Chris G. Demetriou, Jonathan Stone
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <dev/tc/tcvar.h>
#include <dev/tc/ioasicvar.h>

#include <machine/bus.h>			/* wbflush() */
#include <machine/autoconf.h>

#ifdef alpha
#include <machine/rpb.h>
#include <alpha/tc/tc.h>
#include <alpha/tc/asic.h>
#endif

#ifdef pmax
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/machdep.h>		/* XXX ioasic_init( */
#include <pmax/pmax/asic.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/turbochannel.h>	/* interrupt enable declaration */

#include <pmax/pmax/kmin.h>
#include <pmax/pmax/nameglue.h>

/*
 * Which system models were configured?
 */
#include "opt_dec_3max.h"
#include "opt_dec_3min.h"
#include "opt_dec_3maxplus.h"
#include "opt_dec_maxine.h"



#define	C(x)	((void*)(x))
#define	ASIC_NDEVS(x)	(sizeof(x)/sizeof(x[0]))
#define ARRAY_SIZEOF(x) (sizeof((x)) / sizeof ((x)[0]))

#if defined(DEC_3MAXPLUS) || defined(DEC_3MIN)
struct ioasicdev_attach_args kn03_ioasic_devs[] = {
/* 0 */	{ "lance",	0x0C0000, 0, C(KN03_LANCE_SLOT)	},
/* 1 */	{ "scc",	0x100000, 0, C(KN03_SCC0_SLOT)	},
/* 2 */	{ "scc",	0x180000, 0, C(KN03_SCC1_SLOT)	},
/* 3 */	{ "mc146818",	0x200000, 0, C(0), },
/* 4 */	{ "asc",	0x300000, 0, C(KN03_SCSI_SLOT)	},
};
const int nkn03_ioasic_devs  =  ARRAY_SIZEOF(kn03_ioasic_devs);
#endif /* DEC_3MAXPLUS || DEC_3MIN */

#if defined(DEC_MAXINE)
struct ioasicdev_attach_args xine_ioasic_devs[] = {
/* 0 */	{ "lance",	0x0C0000, 0, C(XINE_LANCE_SLOT),	},
/* 1 */	{ "scc",	0x100000, 0, C(XINE_SCC0_SLOT),	},
/* 2 */	{ "mc146818",	0x200000, 0, C(0),  },
/* 3 */	{ "isdn",	0x240000, 0, C(XINE_ISDN_SLOT),	},
/* 4 */	{ "dtop",	0x280000, 0, C(XINE_DTOP_SLOT),	},
/* 5 */	{ "fdc",	0x2C0000, 0, C(XINE_FLOPPY_SLOT),	},
/* 6 */	{ "asc",	0x300000, 0, C(XINE_SCSI_SLOT),	},
};
const int nxine_ioasic_devs  =  ARRAY_SIZEOF(xine_ioasic_devs);
#endif /* DEC_MAXINE */

#if defined(DEC_3MAX)
struct ioasic_dev kn02_ioasic_devs[] = {
/* 0 */	{ "dc",		0x200000, 0, C(7), },
/* 0 */	{ "mc146818",	0x280000, 0, C(0), },
const int n3max_ioasic_devs  =  ARRAY_SIZEOF(3max_ioasic_devs);
};
#endif /* DEC_3MAX */
#endif /* pmax */

struct asic_softc {
	struct	device sc_dv;
	tc_addr_t sc_base;
};

/* Definition of the driver for autoconfig. */
int	ioasicmatch __P((struct device *, struct cfdata *, void *));
void	ioasicattach __P((struct device *, struct device *, void *));
int     ioasicprint(void *, const char *);

/* Device locators. */
#include "locators.h"
#define	ioasiccf_offset	cf_loc[IOASICCF_OFFSET]		/* offset */

struct cfattach ioasic_ca = {
	sizeof(struct asic_softc), ioasicmatch, ioasicattach
};

void    asic_intr_establish __P((struct confargs *, intr_handler_t,
				 intr_arg_t));
void    asic_intr_disestablish __P((struct confargs *));
caddr_t ioasic_cvtaddr __P((struct confargs *));

int	asic_intrnull __P((intr_arg_t));

struct asic_slot {
	struct confargs	as_ca;
	intr_handler_t	as_handler;
	void		*iada_cookie;
	u_int		iada_intrbits;
};

#ifdef	pmax
/*#define IOASIC_DEBUG*/

struct ioasicdev_attach_args *asic_slots;

extern tc_addr_t	ioasic_base;
tc_addr_t	ioasic_base = 0;
#endif	/*pmax*/


#ifdef IOASIC_DEBUG
#define IOASIC_DPRINTF(x)	printf x
#else
#define IOASIC_DPRINTF(x)	do { if (0) printf x ; } while (0)
#endif

int
ioasicmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct tc_attach_args *ta = aux;

	IOASIC_DPRINTF(("asicmatch: %s slot %d offset 0x%x pri %d\n",
		ta->ta_modname, ta->ta_slot, ta->ta_offset, (int)ta->ta_cookie));

	/* An IOCTL asic can only occur on the turbochannel, anyway. */
#ifdef notyet
	if (parent != &tccd)
		return (0);
#endif


	/*
	 * XXX This is wrong.
	 */
	/* The 3MAX (kn02) is special. */
	if (TC_BUS_MATCHNAME(ta, KN02_ASIC_NAME)) {
#if 0
		printf("(configuring KN02 system slot as asic)\n");
#endif
		goto gotasic;
	}

	/* Make sure that we're looking for this type of device. */
	if (!TC_BUS_MATCHNAME(ta, "IOCTL   "))
		return (0);
gotasic:

	if (cf->cf_unit > 0)
		return (0);

	return (1);
}

void
ioasicattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct asic_softc *sc = (struct asic_softc *)self;
	struct tc_attach_args *ta = aux;
	struct ioasicdev_attach_args idev;
	int i, nslots;

	/* See if the unit number is valid. */
	switch (systype) {
	case DS_3MIN:
	case DS_3MAXPLUS:
		/* 3min ioasic addressees are the same as 3maxplus. */
		asic_slots = kn03_ioasic_devs;
		nslots = nkn03_ioasic_devs;
		break;

#ifdef DEC_MAXINE
	case DS_MAXINE:
		asic_slots = xine_ioasic_devs;
		nslots = nxine_ioasic_devs;
		break;
#endif

#ifdef DEC_3MAX
	case DS_3MAX:
		asic_slots = kn02_ioasic_devs;
		nslots = nkn02_ioasic_devs;
		break;
#endif

	default:
		break;
	}

	if (asic_slots == NULL)
		panic("asicattach: no asic_slot map\n");

	IOASIC_DPRINTF(("asicattach: %s\n", sc->sc_dv.dv_xname));

	sc->sc_base = ta->ta_addr;

	ioasic_base = sc->sc_base;			/* XXX XXX XXX */

#ifdef pmax
	printf("\n");
#else	/* Alpha AXP: select ASIC speed  */
#ifdef DEC_3000_300
	if (systype == ST_DEC_3000_300) {
		*(volatile u_int *)IOASIC_REG_CSR(sc->sc_base) |=
		    IOASIC_CSR_FASTMODE;
		MB();
		printf(": slow mode\n");
	} else
#endif /*DEC_3000_300*/
		printf(": fast mode\n");
	
	/* Decstations use hand-craft code to enable asic interrupts */
	BUS_INTR_ESTABLISH(ta, asic_intr, sc);

#endif 	/* Alpha AXP: select ASIC speed  */


/* The MAXINE has seven pseudo-slots in its system slot */
#define ASIC_MAX_NSLOTS 7 /*XXX*/

        /* Try to configure each CPU-internal device */
        for (i = 0; i < nslots; i++) {

		IOASIC_DPRINTF(("asicattach: entry %d, base addr %x\n",
		       i, sc->sc_base));

		/* Compute address at runtime. Leave table readonly. */
		idev = asic_slots[i];
		idev.iada_addr =((u_long)sc->sc_base) + idev.iada_offset;

		if (idev.iada_modname == NULL || idev.iada_modname[0] == 0)
			break;

		IOASIC_DPRINTF((" adding %s offset %x addr %x\n",
		    idev.iada_modname, idev.iada_offset, idev.iada_addr));

                /* Tell the autoconfig machinery we've found the hardware. */
                config_found(self, &idev, ioasicprint);
        }
	IOASIC_DPRINTF(("asicattach: done\n"));
}

int
ioasicprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct ioasicdev_attach_args *d = aux;

	if (pnp)
		printf("%s at %s", d->iada_modname, pnp);
	printf(" offset 0x%x", d->iada_offset);
	printf(" priority %d", (int)d->iada_cookie);
	return (UNCONF);
}

int
ioasic_submatch(match, d)
	struct cfdata *match;
	struct ioasicdev_attach_args *d;
{

	return ((match->ioasiccf_offset == d->iada_offset) ||
		(match->ioasiccf_offset == IOASICCF_OFFSET_DEFAULT));
}


void
ioasic_intr_establish(dev, cookie, level, handler, val)
    struct device *dev;
    void *cookie;
    tc_intrlevel_t level;
    intr_handler_t handler;
    void *val;
{

	(*tc_enable_interrupt)((int)cookie, handler, val, 1);
}

int
asic_intrnull(val)
	intr_arg_t val;
{

        panic("uncaught IOCTL ASIC intr for slot %ld\n", (long)val);
}


/* XXX */
char *
ioasic_lance_ether_address()
{
	return (u_char *)IOASIC_SYS_ETHER_ADDRESS(ioasic_base);
}

void
ioasic_lance_dma_setup(v)
	void *v;
{
	volatile u_int32_t *ldp;
	tc_addr_t tca;

	tca = (tc_addr_t)v;

	ldp = (volatile u_int *)IOASIC_REG_LANCE_DMAPTR(ioasic_base);
	*ldp = ((tca << 3) & ~(tc_addr_t)0x1f) | ((tca >> 29) & 0x1f);
	tc_wmb();

	*(volatile u_int32_t *)IOASIC_REG_CSR(ioasic_base) |=
	    IOASIC_CSR_DMAEN_LANCE;
	tc_mb();
}

void ioasic_init(int flag);

/*
 * Initialize the I/O asic
 */
void
ioasic_init(isa_maxine)
	int isa_maxine;
{
	volatile u_int *decoder;

	/* These are common between 3min and maxine */
	decoder = (volatile u_int *)IOASIC_REG_LANCE_DECODE(ioasic_base);
	*decoder = KMIN_LANCE_CONFIG;

	/* set the SCSI DMA configuration map */
	decoder = (volatile u_int *) IOASIC_REG_SCSI_DECODE(ioasic_base);
	(*decoder) = 0x00000000e;
}
