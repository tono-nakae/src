/*
 * Copyright (c) 1993 Herb Peyerl <hpeyerl@novatel.ca>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *	$Id: if_ep.c,v 1.31 1994/04/16 09:53:45 deraadt Exp $
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/select.h>
#include <sys/device.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
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
#include <machine/pio.h>

#include <i386/isa/isavar.h>
#include <i386/isa/if_epreg.h>
#include <i386/isa/elink.h>

#define ETHER_MIN_LEN 64
#define ETHER_MAX_LEN   1518
#define ETHER_ADDR_LEN  6
/*
 * Ethernet software status per interface.
 */
struct ep_softc {
	struct device sc_dev;
	struct intrhand sc_ih;

	struct arpcom ep_ac;		/* Ethernet common part		*/
	short   ep_iobase;		/* i/o bus address		*/
	char    ep_connectors;		/* Connectors on this card.	*/
#define MAX_MBS  4			/* # of mbufs we keep around	*/
	struct mbuf *mb[MAX_MBS];	/* spare mbuf storage.		*/
	int	next_mb;		/* Which mbuf to use next. 	*/
	int	last_mb;		/* Last mbuf.			*/
	int	tx_start_thresh;	/* Current TX_start_thresh.	*/
	caddr_t bpf;			/* BPF  "magic cookie"		*/
	char	bus32bit;		/* 32bit access possible */
};

static int epprobe();
static void epattach();

struct cfdriver epcd = {
	NULL, "ep", epprobe, epattach, DV_IFNET, sizeof(struct ep_softc)
};

int epintr __P((struct ep_softc *));
static void epinit __P((struct ep_softc *));
static int epioctl __P((struct ifnet *, int, caddr_t));
static int epstart __P((struct ifnet *));
static int epwatchdog __P((int));
static void epreset __P((struct ep_softc *));
static void epread __P((struct ep_softc *));
static void epmbuffill __P((struct ep_softc *));
static void epmbufempty __P((struct ep_softc *));
static void epstop __P((struct ep_softc *));
static void epsetfilter __P((struct ep_softc *sc));
static void epsetlink __P((struct ep_softc *sc));

static u_short epreadeeprom __P((int id_port, int offset));
static int epbusyeeprom __P((struct ep_softc *));

#define MAXEPCARDS 20	/* if you have 21 cards in your machine... you lose */

static struct epcard {
	u_short	port;
	u_short	irq;
	char	available;
	char	bus32bit;
} epcards[MAXEPCARDS];
static int nepcards;

static void
epaddcard(p, i, mode)
	short p;
	u_short i;
	char mode;
{
	if (nepcards >= sizeof(epcards)/sizeof(epcards[0]))
		return;
	epcards[nepcards].port = p;
	epcards[nepcards].irq = 1 << ((i == 2) ? 9 : i);
	epcards[nepcards].available = 1;
	epcards[nepcards].bus32bit = mode;
	nepcards++;
}
	
/*
 * 3c579 cards on the EISA bus are probed by their slot number. 3c509
 * cards on the ISA bus are probed in ethernet address order. The probe
 * sequence requires careful orchestration, and we'd like like to allow
 * the irq and base address to be wildcarded. So, we probe all the cards
 * the first time epprobe() is called. On subsequent calls we look for
 * matching cards.
 */
int
epprobe(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	static int firsttime;
	int slot, port, i;
	u_short k, k2;

	if (firsttime==0) {
		firsttime = 1;

		/* find all EISA cards */
		for (slot = 1; slot < 8; slot++) {
			port = 0x1000 * slot;
			outw(port + EP_COMMAND, GLOBAL_RESET);
			delay(1000);
			if (inw(port + EP_W0_MFG_ID) != MFG_ID)
				continue;
			k = inw(port + EP_W0_PRODUCT_ID);
			if ((k & 0xf0ff) != (PROD_ID & 0xf0ff))
				continue;
			k = inw(port + EP_W0_ADDRESS_CFG);
			k = (k & 0x1f) * 0x10 + 0x200;
			k2 = inw(port + EP_W0_RESOURCE_CFG);
			k2 >>= 12;
			epaddcard(port, k2, 0);
		}

		/* find all isa cards */
		outw(BASE + EP_COMMAND, GLOBAL_RESET);
		delay(1000);
		elink_reset();	/* global reset to ELINK_ID_PORT */
		delay(1000);

		for (slot = 0; slot < 10; slot++) {
			outb(ELINK_ID_PORT, 0x00);
			elink_idseq(ELINK_509_POLY);
			delay(1000);

			k = epreadeeprom(ELINK_ID_PORT, EEPROM_MFG_ID);
			if (k == 0xff)
				continue;	/* no more isa cards */
			if (k != MFG_ID)
				continue;
			k = epreadeeprom(ELINK_ID_PORT, EEPROM_PROD_ID);
			if ((k & 0xf0ff) != (PROD_ID & 0xf0ff))
				continue;

			k = epreadeeprom(ELINK_ID_PORT, EEPROM_ADDR_CFG);
			k = (k & 0x1f) * 0x10 + 0x200;

			k2 = epreadeeprom(ELINK_ID_PORT, EEPROM_RESOURCE_CFG);
			k2 >>= 12;
			epaddcard(k, k2, 0);

			/* so card will not respond to contention again */
			outb(ELINK_ID_PORT, TAG_ADAPTER_0 + 1);

			/*
			 * XXX: this should probably not be done here
			 * because it enables the drq/irq lines from
			 * the board. Perhaps it should be done after
			 * we have checked for irq/drq collisions?
			 */
			outb(ELINK_ID_PORT, ACTIVATE_ADAPTER_TO_CONFIG);
		}
		/* XXX should we sort by ethernet address? */
	}

	/*
	 * a very specific search order:
	 * 	exact port & irq
	 * 	exact port, wildcard irq
	 * 	wildcard port, exact irq
	 *	wildcard port & irq
	 * else fail..
	 */
	if (ia->ia_iobase != (u_short)-1 || ia->ia_irq != (u_short)-1) {
		for (i = 0; i<nepcards; i++) {
			if (epcards[i].available == 0)
				continue;
			if (ia->ia_iobase == epcards[i].port &&
			    ia->ia_irq == epcards[i].irq)
				goto good;
		}
	}
	if (ia->ia_iobase != (u_short)-1 && ia->ia_irq == (u_short)-1) {
		for (i = 0; i<nepcards; i++) {
			if (epcards[i].available == 0)
				continue;
			if (ia->ia_iobase == epcards[i].port)
				goto good;
		}
	}
	if (ia->ia_iobase == (u_short)-1 && ia->ia_irq != (u_short)-1) {
		for (i = 0; i<nepcards; i++) {
			if (epcards[i].available == 0)
				continue;
			if (ia->ia_irq == epcards[i].irq)
				goto good;
		}
	}
	for (i = 0; i<nepcards; i++)
		if (epcards[i].available != 0) {
			goto good;
	}
	return 0;

good:
	epcards[i].available = 0;
	sc->bus32bit = epcards[i].bus32bit;
	ia->ia_iobase = epcards[i].port;
	ia->ia_irq = epcards[i].irq;
	ia->ia_iosize = 0x10;
	ia->ia_msize = 0;
	return 1;
}

static void
epattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ep_softc *sc = (void *)self;
	struct isa_attach_args *ia = aux;
	struct ifnet *ifp = &sc->ep_ac.ac_if;
	struct ifaddr *ifa;
	struct sockaddr_dl *sdl;
	u_short i;

	printf(": ");

	sc->ep_iobase = ia->ia_iobase;

	sc->ep_connectors = 0;
	i = inw(ia->ia_iobase + EP_W0_CONFIG_CTRL);
	if (i & IS_AUI) {
		printf("aui");
		sc->ep_connectors |= AUI;
	}
	if (i & IS_BNC) {
		if (sc->ep_connectors)
			printf("/");
		printf("bnc");
		sc->ep_connectors |= BNC;
	}
	if (i & IS_UTP) {
		if (sc->ep_connectors)
			printf("/");
		printf("utp");
		sc->ep_connectors |= UTP;
	}
	if (!sc->ep_connectors)
		printf("no connectors!");

	/*
	 * Read the station address from the eeprom
	 */
	for (i = 0; i < 3; i++) {
		u_short *p;
		GO_WINDOW(0);
		if (epbusyeeprom(sc))
			return;
		outw(BASE + EP_W0_EEPROM_COMMAND, READ_EEPROM | i);
		if (epbusyeeprom(sc))
			return;
		p = (u_short *) & sc->ep_ac.ac_enaddr[i * 2];
		*p = htons(inw(BASE + EP_W0_EEPROM_DATA));
		GO_WINDOW(2);
		outw(BASE + EP_W2_ADDR_0 + (i * 2), ntohs(*p));
	}
	printf(" address %s\n", ether_sprintf(sc->ep_ac.ac_enaddr));

	ifp->if_unit = sc->sc_dev.dv_unit;
	ifp->if_name = epcd.cd_name;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS |
	    IFF_MULTICAST;
	ifp->if_output = ether_output;
	ifp->if_start = epstart;
	ifp->if_ioctl = epioctl;
	ifp->if_watchdog = epwatchdog;

	if_attach(ifp);

	/*
	 * Search down the ifa address list looking for the AF_LINK type entry
	 * and initialize it.
	 */
	ifa = ifp->if_addrlist;
	while (ifa && ifa->ifa_addr) {
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			/*
			 * Fill in the link-level address for this interface.
			 */
			sdl = (struct sockaddr_dl *) ifa->ifa_addr;
			sdl->sdl_type = IFT_ETHER;
			sdl->sdl_alen = ETHER_ADDR_LEN;
			sdl->sdl_slen = 0;
			bcopy(sc->ep_ac.ac_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);
			break;
		} else
			ifa = ifa->ifa_next;
	}

#if NBPFILTER > 0
	bpfattach(&sc->bpf, ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif

	sc->sc_ih.ih_fun = epintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_level = IPL_NET;
	intr_establish(ia->ia_irq, &sc->sc_ih);
}

/*
 * The order in here seems important. Otherwise we may not receive
 * interrupts. ?!
 */
static void
epinit(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->ep_ac.ac_if;
	int     s, i;

	if (ifp->if_addrlist == 0)
		return;

	s = splimp();
	while (inb(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;

	GO_WINDOW(0);
	outw(BASE + EP_W0_CONFIG_CTRL, 0);	/* Disable the card */

	outw(BASE + EP_W0_CONFIG_CTRL, ENABLE_DRQ_IRQ);	/* Enable the card */

	GO_WINDOW(2);
	for (i = 0; i < 6; i++)	/* Reload the ether_addr. */
		outb(BASE + EP_W2_ADDR_0 + i, sc->ep_ac.ac_enaddr[i]);

	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);

	GO_WINDOW(1);		/* Window 1 is operating window */
	for (i = 0; i < 31; i++)
		inb(BASE + EP_W1_TX_STATUS);

	outw(BASE + EP_COMMAND, ACK_INTR | 0xff); /* get rid of stray intr's */

	outw(BASE + EP_COMMAND, SET_RD_0_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
	    S_TX_COMPLETE | S_TX_AVAIL);
	outw(BASE + EP_COMMAND, SET_INTR_MASK | S_CARD_FAILURE | S_RX_COMPLETE |
	    S_TX_COMPLETE | S_TX_AVAIL);

	epsetfilter(sc);
	epsetlink(sc);

	outw(BASE + EP_COMMAND, RX_ENABLE);
	outw(BASE + EP_COMMAND, TX_ENABLE);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;	/* just in case */
	sc->tx_start_thresh = 20;	/* probably a good starting point. */
	/* Store up a bunch of mbuf's for use later. (MAX_MBS). */
	epmbuffill(sc);

	epstart(ifp);
	splx(s);
}

static void
epsetfilter(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->ep_ac.ac_if;

	GO_WINDOW(1);		/* Window 1 is operating window */
	outw(BASE + EP_COMMAND, SET_RX_FILTER |
	    FIL_INDIVIDUAL | FIL_BRDCST |
	    ((ifp->if_flags & IFF_MULTICAST) ? FIL_MULTICAST : 0 ) |
	    ((ifp->if_flags & IFF_PROMISC) ? FIL_PROMISC : 0 ));
}

static void
epsetlink(sc)
	register struct ep_softc *sc;
{
	register struct ifnet *ifp = &sc->ep_ac.ac_if;

	/*
	 * you can `ifconfig (link0|-link0) ep0' to get the following
	 * behaviour:
	 *	-link0	disable AUI/UTP. enable BNC.
	 *	link0	disable BNC. enable AUI.
	 *	link1	if the card has a UTP connector, and link0 is
	 *		set too, then you get the UTP port.
	 */
	GO_WINDOW(4);
	outw(BASE + EP_W4_MEDIA_TYPE, DISABLE_UTP);
	GO_WINDOW(1);
	if (!(ifp->if_flags & IFF_LINK0) && (sc->ep_connectors & BNC)) {
		outw(BASE + EP_COMMAND, START_TRANSCEIVER);
		delay(1000);
	}
	if (ifp->if_flags & IFF_LINK0) {
		outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
		delay(1000);
		if((ifp->if_flags & IFF_LINK1) && (sc->ep_connectors & UTP)) {
			GO_WINDOW(4);
			outw(BASE + EP_W4_MEDIA_TYPE, ENABLE_UTP);
			GO_WINDOW(1);
		}
	}
}

static const char padmap[] = {0, 3, 2, 1};

static int
epstart(ifp)
	struct ifnet *ifp;
{
	register struct ep_softc *sc = epcd.cd_devs[ifp->if_unit];
	struct mbuf *m, *top;
	int     s, len, pad;

	s = splimp();
	if (sc->ep_ac.ac_if.if_flags & IFF_OACTIVE) {
		splx(s);
		return (0);
	}

startagain:
	/* Sneak a peek at the next packet */
	m = sc->ep_ac.ac_if.if_snd.ifq_head;
	if (m == 0) {
		splx(s);
		return (0);
	}
#if 0
	len = m->m_pkthdr.len;
#else
	for (len = 0, top = m; m; m = m->m_next)
		len += m->m_len;
#endif

	pad = padmap[len & 3];

	/*
	 * The 3c509 automatically pads short packets to minimum ethernet
	 * length, but we drop packets that are too large. Perhaps we should
	 * truncate them instead?
	 */
	if (len + pad > ETHER_MAX_LEN) {
		/* packet is obviously too large: toss it */
		+sc->ep_ac.ac_if.if_oerrors;
		IF_DEQUEUE(&sc->ep_ac.ac_if.if_snd, m);
		m_freem(m);
		goto readcheck;
	}

	if (inw(BASE + EP_W1_FREE_TX) < len + pad + 4) {
		/* no room in FIFO */
		outw(BASE + EP_COMMAND, SET_TX_AVAIL_THRESH | (len + pad + 4));
		sc->ep_ac.ac_if.if_flags |= IFF_OACTIVE;
		splx(s);
		return (0);
	}
	IF_DEQUEUE(&sc->ep_ac.ac_if.if_snd, m);
	if (m == 0) {		/* not really needed */
		splx(s);
		return (0);
	}
	outw(BASE + EP_COMMAND, SET_TX_START_THRESH |
	    (len / 4 + sc->tx_start_thresh));

	outw(BASE + EP_W1_TX_PIO_WR_1, len);
	outw(BASE + EP_W1_TX_PIO_WR_1, 0xffff);	/* Second dword meaningless */

	for (top = m; m != 0; m = m->m_next) {
		if (sc->bus32bit) {
			outsl(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t),
			    m->m_len/4);
			if (m->m_len & 3)
				outsb(BASE + EP_W1_TX_PIO_WR_1,
				    mtod(m, caddr_t) + m->m_len/4,
				    m->m_len & 3);
		} else {
			outsw(BASE + EP_W1_TX_PIO_WR_1, mtod(m, caddr_t), m->m_len/2);
			if (m->m_len & 1)
				outb(BASE + EP_W1_TX_PIO_WR_1,
				    *(mtod(m, caddr_t) + m->m_len - 1));
		}
	}
	while (pad--)
		outb(BASE + EP_W1_TX_PIO_WR_1, 0);	/* Padding */

#if NBPFILTER > 0
	if (sc->bpf) {
		u_short etype;
		int     off, datasize, resid;
		struct ether_header *eh;
		struct trailer_header {
			u_short ether_type;
			u_short ether_residual;
		}       trailer_header;
		char    ether_packet[ETHER_MAX_LEN];
		char   *ep;

		ep = ether_packet;

		/*
		 * We handle trailers below:
		 * Copy ether header first, then residual data,
		 * then data. Put all this in a temporary buffer
		 * 'ether_packet' and send off to bpf. Since the
		 * system has generated this packet, we assume
		 * that all of the offsets in the packet are
		 * correct; if they're not, the system will almost
		 * certainly crash in m_copydata.
		 * We make no assumptions about how the data is
		 * arranged in the mbuf chain (i.e. how much
		 * data is in each mbuf, if mbuf clusters are
		 * used, etc.), which is why we use m_copydata
		 * to get the ether header rather than assume
		 * that this is located in the first mbuf.
		 */
		/* copy ether header */
		m_copydata(top, 0, sizeof(struct ether_header), ep);
		eh = (struct ether_header *) ep;
		ep += sizeof(struct ether_header);
		etype = ntohs(eh->ether_type);
		if (etype >= ETHERTYPE_TRAIL &&
		    etype < ETHERTYPE_TRAIL + ETHERTYPE_NTRAILER) {
			datasize = ((etype - ETHERTYPE_TRAIL) << 9);
			off = datasize + sizeof(struct ether_header);

			/* copy trailer_header into a data structure */
			m_copydata(top, off, sizeof(struct trailer_header),
			    &trailer_header.ether_type);

			/* copy residual data */
			resid = trailer_header.ether_residual -
			    sizeof(struct trailer_header);
			resid = ntohs(resid);
			m_copydata(top, off + sizeof(struct trailer_header),
			    resid, ep);
			ep += resid;

			/* copy data */
			m_copydata(top, sizeof(struct ether_header),
			    datasize, ep);
			ep += datasize;

			/* restore original ether packet type */
			eh->ether_type = trailer_header.ether_type;

			bpf_tap(sc->bpf, ether_packet, ep - ether_packet);
		} else
			bpf_mtap(sc->bpf, top);
	}
#endif

	m_freem(top);
	++sc->ep_ac.ac_if.if_opackets;

	/*
	 * Is another packet coming in? We don't want to overflow the
	 * tiny RX fifo.
	 */
readcheck:
	if (inw(BASE + EP_W1_RX_STATUS) & RX_BYTES_MASK) {
		splx(s);
		return (0);
	}
	goto startagain;
}

int
epintr(sc)
	register struct ep_softc *sc;
{
	int     status, i;
	struct ifnet *ifp = &sc->ep_ac.ac_if;

	status = 0;
	status = inw(BASE + EP_STATUS) &
	    (S_TX_COMPLETE | S_TX_AVAIL | S_RX_COMPLETE | S_CARD_FAILURE);
	if (status == 0) {
		/* No interrupts. */
		outw(BASE + EP_COMMAND, C_INTR_LATCH);
		return (0);
	}
loop:
	/* important that we do this first. */
	outw(BASE + EP_COMMAND, ACK_INTR | status);

	if (status & S_TX_AVAIL) {
		status &= ~S_TX_AVAIL;
		inw(BASE + EP_W1_FREE_TX);
		sc->ep_ac.ac_if.if_flags &= ~IFF_OACTIVE;
		epstart(&sc->ep_ac.ac_if);
	}
	if (status & S_RX_COMPLETE) {
		status &= ~S_RX_COMPLETE;
		epread(sc);
	}
	if (status & S_CARD_FAILURE) {
		printf("%s: reset (status: %x)\n", sc->sc_dev.dv_xname, status);
		outw(BASE + EP_COMMAND, C_INTR_LATCH);
		epinit(sc);
		return (1);
	}
	if (status & S_TX_COMPLETE) {
		status &= ~S_TX_COMPLETE;
		/*
		 * We need to read TX_STATUS until we get a 0 status in
		 * order to turn off the interrupt flag.
		 */
		while ((i = inb(BASE + EP_W1_TX_STATUS)) & TXS_COMPLETE) {
			outw(BASE + EP_W1_TX_STATUS, 0x0);
			if (i & (TXS_MAX_COLLISION | TXS_JABBER | TXS_UNDERRUN)) {
				if (i & TXS_MAX_COLLISION)
					++sc->ep_ac.ac_if.if_collisions;
				if (i & (TXS_JABBER | TXS_UNDERRUN)) {
					outw(BASE + EP_COMMAND, TX_RESET);
					if (i & TXS_UNDERRUN) {
						if (sc->tx_start_thresh < ETHER_MAX_LEN) {
							sc->tx_start_thresh += 20;
							outw(BASE + EP_COMMAND,
							    SET_TX_START_THRESH |
							    sc->tx_start_thresh);
						}
					}
				}
				outw(BASE + EP_COMMAND, TX_ENABLE);
				++sc->ep_ac.ac_if.if_oerrors;
			}
		}
		epstart(ifp);
	}
	status = inw(BASE + EP_STATUS) &
	    (S_TX_COMPLETE | S_TX_AVAIL | S_RX_COMPLETE | S_CARD_FAILURE);
	if (status == 0) {
		/* No interrupts. */
		outw(BASE + EP_COMMAND, C_INTR_LATCH);
		return (1);
	}
	goto loop;
}

static void
epread(sc)
	register struct ep_softc *sc;
{
	struct ether_header *eh;
	struct mbuf *mcur, *m, *m0, *top;
	int     totlen, lenthisone;
	int     save_totlen, off;
	u_short etype;

	totlen = inw(BASE + EP_W1_RX_STATUS);
	off = 0;
	top = 0;

	if (totlen & ERR_RX) {
		++sc->ep_ac.ac_if.if_ierrors;
		goto out;
	}
	save_totlen = totlen &= RX_BYTES_MASK;	/* Lower 11 bits = RX bytes. */

	m = sc->mb[sc->next_mb];
	sc->mb[sc->next_mb] = 0;

	if (m == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			goto out;
	} else {		/* Convert one of our saved mbuf's */
		sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
		m->m_data = m->m_pktdat;
		m->m_flags = M_PKTHDR;
	}

	top = m0 = m;		/* We assign top so we can "goto out" */
#define EROUND  ((sizeof(struct ether_header) + 3) & ~3)
#define EOFF    (EROUND - sizeof(struct ether_header))
	m0->m_data += EOFF;
	/* Read what should be the header. */
	insw(BASE + EP_W1_RX_PIO_RD_1,
	    mtod(m0, caddr_t), sizeof(struct ether_header) / 2);
	m->m_len = sizeof(struct ether_header);
	totlen -= sizeof(struct ether_header);
	/*
 	 * mostly deal with trailer here.  (untested)
	 * We do this in a couple of parts.  First we check for a trailer, if
	 * we have one we convert the mbuf back to a regular mbuf and set the offset and
	 * subtract sizeof(struct ether_header) from the pktlen.
	 * After we've read the packet off the interface (all except for the trailer
	 * header, we then get a header mbuf, read the trailer into it, and fix up
	 * the mbuf pointer chain.
	 */
	eh = mtod(m, struct ether_header *);
	etype = ntohs((u_short) eh->ether_type);
	if (etype >= ETHERTYPE_TRAIL &&
	    etype < ETHERTYPE_TRAIL + ETHERTYPE_NTRAILER) {
		m->m_data = m->m_dat;	/* Convert back to regular mbuf.  */
		m->m_flags = 0;		/* This sucks but non-trailers are the norm */
		off = (etype - ETHERTYPE_TRAIL) * 512;
		if (off >= ETHERMTU) {
			m_freem(m);
			return;	/* sanity */
		}
		totlen -= sizeof(struct ether_header);	/* We don't read the trailer */
		m->m_data += 2 * sizeof(u_short);	/* Get rid of type & len */
	}
	while (totlen > 0) {
		lenthisone = min(totlen, M_TRAILINGSPACE(m));
		if (lenthisone == 0) {	/* no room in this one */
			mcur = m;
			m = sc->mb[sc->next_mb];
			sc->mb[sc->next_mb] = 0;
			if (!m) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == 0)
					goto out;
			} else {
				timeout(epmbuffill, sc, 1);
				sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			}
			if (totlen >= MINCLSIZE)
				MCLGET(m, M_DONTWAIT);
			m->m_len = 0;
			mcur->m_next = m;
			lenthisone = min(totlen, M_TRAILINGSPACE(m));
		}
		if (sc->bus32bit) {
			insl(BASE + EP_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
			    lenthisone / 4);
			m->m_len += (lenthisone & ~3);
			if (lenthisone & 3)
				insb(BASE + EP_W1_RX_PIO_RD_1,
				    mtod(m, caddr_t) + m->m_len,
				    lenthisone & 3);
			m->m_len += (lenthisone & 3);
		} else {
			insw(BASE + EP_W1_RX_PIO_RD_1, mtod(m, caddr_t) + m->m_len,
			    lenthisone / 2);
			m->m_len += lenthisone;
			if (lenthisone & 1)
				*(mtod(m, caddr_t) + m->m_len - 1) = inb(BASE + EP_W1_RX_PIO_RD_1);
		}
		totlen -= lenthisone;
	}
	if (off) {
		top = sc->mb[sc->next_mb];
		sc->mb[sc->next_mb] = 0;
		if (top == 0) {
			MGETHDR(top, M_DONTWAIT, MT_DATA);
			if (top == 0)
				goto out;
		} else {	/* Convert one of our saved mbuf's */
			sc->next_mb = (sc->next_mb + 1) % MAX_MBS;
			top->m_data = top->m_pktdat;
			top->m_flags = M_PKTHDR;
		}
		insw(BASE + EP_W1_RX_PIO_RD_1, mtod(top, caddr_t),
		    sizeof(struct ether_header));
		top->m_next = m0;
		top->m_len = sizeof(struct ether_header);
		/* XXX Accomodate for type and len from beginning of trailer */
		top->m_pkthdr.len = save_totlen - (2 * sizeof(u_short));
	} else {
		top = m0;
		top->m_pkthdr.len = save_totlen;
	}

	top->m_pkthdr.rcvif = &sc->ep_ac.ac_if;
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inb(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
	++sc->ep_ac.ac_if.if_ipackets;
#if NBPFILTER > 0
	if (sc->bpf) {
		bpf_mtap(sc->bpf, top);

		/*
		 * Note that the interface cannot be in promiscuous mode if
		 * there are no BPF listeners.  And if we are in promiscuous
		 * mode, we have to check if this packet is really ours.
		 */
		if ((sc->ep_ac.ac_if.if_flags & IFF_PROMISC) &&
		    (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost, sc->ep_ac.ac_enaddr,
			 sizeof(eh->ether_dhost)) != 0 &&
		    bcmp(eh->ether_dhost, etherbroadcastaddr,
			 sizeof(eh->ether_dhost)) != 0) {
			m_freem(top);
			return;
		}
	}
#endif
	m_adj(top, sizeof(struct ether_header));
	ether_input(&sc->ep_ac.ac_if, eh, top);
	return;

out:	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inb(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
	if (top)
		m_freem(top);
}


/*
 * Look familiar?
 */
static int
epioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	int     cmd;
	caddr_t data;
{
	register struct ifaddr *ifa = (struct ifaddr *) data;
	struct ep_softc *sc = epcd.cd_devs[ifp->if_unit];
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			epinit(sc);	/* before arpwhohas */
			((struct arpcom *) ifp)->ac_ipaddr = IA_SIN(ifa)->sin_addr;
			arpwhohas((struct arpcom *) ifp, &IA_SIN(ifa)->sin_addr);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			if (ns_nullhost(*ina))
				ina->x_host =
				    *(union ns_host *)(sc->ep_ac.ac_enaddr);
			else {
				ifp->if_flags &= ~IFF_RUNNING;
				bcopy(ina->x_host.c_host,
				    sc->ep_ac.ac_enaddr,
				    sizeof(sc->ep_ac.ac_enaddr));
			}
			epinit(sc);
			break;
		    }
#endif
		default:
			epinit(sc);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			ifp->if_flags &= ~IFF_RUNNING;
			epstop(sc);
			epmbufempty(sc);
			break;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    (ifp->if_flags & IFF_RUNNING) == 0) {
			epinit(sc);
		} else {
			/*
			 * deal with flags changes:
			 * IFF_MULTICAST, IFF_PROMISC,
			 * IFF_LINK0, IFF_LINK1,
			 */
			epsetfilter(sc);
			epsetlink(sc);
		}
		break;
#ifdef notdef
	case SIOCGHWADDR:
		bcopy((caddr_t) sc->sc_addr, (caddr_t) &ifr->ifr_data,
		    sizeof(sc->sc_addr));
		break;
#endif
	default:
		error = EINVAL;
	}
	return (error);
}

static void
epreset(sc)
	struct ep_softc *sc;
{
	int s = splimp();

	epstop(sc);
	epinit(sc);
	splx(s);
}

static int
epwatchdog(unit)
	int     unit;
{
	register struct ep_softc *sc = epcd.cd_devs[unit];

	log(LOG_ERR, "%s: watchdog\n", sc->sc_dev.dv_xname);
	epreset(sc);
	return 0;
}

static void
epstop(sc)
	register struct ep_softc *sc;
{

	outw(BASE + EP_COMMAND, RX_DISABLE);
	outw(BASE + EP_COMMAND, RX_DISCARD_TOP_PACK);
	while (inb(BASE + EP_STATUS) & S_COMMAND_IN_PROGRESS)
		;
	outw(BASE + EP_COMMAND, TX_DISABLE);
	outw(BASE + EP_COMMAND, STOP_TRANSCEIVER);
	outw(BASE + EP_COMMAND, RX_RESET);
	outw(BASE + EP_COMMAND, TX_RESET);
	outw(BASE + EP_COMMAND, C_INTR_LATCH);
	outw(BASE + EP_COMMAND, SET_RD_0_MASK);
	outw(BASE + EP_COMMAND, SET_INTR_MASK);
	outw(BASE + EP_COMMAND, SET_RX_FILTER);
}

/*
 * We get eeprom data from the id_port given an offset into the
 * eeprom.  Basically; after the ID_sequence is sent to all of
 * the cards; they enter the ID_CMD state where they will accept
 * command requests. 0x80-0xbf loads the eeprom data.  We then
 * read the port 16 times and with every read; the cards check
 * for contention (ie: if one card writes a 0 bit and another
 * writes a 1 bit then the host sees a 0. At the end of the cycle;
 * each card compares the data on the bus; if there is a difference
 * then that card goes into ID_WAIT state again). In the meantime;
 * one bit of data is returned in the AX register which is conveniently
 * returned to us by inb().  Hence; we read 16 times getting one
 * bit of data with each read.
 */
static u_short
epreadeeprom(id_port, offset)
	int     id_port;
	int     offset;
{
	int     i, data = 0;

	outb(id_port, 0x80 + offset);
	delay(1000);
	for (i = 0; i < 16; i++)
		data = (data << 1) | (inw(id_port) & 1);
	return (data);
}

static int
epbusyeeprom(sc)
	struct ep_softc *sc;
{
	int     i = 100, j;

	while (i--) {
		j = inw(BASE + EP_W0_EEPROM_COMMAND);
		if (j & EEPROM_BUSY)
			delay(100);
		else
			break;
	}
	if (!i) {
		printf("\n%s: eeprom failed to come ready.\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	if (j & EEPROM_TST_MODE) {
		printf("\n%s: 3c509 in test mode. Erase pencil mark!\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}
	return (0);
}

static void
epmbuffill(sc)
	struct ep_softc *sc;
{
	int s, i;

	s = splimp();
	i = sc->last_mb;
	do {
		if (sc->mb[i] == NULL)
			MGET(sc->mb[i], M_DONTWAIT, MT_DATA);
		if (sc->mb[i] == NULL)
			break;
		i = (i + 1) % MAX_MBS;
	} while (i != sc->next_mb);
	sc->last_mb = i;
	splx(s);
}

static void
epmbufempty(sc)
	struct ep_softc *sc;
{
	int s, i;

	s = splimp();
	for (i = 0; i<MAX_MBS; i++) {
		if (sc->mb[i]) {
			m_freem(sc->mb[i]);
			sc->mb[i] = NULL;
		}
	}
	sc->last_mb = sc->next_mb = 0;
	untimeout(epmbuffill, sc);
	splx(s);
}
