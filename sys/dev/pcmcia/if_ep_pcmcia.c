/*	$NetBSD: if_ep_pcmcia.c,v 1.12 1998/07/20 02:17:17 mellon Exp $	*/

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
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

#include <dev/ic/elink3var.h>
#include <dev/ic/elink3reg.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

int	ep_pcmcia_match __P((struct device *, struct cfdata *, void *));
void	ep_pcmcia_attach __P((struct device *, struct device *, void *));

int	ep_pcmcia_get_enaddr __P((struct pcmcia_tuple *, void *));
int	ep_pcmcia_enable __P((struct ep_softc *));
void	ep_pcmcia_disable __P((struct ep_softc *));

int	ep_pcmcia_enable1 __P((struct ep_softc *));
void	ep_pcmcia_disable1 __P((struct ep_softc *));

struct ep_pcmcia_softc {
	struct ep_softc sc_ep;			/* real "ep" softc */

	/* PCMCIA-specific goo */
	struct pcmcia_io_handle sc_pcioh;	/* PCMCIA i/o space info */
	int sc_io_window;			/* our i/o window */
	struct pcmcia_function *sc_pf;		/* our PCMCIA function */
};

struct cfattach ep_pcmcia_ca = {
	sizeof(struct ep_pcmcia_softc), ep_pcmcia_match, ep_pcmcia_attach
};

int
ep_pcmcia_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_3COM) {
		switch (pa->product) {
		case PCMCIA_PRODUCT_3COM_3C562:
		case PCMCIA_PRODUCT_3COM_3C589:
			if (pa->pf->number == 0)
				return (1);
		}
	}

	return (0);
}

int
ep_pcmcia_enable(sc)
	struct ep_softc *sc;
{
	struct ep_pcmcia_softc *psc = (struct ep_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;

	/* establish the interrupt. */
	sc->sc_ih = pcmcia_intr_establish(pf, IPL_NET, epintr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	return (ep_pcmcia_enable1(sc));
}

int
ep_pcmcia_enable1(sc)
	struct ep_softc *sc;
{
	struct ep_pcmcia_softc *psc = (struct ep_pcmcia_softc *) sc;
	struct pcmcia_function *pf = psc->sc_pf;
	int ret;

	if ((ret = pcmcia_function_enable(pf)))
		return (ret);

	if (psc->sc_pf->sc->card.product == PCMCIA_PRODUCT_3COM_3C562) {
		int reg;

		/* turn off the serial-disable bit */

		reg = pcmcia_ccr_read(pf, PCMCIA_CCR_OPTION);
		if (reg & 0x08) {
			reg &= ~0x08;
			pcmcia_ccr_write(pf, PCMCIA_CCR_OPTION, reg);
		}

	}

	return (ret);
}

void
ep_pcmcia_disable(sc)
	struct ep_softc *sc;
{
	struct ep_pcmcia_softc *psc = (struct ep_pcmcia_softc *) sc;

	ep_pcmcia_disable1(sc);
	pcmcia_intr_disestablish(psc->sc_pf, sc->sc_ih);
}

void
ep_pcmcia_disable1(sc)
	struct ep_softc *sc;
{
	struct ep_pcmcia_softc *psc = (struct ep_pcmcia_softc *) sc;

	pcmcia_function_disable(psc->sc_pf);
}

void
ep_pcmcia_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct ep_pcmcia_softc *psc = (void *) self;
	struct ep_softc *sc = &psc->sc_ep;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_config_entry *cfe;
	int i;
	u_int8_t myla[ETHER_ADDR_LEN];
	u_int8_t *enaddr;
	char *model;

	psc->sc_pf = pa->pf;
	cfe = pa->pf->cfe_head.sqh_first;

	/* Enable the card. */
	pcmcia_function_init(pa->pf, cfe);
	if (ep_pcmcia_enable1(sc))
		printf(": function enable failed\n");

	sc->enabled = 1;

	if (cfe->num_memspace != 0)
		printf(": unexpected number of memory spaces %d should be 0\n",
		    cfe->num_memspace);

	if (cfe->num_iospace != 1)
		printf(": unexpected number of I/O spaces %d should be 1\n",
		    cfe->num_iospace);

	if (pa->product == PCMCIA_PRODUCT_3COM_3C562) {
		bus_addr_t maxaddr = (pa->pf->sc->iobase + pa->pf->sc->iosize);

		for (i = pa->pf->sc->iobase; i < maxaddr; i += 0x10) {
			/*
			 * the 3c562 can only use 0x??00-0x??7f
			 * according to the Linux driver
			 */
			if (i & 0x80)
				continue;
			if (pcmcia_io_alloc(pa->pf, i, cfe->iospace[0].length,
			    0, &psc->sc_pcioh) == 0)
				break;
		}
		if (i >= maxaddr) {
			printf(": can't allocate i/o space\n");
			return;
		}
	} else {
		if (pcmcia_io_alloc(pa->pf, 0, cfe->iospace[0].length,
		    cfe->iospace[0].length, &psc->sc_pcioh))
			printf(": can't allocate i/o space\n");
	}

	sc->sc_iot = psc->sc_pcioh.iot;
	sc->sc_ioh = psc->sc_pcioh.ioh;

	if (pcmcia_io_map(pa->pf, ((cfe->flags & PCMCIA_CFE_IO16) ?
	    PCMCIA_WIDTH_IO16 : PCMCIA_WIDTH_IO8), 0, cfe->iospace[0].length,
	    &psc->sc_pcioh, &psc->sc_io_window)) {
		printf(": can't map i/o space\n");
		return;
	}
	if (pa->product == PCMCIA_PRODUCT_3COM_3C562) {
		/*
		 * 3c562a-c use this; 3c562d does it in the regular way.
		 * we might want to check the revision and produce a warning
		 * in the future.
		 */
		if (pcmcia_scan_cis(parent, ep_pcmcia_get_enaddr, myla) != 1)
			enaddr = NULL;
		else
			enaddr = myla;
	} else {
		enaddr = NULL;
	}

	sc->bustype = EP_BUS_PCMCIA;

	switch (pa->product) {
	case PCMCIA_PRODUCT_3COM_3C589:
		model = PCMCIA_STR_3COM_3C589;
		break;
	case PCMCIA_PRODUCT_3COM_3C562:
		model = PCMCIA_STR_3COM_3C562;
		break;
	default:
		model = "3Com Ethernet, model unknown";
		break;
	}

	printf(": %s\n", model);

	sc->enable = ep_pcmcia_enable;
	sc->disable = ep_pcmcia_disable;

	epconfig(sc, EP_CHIPSET_3C509, enaddr);

	sc->enabled = 0;

	ep_pcmcia_disable1(sc);
}

int
ep_pcmcia_get_enaddr(tuple, arg)
	struct pcmcia_tuple *tuple;
	void *arg;
{
	u_int8_t *myla = arg;
	int i;

	/* this is 3c562a-c magic */
	if (tuple->code == 0x88) {
		if (tuple->length < ETHER_ADDR_LEN)
			return (0);

		for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
			myla[i] = pcmcia_tuple_read_1(tuple, i + 1);
			myla[i + 1] = pcmcia_tuple_read_1(tuple, i);
		}

		return (1);
	}
	return (0);
}
