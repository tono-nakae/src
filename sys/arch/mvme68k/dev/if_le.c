/*	$NetBSD: if_le.c,v 1.5 1996/04/18 00:25:18 cgd Exp $ */

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
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
 *	@(#)if_le.c	8.2 (Berkeley) 11/16/93
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>

#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/pmap.h>

#include <mvme68k/dev/iio.h>
#include <mvme68k/dev/pccreg.h>
#include <mvme68k/dev/if_lereg.h>
#include <mvme68k/dev/if_levar.h>
#include <dev/ic/am7990reg.h>
#define	LE_NEED_BUF_CONTIG
#include <dev/ic/am7990var.h>

#define	LE_SOFTC(unit)	le_cd.cd_devs[unit]
#define	LE_DELAY(x)	DELAY(x)

int	lematch __P((struct device *, void *, void *));
void	leattach __P((struct device *, struct device *, void *));
int	leintr __P((void *));

struct cfattach le_ca = {
	sizeof(struct le_softc), lematch, leattach
};

struct	cfdriver le_cd = {
	NULL, "le", DV_IFNET
};

void *ledatabuf; /* XXXCDC hack from pmap bootstrap */

integrate void
lewrcsr(sc, port, val)
	struct le_softc *sc;
	u_int16_t port, val;
{
	register struct lereg1 *ler1 = sc->sc_r1;

	ler1->ler1_rap = port;
	ler1->ler1_rdp = val;
}

integrate u_int16_t
lerdcsr(sc, port)
	struct le_softc *sc;
	u_int16_t port;
{
	register struct lereg1 *ler1 = sc->sc_r1;
	u_int16_t val;

	ler1->ler1_rap = port;
	val = ler1->ler1_rdp;
	return (val);
}

int
lematch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;

	return !badbaddr((caddr_t) IIO_CFLOC_ADDR(cf));
}

void
leattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct le_softc *sc = (void *)self;
	struct cfdata *cf = self->dv_cfdata;
	int pri = IIO_CFLOC_LEVEL(cf);

	/* XXX the following declarations should be elsewhere */
	extern void myetheraddr __P((u_char *));

	iio_print(cf);

	sc->sc_r1 = (struct lereg1 *)IIO_CFLOC_ADDR(cf);
	sc->sc_mem = ledatabuf;		/* XXX */
	sc->sc_conf3 = LE_C3_BSWP;
	sc->sc_addr = (u_long)sc->sc_mem;
	sc->sc_memsize = 4 * NBPG;	/* XXX */

	myetheraddr(sc->sc_arpcom.ac_enaddr);

	sc->sc_copytodesc = am7990_copytobuf_contig;
	sc->sc_copyfromdesc = am7990_copyfrombuf_contig;
	sc->sc_copytobuf = am7990_copytobuf_contig;
	sc->sc_copyfrombuf = am7990_copyfrombuf_contig;
	sc->sc_zerobuf = am7990_zerobuf_contig;

	sc->sc_arpcom.ac_if.if_name = le_cd.cd_name;
	leconfig(sc);

	pccintr_establish(PCCV_LE, leintr, pri, sc);
	sys_pcc->le_int = pri | PCC_IENABLE;
}

#include <dev/ic/am7990.c>
