/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bill Sommerfeld
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*
 * PCMCIA attachment for BayStack 650 802.11FH PCMCIA card,
 * based on the AMD 79c930 802.11 controller chip.
 *
 * This attachment can probably be trivally adapted for other FH and
 * DS cards based on the same chipset.
 */

#include "opt_inet.h"
#include "opt_ns.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_ether.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/ic/am79c930reg.h>
#include <dev/ic/am79c930var.h>
#include <dev/ic/awireg.h>
#include <dev/ic/awivar.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

int	awi_pcmcia_match __P((struct device *, struct cfdata *, void *));
void	awi_pcmcia_attach __P((struct device *, struct device *, void *));
int	awi_pcmcia_detach __P((struct device *, int));

int	awi_pcmcia_get_enaddr __P((struct pcmcia_tuple *, void *));
int	awi_pcmcia_enable __P((struct awi_softc *));
void	awi_pcmcia_disable __P((struct awi_softc *));

struct awi_pcmcia_softc {
	struct awi_softc sc_awi;			/* real "awi" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int sc_io_window;			/* our i/o window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
};

int	awi_pcmcia_find __P((struct awi_pcmcia_softc *,
    struct pcmcia_attach_args *, struct pcmcia_config_entry *));

struct cfattach awi_pcmcia_ca = {
	sizeof(struct awi_pcmcia_softc), awi_pcmcia_match, awi_pcmcia_attach,
	awi_pcmcia_detach, /* awi_activate */ 0
};

int
awi_pcmcia_enable(sc)
	struct awi_softc *sc;
{
	struct awi_pcmcia_softc *psc = (struct awi_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;

	/* establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(pf, IPL_NET, awi_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	return (pcmcia_function_enable(pf));
}

void
awi_pcmcia_disable(sc)
	struct awi_softc *sc;
{
	struct awi_pcmcia_softc *psc = (struct awi_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;

	pcmcia_function_disable (pf);
	pcmcia_intr_disestablish (pf, sc->sc_ih);
}

int
awi_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer != PCMCIA_VENDOR_BAY)
		return (0);

	if (pa->product == PCMCIA_PRODUCT_BAY_STACK_650)
		return (1);

	return (0);
}

int
awi_pcmcia_find (psc, pa, cfe)
	struct awi_pcmcia_softc *psc;
	struct pcmcia_attach_args *pa;
	struct pcmcia_config_entry *cfe;
{
	struct awi_softc *sc = &psc->sc_awi;
	int fail = 0;
	u_int8_t version[AWI_BANNER_LEN];
	
#if 0
	printf("\ntry %lx\n", cfe->iospace[0].start);
#endif
	/*
	 * see if we can read the firmware version sanely
	 * through the i/o ports.
	 * if not, try a different CIS string..
	 */
	if (pcmcia_io_alloc(psc->sc_pf, cfe->iospace[0].start,
	    cfe->iospace[0].length, 0, &psc->sc_pcioh) != 0) {
#if 0
		printf("alloc failed\n");
#endif
		goto fail;
	}
	

	if (pcmcia_io_map(psc->sc_pf, PCMCIA_WIDTH_AUTO, 0, psc->sc_pcioh.size,
	    &psc->sc_pcioh, &psc->sc_io_window)) {
#if 0
		printf("io_map failed\n");
#endif
		goto fail_io_free;
	}

	/* Enable the card. */
	pcmcia_function_init(psc->sc_pf, cfe);
	if (pcmcia_function_enable(psc->sc_pf)) {
#if 0
		printf("function_enable failed\n");
#endif
		goto fail_io_unmap;
	}
		
	sc->sc_chip.sc_iot = psc->sc_pcioh.iot;
	sc->sc_chip.sc_ioh = psc->sc_pcioh.ioh;
	am79c930_chip_init(&sc->sc_chip, 0);

	DELAY(100);

	awi_read_bytes (sc, AWI_BANNER, version, AWI_BANNER_LEN);

	if (memcmp(version, "PCnetMobile:", 12) == 0) {
#if 0
		printf("win\n");
#endif
		return 0;
	} else {
#if 0
		printf("bad version: %s\n", version);
#endif
	}
	
	fail++;
	pcmcia_function_disable (psc->sc_pf);
	
 fail_io_unmap:
	fail++;
	pcmcia_io_unmap (psc->sc_pf, psc->sc_io_window);
	
 fail_io_free:
	fail++;
	pcmcia_io_free (psc->sc_pf, &psc->sc_pcioh);
 fail:
	fail++;
	printf("awi_pcmcia_find fail %d\n", fail);
	return fail;
}



void
awi_pcmcia_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct awi_pcmcia_softc *psc = (void *) self;
	struct awi_softc *sc = &psc->sc_awi;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	struct pcmcia_mem_handle memh;
	bus_addr_t memoff;
	int memwin;

#if 0
	int i, j;
#endif

#if 0		

	for (cfe = pa->pf->cfe_head.sqh_first, i=0;
	     cfe != NULL;
	     cfe = cfe->cfe_list.sqe_next, i++) {
		printf("%d: %d memspaces, %d iospaces\n",
		    i, cfe->num_memspace, cfe->num_iospace);
		printf("%d: number %d flags %x iftype %d iomask %lx irqmask %x maxtwins %x\n",
		    i, cfe->number, cfe->flags, cfe->iftype, cfe->iomask,
		    cfe->irqmask, cfe->maxtwins);
		for (j=0; j<cfe->num_memspace; j++) {
			printf("%d: mem %d: len %lx card %lx host %lx\n",
			    i, j,
			    cfe->memspace[j].length,
			    cfe->memspace[j].cardaddr,
			    cfe->memspace[j].hostaddr);
		}
		for (j=0; j<cfe->num_iospace; j++) {
			printf("%d: io %d: len %lx start %lx\n",
			    i, j,
			    cfe->iospace[j].length,
			    cfe->iospace[j].start);
		}
	}
#endif

	psc->sc_pf = pa->pf;

	for (cfe = SIMPLEQ_FIRST(&pa->pf->cfe_head); cfe != NULL;
	     cfe = SIMPLEQ_NEXT(cfe, cfe_list)) {
		if (cfe->iftype != PCMCIA_IFTYPE_IO)
			continue;
		if (cfe->num_iospace < 1)
			continue;
		if (cfe->iospace[0].length < AM79C930_IO_SIZE)
			continue;

		if (awi_pcmcia_find(psc, pa, cfe) == 0)
			break;
	}
	if (cfe == NULL) {
		printf(": no suitable CIS info found\n");
		return;
	}
#if 0
	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);
	if (pcmcia_function_enable(psc->sc_pf))
		printf(": function enable failed\n");
#endif

	sc->sc_enabled = 1;
	sc->sc_state = AWI_ST_SELFTEST;
	printf(": BayStack 650 Wireless (802.11)\n");
	
#if 0
	for (i=0; i<cfe->num_memspace; i++) {
		printf("memspace %d: len 0x%lx, cardaddr 0x%lx"
		    ", hostaddr 0x%lx\n",
		    i,
		    cfe->memspace[i].length,
		    cfe->memspace[i].cardaddr,
		    cfe->memspace[i].hostaddr);
	}
#endif
	
	if (pcmcia_mem_alloc(psc->sc_pf, AM79C930_MEM_SIZE, &memh) != 0) {
		printf(": unable to allocate memory space\n");
		return;		/* XXX */
	}
	if (pcmcia_mem_map(psc->sc_pf, PCMCIA_MEM_COMMON,
	    AM79C930_MEM_BASE, AM79C930_MEM_SIZE,
	    &memh, &memoff, &memwin)) {
		printf(": unable to map memory space\n");
		return;		/* XXX */
	}
	sc->sc_chip.sc_memt = memh.memt;
	sc->sc_chip.sc_memh = memh.memh;

#if 0
	if (pcmcia_io_alloc(pa->pf, AM79C930_IO_BASE, AM79C930_IO_SIZE,
	    AM79C930_IO_SIZE, &psc->sc_pcioh) != 0) {
		printf(": unable to allocate i/o space\n");
		return;		/* XXX */
	}
	
	if (pcmcia_io_map(pa->pf, PCMCIA_WIDTH_AUTO, 0, psc->sc_pcioh.size,
	    &psc->sc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return;
	}
	
	sc->sc_chip.sc_iot = psc->sc_pcioh.iot;
	sc->sc_chip.sc_ioh = psc->sc_pcioh.ioh;
#endif

	sc->sc_chip.sc_bustype = AM79C930_BUS_PCMCIA;
	
	sc->sc_enable = awi_pcmcia_enable;
	sc->sc_disable = awi_pcmcia_disable;

	awi_attach(sc);

	sc->sc_state = AWI_ST_OFF;

	sc->sc_enabled = 0;
	pcmcia_function_disable(psc->sc_pf);
}


int
awi_pcmcia_detach(self, flags)
	struct device *self;
	int flags;
{
	struct awi_pcmcia_softc *psc = (struct awi_pcmcia_softc *)self;

	/* Unmap our i/o window. */
	pcmcia_io_unmap(psc->sc_pf, psc->sc_io_window);

	/* Free our i/o space. */
	pcmcia_io_free(psc->sc_pf, &psc->sc_pcioh);

#ifdef notyet
	/*
	 * Our softc is about to go away, so drop our reference
	 * to the ifnet.
	 */
	if_delref(psc->sc_awi.sc_ethercom.ec_if);
	return (0);
#else
	return (EBUSY);
#endif
}
