/*	$NetBSD: sio.c,v 1.5 1996/04/12 02:11:22 cgd Exp $	*/

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/eisa/eisavar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <alpha/pci/siovar.h>

int	siomatch __P((struct device *, void *, void *));
void	sioattach __P((struct device *, struct device *, void *));

struct cfattach sio_ca = {
	sizeof(struct device), siomatch, sioattach,
};

struct cfdriver sio_cd = {
	NULL, "sio", DV_DULL,
};

int	pcebmatch __P((struct device *, void *, void *));

struct cfattach pceb_ca = {
	sizeof(struct device), pcebmatch, sioattach,
};

struct cfdriver pceb_cd = {
	NULL, "pceb", DV_DULL,
};

union sio_attach_args {
	const char *sa_name;			/* XXX should be common */
	struct isabus_attach_args sa_iba;
	struct eisabus_attach_args sa_eba;
};

static int	sioprint __P((void *, char *pnp));

int
siomatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_INTEL_SIO)
		return (0);

	return (1);
}

int
pcebmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL ||
	    PCI_PRODUCT(pa->pa_id) != PCI_PRODUCT_INTEL_PCEB)
		return (0);

	return (1);
}

void
sioattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	union sio_attach_args sa;
	int sio, haseisa;
	char devinfo[256];

	sio = (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_SIO);
	haseisa = (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PCEB);

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo,
	    PCI_REVISION(pa->pa_class));

	if (sio) {
		pci_revision_t rev;

		rev = PCI_REVISION(pa->pa_class);
		
		if (rev < 3)
			printf("%s: WARNING: SIO I SUPPORT UNTESTED\n",
			    self->dv_xname);
	}

#ifdef EVCNT_COUNTERS
	evcnt_attach(self, "intr", &sio_intr_evcnt);
#endif

	if (haseisa) {
		sa.sa_eba.eba_busname = "eisa";
		sa.sa_eba.eba_bc = pa->pa_bc;
		config_found(self, &sa.sa_eba, sioprint);
	}

	sa.sa_iba.iba_busname = "isa";
	sa.sa_iba.iba_bc = pa->pa_bc;
	config_found(self, &sa.sa_iba, sioprint);
}

static int
sioprint(aux, pnp)
	void *aux;
	char *pnp;
{
        register union sio_attach_args *sa = aux;

        if (pnp)
                printf("%s at %s", sa->sa_name, pnp);
        return (UNCONF);
}
