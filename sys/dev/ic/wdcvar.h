/*	$NetBSD: wdcvar.h,v 1.5 1998/09/22 00:21:16 mark Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum, by Onno van der Linden and by Manuel Bouyer.
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

struct wdc_attachment_data {
	/* manadatory fields */
	int			cap;
	int			flags;
	bus_space_tag_t		iot;
	bus_space_handle_t	ioh;
	bus_space_tag_t		auxiot;
	bus_space_handle_t	auxioh;

	/* data32{iot,ioh} are only used for 32 bit xfers */
	bus_space_tag_t		data32iot;
	bus_space_handle_t	data32ioh;

	/* if WDC_CAPABILITY_DMA set in 'cap' */
	void			*dma_arg;
	void			(*dma_setup) __P((void *));
	void			(*dma_start) __P((void *, void *, size_t,
				    int));
	void			(*dma_finish) __P((void *));

	/* if WDC_CAPABILITY_HWLOCK set in 'cap' */
	int			(*claim_hw) __P((void *, int));
	void			(*free_hw) __P((void *));
};

/* Capabilities supported by the controller */
#define	WDC_CAPABILITY_DATA32	0x01		/* 32-bit data access */
#define	WDC_CAPABILITY_DMA	0x02		/* DMA */
#define	WDC_CAPABILITY_HWLOCK	0x04		/* Needs to lock HW */

/* Flags passed from the attach routine to the wdc driver */
#define WDC_NO_EXTRA_RESETS	0x01		/* Only reset once at attach */

struct wdc_softc {
	struct device sc_dev;
	const struct wdc_attachment_data *sc_adp;

	struct wd_link *d_link[2];
	struct scsipi_link *ab_link;

	TAILQ_HEAD(xferhead, wdc_xfer) sc_xfer;
	int sc_flags;
#define	WDCF_ACTIVE		0x01	/* controller is active */
#define	WDCF_SINGLE		0x02	/* sector at a time mode */
#define	WDCF_ERROR		0x04	/* processing a disk error */
#define	WDCF_WANTED		0x08	/* XXX locking for wd_get_parms() */
#define	WDCF_IRQ_WAIT		0x10	/* controller is waiting for irq */
#define	WDCF_ONESLAVE		0x20	/* ctrl. has one ATAPI slave attached */
	int sc_errors;			/* errors during current transfer */
	u_char sc_status;		/* copy of status register */
	u_char sc_error;		/* copy of error register */
	u_char sc_drives_mask;	/* bitmask for drives present/absent */
};

#define	sc_cap		sc_adp->cap
#define	sc_iot		sc_adp->iot
#define	sc_ioh		sc_adp->ioh
#define	sc_auxiot	sc_adp->auxiot
#define	sc_auxioh	sc_adp->auxioh
#define	sc_data32iot	sc_adp->data32iot
#define	sc_data32ioh	sc_adp->data32ioh
#define	sc_dma_arg	sc_adp->dma_arg
#define	sc_dma_setup	sc_adp->dma_setup
#define	sc_dma_start	sc_adp->dma_start
#define	sc_dma_finish	sc_adp->dma_finish
#define	sc_claim_hw	sc_adp->claim_hw
#define	sc_free_hw	sc_adp->free_hw

void	wdcattach(struct wdc_softc *, const struct wdc_attachment_data *);
int	wdcintr(void *);
int	wdcprobe(const struct wdc_attachment_data *);
