/*	$NetBSD: esp.c,v 1.18 1999/02/03 20:44:43 dbj Exp $	*/

/*-
 * Copyright (c) 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Copyright (c) 1994 Peter Galbavy
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
 *	This product includes software developed by Peter Galbavy
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Based on aic6360 by Jarle Greipsland
 *
 * Acknowledgements: Many of the algorithms used in this driver are
 * inspired by the work of Julian Elischer (julian@tfs.com) and
 * Charles Hannum (mycroft@duality.gnu.ai.mit.edu).  Thanks a million!
 */

/*
 * Grabbed from the sparc port at revision 1.73 for the NeXT.
 * Darrin B. Jewell <dbj@netbsd.org>  Sat Jul  4 15:41:32 1998
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_message.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <next68k/next68k/isr.h>

#include <next68k/dev/nextdmareg.h>
#include <next68k/dev/nextdmavar.h>

#include "espreg.h"
#include "espvar.h"

#if 1
#define ESP_DEBUG
#endif

#ifdef ESP_DEBUG
int esp_debug = 0;
#define DPRINTF(x) if (esp_debug) printf x;
#else
#define DPRINTF(x)
#endif


void	espattach_intio	__P((struct device *, struct device *, void *));
int	espmatch_intio	__P((struct device *, struct cfdata *, void *));

/* DMA callbacks */
bus_dmamap_t esp_dmacb_continue __P((void *arg));
void esp_dmacb_completed __P((bus_dmamap_t map, void *arg));
void esp_dmacb_shutdown __P((void *arg));

/* Linkup to the rest of the kernel */
struct cfattach esp_ca = {
	sizeof(struct esp_softc), espmatch_intio, espattach_intio
};

struct scsipi_device esp_dev = {
	NULL,			/* Use default error handler */
	NULL,			/* have a queue, served by this */
	NULL,			/* have no async handler */
	NULL,			/* Use default 'done' routine */
};

/*
 * Functions and the switch for the MI code.
 */
u_char	esp_read_reg __P((struct ncr53c9x_softc *, int));
void	esp_write_reg __P((struct ncr53c9x_softc *, int, u_char));
int	esp_dma_isintr __P((struct ncr53c9x_softc *));
void	esp_dma_reset __P((struct ncr53c9x_softc *));
int	esp_dma_intr __P((struct ncr53c9x_softc *));
int	esp_dma_setup __P((struct ncr53c9x_softc *, caddr_t *,
	    size_t *, int, size_t *));
void	esp_dma_go __P((struct ncr53c9x_softc *));
void	esp_dma_stop __P((struct ncr53c9x_softc *));
int	esp_dma_isactive __P((struct ncr53c9x_softc *));

struct ncr53c9x_glue esp_glue = {
	esp_read_reg,
	esp_write_reg,
	esp_dma_isintr,
	esp_dma_reset,
	esp_dma_intr,
	esp_dma_setup,
	esp_dma_go,
	esp_dma_stop,
	esp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

#ifdef ESP_DEBUG
#define XCHR(x) "0123456789abcdef"[(x) & 0xf]
static void
esp_hex_dump(unsigned char *pkt, size_t len)
{
	size_t i, j;

	printf("0000: ");
	for(i=0; i<len; i++) {
		printf("%c%c ", XCHR(pkt[i]>>4), XCHR(pkt[i]));
		if ((i+1) % 16 == 0) {
			printf("  %c", '"');
			for(j=0; j<16; j++)
				printf("%c", pkt[i-15+j]>=32 && pkt[i-15+j]<127?pkt[i-15+j]:'.');
			printf("%c\n%c%c%c%c: ", '"', XCHR((i+1)>>12),
				XCHR((i+1)>>8), XCHR((i+1)>>4), XCHR(i+1));
		}
	}
	printf("\n");
}
#endif

int
espmatch_intio(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
  /* should probably probe here */
  /* Should also probably set up data from config */

	return(1);
}

void
espattach_intio(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct esp_softc *esc = (void *)self;
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;

	esc->sc_bst = NEXT68K_INTIO_BUS_SPACE;
	if (bus_space_map(esc->sc_bst, NEXT_P_SCSI, 
			ESP_DEVICE_SIZE, 0, &esc->sc_bsh)) {
    panic("\n%s: can't map ncr53c90 registers",
				sc->sc_dev.dv_xname);
	}

	sc->sc_id = 7;
	sc->sc_freq = 20;							/* Mhz */

	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &esp_glue;

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * It is necessary to try to load the 2nd config register here,
	 * to find out what rev the esp chip is, else the ncr53c9x_reset
	 * will not set up the defaults correctly.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2 | NCRCFG2_RPE;
	sc->sc_cfg3 = NCRCFG3_CDB;
	NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);

	if ((NCR_READ_REG(sc, NCR_CFG2) & ~NCRCFG2_RSVD) !=
	    (NCRCFG2_SCSI2 | NCRCFG2_RPE)) {
		sc->sc_rev = NCR_VARIANT_ESP100;
	} else {
		sc->sc_cfg2 = NCRCFG2_SCSI2;
		NCR_WRITE_REG(sc, NCR_CFG2, sc->sc_cfg2);
		sc->sc_cfg3 = 0;
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		sc->sc_cfg3 = (NCRCFG3_CDB | NCRCFG3_FCLK);
		NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
		if (NCR_READ_REG(sc, NCR_CFG3) !=
		    (NCRCFG3_CDB | NCRCFG3_FCLK)) {
			sc->sc_rev = NCR_VARIANT_ESP100A;
		} else {
			/* NCRCFG2_FE enables > 64K transfers */
			sc->sc_cfg2 |= NCRCFG2_FE;
			sc->sc_cfg3 = 0;
			NCR_WRITE_REG(sc, NCR_CFG3, sc->sc_cfg3);
			sc->sc_rev = NCR_VARIANT_ESP200;
		}
	}

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = 1000 / sc->sc_freq;

	/*
	 * Alas, we must now modify the value a bit, because it's
	 * only valid when can switch on FASTCLK and FASTSCSI bits  
	 * in config register 3... 
	 */
	switch (sc->sc_rev) {
	case NCR_VARIANT_ESP100:
		sc->sc_maxxfer = 64 * 1024;
		sc->sc_minsync = 0;	/* No synch on old chip? */
		break;

	case NCR_VARIANT_ESP100A:
		sc->sc_maxxfer = 64 * 1024;
		/* Min clocks/byte is 5 */
		sc->sc_minsync = ncr53c9x_cpb2stp(sc, 5);
		break;

	case NCR_VARIANT_ESP200:
		sc->sc_maxxfer = 16 * 1024 * 1024;
		/* XXX - do actually set FAST* bits */
		break;
	}

	/* @@@ Some ESP_DCTL bits probably need setting */
	NCR_WRITE_REG(sc, ESP_DCTL, 
			ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_RESET);
	DELAY(10);
	NCR_WRITE_REG(sc, ESP_DCTL, ESPDCTL_20MHZ | ESPDCTL_INTENB);
	DELAY(10);

	/* Set up SCSI DMA */
	{
		esc->sc_scsi_dma.nd_bst = NEXT68K_INTIO_BUS_SPACE;

		if (bus_space_map(esc->sc_scsi_dma.nd_bst, NEXT_P_SCSI_CSR,
				sizeof(struct dma_dev),0, &esc->sc_scsi_dma.nd_bsh)) {
			panic("\n%s: can't map scsi DMA registers",
					sc->sc_dev.dv_xname);
		}

		esc->sc_scsi_dma.nd_intr = NEXT_I_SCSI_DMA;
		esc->sc_scsi_dma.nd_shutdown_cb  = &esp_dmacb_shutdown;
		esc->sc_scsi_dma.nd_continue_cb  = &esp_dmacb_continue;
		esc->sc_scsi_dma.nd_completed_cb = &esp_dmacb_completed;
		esc->sc_scsi_dma.nd_cb_arg       = sc;
		nextdma_config(&esc->sc_scsi_dma);
		nextdma_init(&esc->sc_scsi_dma);

#if 0
		/* Turn on target selection using the `dma' method */
		ncr53c9x_dmaselect = 1;
#else
		ncr53c9x_dmaselect = 0;
#endif

		esc->sc_datain = -1;
		esc->sc_dmaaddr = 0;
		esc->sc_dmalen  = 0;
		esc->sc_dmasize = -1;

		esc->sc_loaded = 0;

		esc->sc_begin = 0;
		esc->sc_begin_size = 0;

		{
			int error;
			if ((error = bus_dmamap_create(esc->sc_scsi_dma.nd_dmat,
					sc->sc_maxxfer, 1, sc->sc_maxxfer,
					0, BUS_DMA_ALLOCNOW, &esc->sc_main_dmamap)) != 0) {
				panic("%s: can't create main i/o DMA map, error = %d",
						sc->sc_dev.dv_xname,error);
			}
		}
		esc->sc_main = 0;
		esc->sc_main_size = 0;

		{
			int error;
			if ((error = bus_dmamap_create(esc->sc_scsi_dma.nd_dmat,
					ESP_DMA_MAXTAIL, 1, ESP_DMA_MAXTAIL,
					0, BUS_DMA_ALLOCNOW, &esc->sc_tail_dmamap)) != 0) {
				panic("%s: can't create tail i/o DMA map, error = %d",
						sc->sc_dev.dv_xname,error);
			}
		}
		esc->sc_tail = 0;
		esc->sc_tail_size = 0;

	}

	/* Establish interrupt channel */
	isrlink_autovec((int(*)__P((void*)))ncr53c9x_intr, sc,
			NEXT_I_IPL(NEXT_I_SCSI), 0);
	INTR_ENABLE(NEXT_I_SCSI);

	/* register interrupt stats */
	evcnt_attach(&sc->sc_dev, "intr", &sc->sc_intrcnt);

	/* Do the common parts of attachment. */
	sc->sc_adapter.scsipi_cmd = ncr53c9x_scsi_cmd;
	sc->sc_adapter.scsipi_minphys = minphys; 
	ncr53c9x_attach(sc, &esp_dev);
}

/*
 * Glue functions.
 */

u_char
esp_read_reg(sc, reg)
	struct ncr53c9x_softc *sc;
	int reg;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	return(bus_space_read_1(esc->sc_bst, esc->sc_bsh, reg));
}

void
esp_write_reg(sc, reg, val)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char val;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	bus_space_write_1(esc->sc_bst, esc->sc_bsh, reg, val);
}

int
esp_dma_isintr(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	int r = (INTR_OCCURRED(NEXT_I_SCSI));

	if (r) {
		DPRINTF(("esp_dma_isintr = 0x%b\n",
				(*(volatile u_long *)IIOV(NEXT_P_INTRSTAT)),NEXT_INTR_BITS));

		while (esp_dma_isactive(sc)) {

#ifdef DIAGNOSTIC
			r = (INTR_OCCURRED(NEXT_I_SCSI));
			if (!r) panic("esp dma enabled but failed to flush");
#endif

			if (esc->sc_datain) {
				NCR_WRITE_REG(sc, ESP_DCTL, 
						ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_DMAMOD | ESPDCTL_DMARD | ESPDCTL_FLUSH);
				NCR_WRITE_REG(sc, ESP_DCTL, 
						ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_DMAMOD | ESPDCTL_DMARD);
			} else {
				NCR_WRITE_REG(sc, ESP_DCTL, 
						ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_DMAMOD | ESPDCTL_FLUSH);
				NCR_WRITE_REG(sc, ESP_DCTL, 
						ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_DMAMOD);
			}
			{
				int nr;
				nr = nextdma_intr(&esc->sc_scsi_dma);
				if (nr) {
					DPRINTF(("nextma_intr = %d\n",nr));
				}
			}
		}

		/* Clear the DMAMOD bit in the DCTL register, since if this
		 * routine returns true, then the ncr53c9x_intr handler will
		 * be called and needs access to the scsi registers.
		 */
		if (esc->sc_datain) {
			NCR_WRITE_REG(sc, ESP_DCTL,
					ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_DMARD);
		} else {
			NCR_WRITE_REG(sc, ESP_DCTL,
					ESPDCTL_20MHZ | ESPDCTL_INTENB);
		}

	}

	return (r);
}

void
esp_dma_reset(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("esp dma reset\n"));

#ifdef ESP_DEBUG
	if (esp_debug) {
		printf("  *intrstat = 0x%b\n",
				(*(volatile u_long *)IIOV(NEXT_P_INTRSTAT)),NEXT_INTR_BITS);
		printf("  *intrmask = 0x%b\n",
				(*(volatile u_long *)IIOV(NEXT_P_INTRMASK)),NEXT_INTR_BITS);
	}
#endif

	/* Clear the DMAMOD bit in the DCTL register: */
	NCR_WRITE_REG(sc, ESP_DCTL,
			ESPDCTL_20MHZ | ESPDCTL_INTENB);

	nextdma_reset(&esc->sc_scsi_dma);

	esc->sc_datain = -1;
	esc->sc_dmaaddr = 0;
	esc->sc_dmalen  = 0;
	esc->sc_dmasize = -1;

	esc->sc_loaded = 0;

	esc->sc_begin = 0;
	esc->sc_begin_size = 0;

	if (esc->sc_main_dmamap->dm_mapsize) {
		bus_dmamap_unload(esc->sc_scsi_dma.nd_dmat, esc->sc_main_dmamap);
	}
	esc->sc_main = 0;
	esc->sc_main_size = 0;

	if (esc->sc_tail_dmamap->dm_mapsize) {
		bus_dmamap_unload(esc->sc_scsi_dma.nd_dmat, esc->sc_tail_dmamap);
	}
	esc->sc_tail = 0;
	esc->sc_tail_size = 0;
}

int
esp_dma_intr(sc)
	struct ncr53c9x_softc *sc;
{
#ifdef DIAGNOSTIC
	panic("%s: esp_dma_intr shouldn't be invoked.\n", sc->sc_dev.dv_xname);
#endif

	return -1;
}

int
esp_dma_setup(sc, addr, len, datain, dmasize)
	struct ncr53c9x_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

#ifdef DIAGNOSTIC
	/* if this is a read DMA, pre-fill the buffer with 0xdeadbeef
	 * to identify bogus reads
	 */
	if (datain) {
		int *v = (int *)(*addr);
		int i;
		for(i=0;i<((*len)/4);i++) v[i] = 0xdeadbeef;
		v = (int *)(&(esc->sc_tailbuf[0]));
		for(i=0;i<((sizeof(esc->sc_tailbuf)/4));i++) v[i] = 0xdeaffeed;
	}

#endif

	DPRINTF(("esp_dma_setup(0x%08lx,0x%08lx,0x%08lx)\n",*addr,*len,*dmasize));

#ifdef DIAGNOSTIC /* @@@ this is ok sometimes. verify that we handle it ok
									 * and then remove this check
									 */
	if (*len != *dmasize) {
		panic("esp dmalen != size");
	}
#endif

#ifdef DIAGNOSTIC
	if ((esc->sc_datain != -1) ||
			(esc->sc_main_dmamap->dm_mapsize != 0) ||
			(esc->sc_tail_dmamap->dm_mapsize != 0)) {
		panic("%s: map already loaded in esp_dma_setup\n"
				"\tdatain = %d\n\tmain_mapsize=%d\n\tail_mapsize=%d",
				sc->sc_dev.dv_xname, esc->sc_datain,
				esc->sc_main_dmamap->dm_mapsize,esc->sc_tail_dmamap->dm_mapsize);
	}
#endif

	/* Save these in case we have to abort DMA */
	esc->sc_datain   = datain;
	esc->sc_dmaaddr  = addr;
	esc->sc_dmalen   = len;
	esc->sc_dmasize  = *dmasize;

	esc->sc_loaded = 0;

	{
		size_t slop_bgn_size; /* # bytes to be fifo'd at beginning */
		size_t slop_end_size; /* # bytes to be transferred in tail buffer */
		
		{
			u_long bgn = (u_long)(*esc->sc_dmaaddr);
			u_long end = (u_long)(*esc->sc_dmaaddr+esc->sc_dmasize);

			slop_bgn_size = DMA_BEGINALIGNMENT-(bgn % DMA_BEGINALIGNMENT);
			if (slop_bgn_size == DMA_BEGINALIGNMENT) slop_bgn_size = 0;
			slop_end_size = end % DMA_ENDALIGNMENT;
		}

		/* Check to make sure we haven't counted extra slop
		 * as would happen for a very short dma buffer, also
		 * for short buffers, just stuff the entire thing in the tail
		 */
		if ((slop_bgn_size+slop_end_size >= esc->sc_dmasize)
#if 1
				|| (esc->sc_dmasize <= ESP_DMA_MAXTAIL)
#endif
				)
		{
 			slop_bgn_size = 0;
			slop_end_size = esc->sc_dmasize;
		}

		/* initialize the fifo buffer */
		if (slop_bgn_size) {
			esc->sc_begin = *esc->sc_dmaaddr;
			esc->sc_begin_size = slop_bgn_size;
		} else {
			esc->sc_begin = 0;
			esc->sc_begin_size = 0;
		}

		/* Load the normal DMA map */
		{
			esc->sc_main      = *esc->sc_dmaaddr+slop_bgn_size;
			esc->sc_main_size = (esc->sc_dmasize)-(slop_end_size+slop_bgn_size);

			if (esc->sc_main_size) {
				int error;
				error = bus_dmamap_load(esc->sc_scsi_dma.nd_dmat,
						esc->sc_main_dmamap,
						esc->sc_main, esc->sc_main_size,
						NULL, BUS_DMA_NOWAIT);
				if (error) {
					panic("%s: can't load main dma map. error = %d, addr=0x%08x, size=0x%08x",
							sc->sc_dev.dv_xname, error,esc->sc_main,esc->sc_main_size);
				}
			} else {
				esc->sc_main = 0;
			}
		}

		/* Load the tail DMA map */
		if (slop_end_size) {
			esc->sc_tail      = DMA_ENDALIGN(caddr_t,esc->sc_tailbuf+slop_end_size)-slop_end_size;
			/* If the beginning of the tail is not correctly aligned,
			 * we have no choice but to align the start, which might then unalign the end.
			 */
			esc->sc_tail      = DMA_ALIGN(caddr_t,esc->sc_tail);
			/* So therefore, we change the tail size to be end aligned again. */
			esc->sc_tail_size = DMA_ENDALIGN(caddr_t,esc->sc_tail+slop_end_size)-esc->sc_tail;
		
			{
				int error;
				error = bus_dmamap_load(esc->sc_scsi_dma.nd_dmat,
						esc->sc_tail_dmamap,
						esc->sc_tail, esc->sc_tail_size,
						NULL, BUS_DMA_NOWAIT);
				if (error) {
					panic("%s: can't load tail dma map. error = %d, addr=0x%08x, size=0x%08x",
							sc->sc_dev.dv_xname, error,esc->sc_tail,esc->sc_tail_size);
				}
			}
		}
	}

	return (0);
}

void
esp_dma_go(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("%s: esp_dma_go(datain = %d)\n",
			sc->sc_dev.dv_xname, esc->sc_datain));

	DPRINTF(("%s: dma_shutdown: addr=0x%08lx,len=0x%08lx,size=0x%08lx\n",
			sc->sc_dev.dv_xname,
				*esc->sc_dmaaddr, *esc->sc_dmalen, esc->sc_dmasize));

	DPRINTF(("%s: begin = 0x%08x, size = 0x%08x\n",
			sc->sc_dev.dv_xname, esc->sc_begin, esc->sc_begin_size));
	DPRINTF(("%s: main = 0x%08x, size = 0x%08x\n",
			sc->sc_dev.dv_xname, esc->sc_main, esc->sc_main_size));
	DPRINTF(("%s: tail = 0x%08x, size = 0x%08x\n",
			sc->sc_dev.dv_xname, esc->sc_tail, esc->sc_tail_size));

#ifdef DIAGNOSTIC
	{
		int n = NCR_READ_REG(sc, NCR_FFLAG);
		DPRINTF(("esp fifo size = %d, seq = 0x%x\n",n & NCRFIFO_FF, (n & NCRFIFO_SS)>>5));
	}
#endif

#if defined(DIAGNOSTIC)
  if ((esc->sc_begin_size == 0) &&
			(esc->sc_main_dmamap->dm_mapsize == 0) &&
			(esc->sc_tail_dmamap->dm_mapsize == 0)) {
		panic("%s: No DMA requested!",sc->sc_dev.dv_xname);
	}
#endif

	/* Stuff the fifo with the begin buffer */
	if (esc->sc_datain) {
		int i;
		for(i=0;i<esc->sc_begin_size;i++) {
			esc->sc_begin[i]=NCR_READ_REG(sc, NCR_FIFO);
		}
	} else {
		int i;
		for(i=0;i<esc->sc_begin_size;i++) {
			NCR_WRITE_REG(sc, NCR_FIFO, esc->sc_begin[i]);
		}
	}

	/* if we are a dma write cycle, copy the end slop */
	if (esc->sc_datain == 0) {
		memcpy(esc->sc_tail,
				(*esc->sc_dmaaddr+esc->sc_begin_size+esc->sc_main_size),
				(esc->sc_dmasize-(esc->sc_begin_size+esc->sc_main_size)));
	}

	nextdma_start(&esc->sc_scsi_dma, 
			(esc->sc_datain ? DMACSR_READ : DMACSR_WRITE));

	if (esc->sc_datain) { 
		NCR_WRITE_REG(sc, ESP_DCTL,
				ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_DMAMOD | ESPDCTL_DMARD);
	} else {
		NCR_WRITE_REG(sc, ESP_DCTL,
				ESPDCTL_20MHZ | ESPDCTL_INTENB | ESPDCTL_DMAMOD);
	}

}

void
esp_dma_stop(sc)
	struct ncr53c9x_softc *sc;
{
	panic("Not yet implemented");
}

int
esp_dma_isactive(sc)
	struct ncr53c9x_softc *sc;
{
	struct esp_softc *esc = (struct esp_softc *)sc;
	int r = !nextdma_finished(&esc->sc_scsi_dma);
	DPRINTF(("esp_dma_isactive = %d\n",r));
	return(r);
}

/****************************************************************/

/* Internal dma callback routines */
bus_dmamap_t
esp_dmacb_continue(arg)
	void *arg;
{
	struct ncr53c9x_softc *sc = (struct ncr53c9x_softc *)arg;
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("%s: dma continue\n",sc->sc_dev.dv_xname));

#ifdef DIAGNOSTIC
	if ((esc->sc_datain < 0) || (esc->sc_datain > 1)) {
		panic("%s: map not loaded in dma continue callback, datain = %d",
				sc->sc_dev.dv_xname,esc->sc_datain);
	}
#endif

	if ((!(esc->sc_loaded & ESP_LOADED_MAIN)) && 
			(esc->sc_main_dmamap->dm_mapsize)) {
			DPRINTF(("%s: Loading main map\n",sc->sc_dev.dv_xname));
			bus_dmamap_sync(esc->sc_scsi_dma.nd_dmat, esc->sc_main_dmamap,
					0, esc->sc_main_dmamap->dm_mapsize, 
					(esc->sc_datain ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE));
			esc->sc_loaded |= ESP_LOADED_MAIN;
			return(esc->sc_main_dmamap);
	}

	if ((!(esc->sc_loaded & ESP_LOADED_TAIL)) && 
			(esc->sc_tail_dmamap->dm_mapsize)) {
			DPRINTF(("%s: Loading tail map\n",sc->sc_dev.dv_xname));
			bus_dmamap_sync(esc->sc_scsi_dma.nd_dmat, esc->sc_tail_dmamap,
					0, esc->sc_tail_dmamap->dm_mapsize, 
					(esc->sc_datain ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE));
			esc->sc_loaded |= ESP_LOADED_TAIL;
			return(esc->sc_tail_dmamap);
	}

	DPRINTF(("%s: not loading map\n",sc->sc_dev.dv_xname));
	return(0);
}


void
esp_dmacb_completed(map, arg)
	bus_dmamap_t map;
	void *arg;
{
	struct ncr53c9x_softc *sc = (struct ncr53c9x_softc *)arg;
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("esp dma completed\n"));

#ifdef DIAGNOSTIC
	if ((esc->sc_datain < 0) || (esc->sc_datain > 1)) {
		panic("%s: invalid dma direction in completed callback, datain = %d",
				sc->sc_dev.dv_xname,esc->sc_datain);
	}
	if ((map != esc->sc_main_dmamap) && (map != esc->sc_tail_dmamap)) {
		panic("%s: unexpected completed map", sc->sc_dev.dv_xname);
	}
#endif

	bus_dmamap_sync(esc->sc_scsi_dma.nd_dmat, map,
			0, map->dm_mapsize,
			(esc->sc_datain ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE));

}

void
esp_dmacb_shutdown(arg)
	void *arg;
{
	struct ncr53c9x_softc *sc = (struct ncr53c9x_softc *)arg;
	struct esp_softc *esc = (struct esp_softc *)sc;

	DPRINTF(("esp dma shutdown\n"));

	/* Stuff the end slop into fifo */

#ifdef ESP_DEBUG
	if (esp_debug) {
		
		int n = NCR_READ_REG(sc, NCR_FFLAG);
		DPRINTF(("esp fifo size = %d, seq = 0x%x\n",n & NCRFIFO_FF, (n & NCRFIFO_SS)>>5));

		NCR_DMA(("dmaintr: tcl=%d, tcm=%d, tch=%d\n",
				NCR_READ_REG(sc, NCR_TCL),
				NCR_READ_REG(sc, NCR_TCM),
				(sc->sc_cfg2 & NCRCFG2_FE)
				? NCR_READ_REG(sc, NCR_TCH) : 0));
	}
#endif

	/* First copy the tail dma buffer data for read transfers */
	if (esc->sc_datain == 1) {
		memcpy((*esc->sc_dmaaddr+esc->sc_begin_size+esc->sc_main_size),
				esc->sc_tail,
				(esc->sc_dmasize-(esc->sc_begin_size+esc->sc_main_size)));
	}

#ifdef ESP_DEBUG
	if (esp_debug) {
		printf("%s: dma_shutdown: addr=0x%08lx,len=0x%08lx,size=0x%08lx\n",
				sc->sc_dev.dv_xname,
				*esc->sc_dmaaddr, *esc->sc_dmalen, esc->sc_dmasize);
		esp_hex_dump(*(esc->sc_dmaaddr),esc->sc_dmasize);
		printf("%s: tail=0x%08lx,tailbuf=0x%08lx,tail_size=0x%08lx\n",
				sc->sc_dev.dv_xname,
				esc->sc_tail, &(esc->sc_tailbuf[0]), esc->sc_tail_size);
		esp_hex_dump(&(esc->sc_tailbuf[0]),sizeof(esc->sc_tailbuf));
	}
#endif

	esc->sc_datain = -1;
	esc->sc_dmaaddr = 0;
	esc->sc_dmalen  = 0;
	esc->sc_dmasize = -1;

	esc->sc_loaded = 0;

	esc->sc_begin = 0;
	esc->sc_begin_size = 0;

	if (esc->sc_main_dmamap->dm_mapsize) {
		bus_dmamap_unload(esc->sc_scsi_dma.nd_dmat, esc->sc_main_dmamap);
	}
	esc->sc_main = 0;
	esc->sc_main_size = 0;

	if (esc->sc_tail_dmamap->dm_mapsize) {
		bus_dmamap_unload(esc->sc_scsi_dma.nd_dmat, esc->sc_tail_dmamap);
	}
	esc->sc_tail = 0;
	esc->sc_tail_size = 0;

}
