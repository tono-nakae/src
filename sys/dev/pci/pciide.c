/*	$NetBSD: pciide.c,v 1.7 1998/06/08 06:55:57 thorpej Exp $	*/

/*
 * Copyright (c) 1996, 1998 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

/*
 * PCI IDE controller driver.
 *
 * Author: Christopher G. Demetriou, March 2, 1998 (derived from NetBSD
 * sys/dev/pci/ppb.c, revision 1.16).
 *
 * See "PCI IDE Controller Specification, Revision 1.0 3/4/94" and
 * "Programming Interface for Bus Master IDE Controller, Revision 1.0
 * 5/16/94" from the PCI SIG.
 *
 * XXX Does not yet support DMA (but does map the Bus Master DMA regs).
 *
 * XXX Does not support serializing the two channels for broken (at least
 * XXX according to linux and freebsd) controllers, e.g. CMD PCI0640.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/ic/wdcreg.h>

struct pciide_softc {
	struct device		sc_dev;

	void			*sc_pci_ih;	/* PCI interrupt handle */
	int			sc_dma_ok;	/* bus-master DMA info */
	bus_space_tag_t		sc_dma_iot;
	bus_space_handle_t	sc_dma_ioh;

	struct pciide_channel {			/* per-channel data */
		/* internal bookkeeping */
		int		hw_ok;		/* hardware mapped & OK? */
		struct device	*dev;		/* 'wdc' dev attached */
		int		compat;		/* is it compat? */
		void		*ih;		/* compat or pci handle */

		/* used by wdc attachment (read-only after init) */
		bus_space_tag_t	cmd_iot, ctl_iot;
		bus_space_handle_t cmd_ioh, ctl_ioh;

		/* filled in by wdc attachment (written by wdc attach) */
		int		(*ihand) __P((void *));
		void		*ihandarg;
	} sc_channels[PCIIDE_NUM_CHANNELS];
};

#define	PCIIDE_CHANNEL_NAME(chan)	((chan) == 0 ? "primary" : "secondary")

int	pciide_match __P((struct device *, struct cfdata *, void *));
void	pciide_attach __P((struct device *, struct device *, void *));

struct cfattach pciide_ca = {
	sizeof(struct pciide_softc), pciide_match, pciide_attach
};

int	pciide_map_channel_compat __P((struct pciide_softc *,
	    struct pci_attach_args *, int));
const char *pciide_compat_channel_probe __P((struct pciide_softc *,
	    struct pci_attach_args *, int));
int	pciide_probe_wdc __P((struct pciide_channel *));
int	pciide_map_channel_native __P((struct pciide_softc *,
	    struct pci_attach_args *, int));
int	pciide_print __P((void *, const char *pnp));
int	pciide_compat_intr __P((void *));
int	pciide_pci_intr __P((void *));

#define	PCIIDE_PROBE_WDC_DELAY	100		/* 100us each */
#define	PCIIDE_PROBE_WDC_NDELAY	10000		/* wait up to 1s */

int
pciide_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;

	/*
	 * Check the ID register to see that it's a PCI IDE controller.
	 * If it is, we assume that we can deal with it; it _should_
	 * work in a standardized way...
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_MASS_STORAGE &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_MASS_STORAGE_IDE) {
		return (1);
	}

	return (0);
}

void
pciide_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	struct pciide_softc *sc = (struct pciide_softc *)self;
	struct pciide_attach_args aa;
	struct pciide_channel *cp;
	pcireg_t class, interface, csr;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	char devinfo[256];
	int i;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

	if ((pa->pa_flags & PCI_FLAGS_IO_ENABLED) == 0) {
		csr = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
		printf("%s: device disabled (at %s)\n", sc->sc_dev.dv_xname,
		    (csr & PCI_COMMAND_IO_ENABLE) == 0 ? "device" : "bridge");
		return;
	}

	class = pci_conf_read(pc, pa->pa_tag, PCI_CLASS_REG);
	interface = PCI_INTERFACE(class);

	/*
	 * Set up PCI interrupt.
	 *
	 * If mapping fails, that's (probably) because there's no pin
	 * set to intr, which is (probably) because it's a compat-only
	 * device (or hard-wired in compatibility-only mode).  Native-PCI
	 * channels will complain later if the interrupt was needed.
	 *
	 * If establishment fails, that's (probably) some other problem.
	 */
	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &intrhandle) == 0) {
		intrstr = pci_intr_string(pa->pa_pc, intrhandle);
		sc->sc_pci_ih = pci_intr_establish(pa->pa_pc, intrhandle,
		    IPL_BIO, pciide_pci_intr, sc);

		if (sc->sc_pci_ih != NULL) {
			printf("%s: using %s for native-PCI interrupt\n",
			    sc->sc_dev.dv_xname,
			    intrstr ? intrstr : "unknown interrupt");
		} else {
			printf("%s: couldn't establish native-PCI interrupt",
			    sc->sc_dev.dv_xname);
			if (intrstr != NULL)
				printf(" at %s", intrstr); 
			printf("\n");
		}
	}

	/*
	 * Map DMA registers, if DMA is supported.
	 *
	 * Note that sc_dma_ok is the right variable to test to see if
	 * DMA can * be done.  If the interface doesn't support DMA,
	 * sc_dma_ok * will never be non-zero.  If the DMA regs couldn't
	 * be mapped, it'll be zero.  I.e., sc_dma_ok will only be
	 * non-zero if the interface supports DMA and the registers
	 * could be mapped.
	 *
	 * XXX Note that despite the fact that the Bus Master IDE specs
	 * XXX say that "The bus master IDE functoin uses 16 bytes of IO
	 * XXX space," some controllers (at least the United
	 * XXX Microelectronics UM8886BF) place it in memory space.
	 * XXX eventually, we should probably read the register and check
	 * XXX which type it is.  Either that or 'quirk' certain devices.
	 */
	if (interface & PCIIDE_INTERFACE_BUS_MASTER_DMA) {
		sc->sc_dma_ok = (pci_mapreg_map(pa,
		    PCIIDE_REG_BUS_MASTER_DMA, PCI_MAPREG_TYPE_IO, 0,
		    &sc->sc_dma_iot, &sc->sc_dma_ioh, NULL, NULL) == 0);
		printf("%s: bus-master DMA support present, but unused (%s)\n",
		    sc->sc_dev.dv_xname,
		    sc->sc_dma_ok ? "no driver support" :
		      "couldn't map registers");
	}

	for (i = 0; i < PCIIDE_NUM_CHANNELS; i++) {
		cp = &sc->sc_channels[i];

		printf("%s: %s channel %s to %s mode\n",
		    sc->sc_dev.dv_xname, PCIIDE_CHANNEL_NAME(i),
		    (interface & PCIIDE_INTERFACE_SETTABLE(i)) ?
		      "configured" : "wired",
		    (interface & PCIIDE_INTERFACE_PCI(i)) ? "native-PCI" :
		      "compatibility");

		if (interface & PCIIDE_INTERFACE_PCI(i))
			cp->hw_ok = pciide_map_channel_native(sc, pa, i);
		else
			cp->hw_ok = pciide_map_channel_compat(sc, pa, i);
		if (!cp->hw_ok)
			continue;

		aa.channel = i;
		aa.cmd_iot = cp->cmd_iot;
		aa.cmd_ioh = cp->cmd_ioh;
		aa.ctl_iot = cp->ctl_iot;
		aa.ctl_ioh = cp->ctl_ioh;
		aa.ihandp = &cp->ihand;
		aa.ihandargp = &cp->ihandarg;
		cp->dev = config_found(self, &aa, pciide_print);
			
		/*
		 * Note that if the 'wdc' device isn't configured,
		 * the controller's resources are still marked as
		 * being in use.  This is a feature.
		 */
	}
}

int
pciide_map_channel_compat(sc, pa, chan)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	int chan;
{
	struct pciide_channel *cp = &sc->sc_channels[chan];
	const char *probe_fail_reason;
	int rv = 1;

	cp->compat = 1;

	cp->cmd_iot = pa->pa_iot;
	if (bus_space_map(cp->cmd_iot, PCIIDE_COMPAT_CMD_BASE(chan),
	    PCIIDE_COMPAT_CMD_SIZE, 0, &cp->cmd_ioh) != 0) {
		printf("%s: couldn't map %s channel cmd regs\n",
		    sc->sc_dev.dv_xname, PCIIDE_CHANNEL_NAME(chan));
		rv = 0;
	}

	cp->ctl_iot = pa->pa_iot;
	if (bus_space_map(cp->ctl_iot, PCIIDE_COMPAT_CTL_BASE(chan),
	    PCIIDE_COMPAT_CTL_SIZE, 0, &cp->ctl_ioh) != 0) {
		printf("%s: couldn't map %s channel ctl regs\n",
		    sc->sc_dev.dv_xname, PCIIDE_CHANNEL_NAME(chan));
		rv = 0;
	}

	/*
	 * If we weren't able to map the device successfully,
	 * we just give up now.  Something else has already
	 * occupied those ports, indicating that the device has
	 * (probably) been completely disabled (by some nonstandard
	 * mechanism).
	 *
	 * XXX If we successfully map some ports, but not others,
	 * XXX it might make sense to unmap the ones that we mapped.
	 */
	if (rv == 0)
		goto out;

	/*
	 * If we were able to map the device successfully, try to
	 * make sure that there's a wdc there and that it's
	 * attributable to us.
	 *
	 * If there's not, then we assume that there's the device
	 * has been disabled and that other devices are free to use
	 * its ports.
	 */
	probe_fail_reason = pciide_compat_channel_probe(sc, pa, chan);
	if (probe_fail_reason != NULL) {
		printf("%s: %s channel ignored (%s)\n", sc->sc_dev.dv_xname,
		    PCIIDE_CHANNEL_NAME(chan), probe_fail_reason);
		rv = 0;

		bus_space_unmap(cp->cmd_iot, cp->cmd_ioh,
		    PCIIDE_COMPAT_CMD_SIZE);
		bus_space_unmap(cp->ctl_iot, cp->ctl_ioh,
		    PCIIDE_COMPAT_CTL_SIZE);

		goto out;
	}

	/*
	 * If we're here, we were able to map the device successfully
	 * and it really looks like there's a controller there.
	 *
	 * Unless those conditions are true, we don't map the
	 * compatibility interrupt.  The spec indicates that if a
	 * channel is configured for compatibility mode and the PCI
	 * device's I/O space is enabled, the channel will be enabled.
	 * Hoewver, some devices seem to be able to disable invididual
	 * compatibility channels (via non-standard mechanisms).  If
	 * the channel is disabled, the interrupt line can (probably)
	 * be used by other devices (and may be assigned to other
	 * devices by the BIOS).  If we mapped the interrupt we might
	 * conflict with another interrupt assignment.
	 */
	cp->ih = pciide_machdep_compat_intr_establish(&sc->sc_dev, pa,
	    chan, pciide_compat_intr, cp);
	if (cp->ih == NULL) {
		printf("%s: no compatibility interrupt for use by %s channel\n",
		    sc->sc_dev.dv_xname, PCIIDE_CHANNEL_NAME(chan));
		rv = 0;
	}

out:
	return (rv);
}

const char *
pciide_compat_channel_probe(sc, pa, chan)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
{
	pcireg_t csr;
	const char *failreason = NULL;

	/*
	 * Check to see if something appears to be there.
	 */
	if (!pciide_probe_wdc(&sc->sc_channels[chan])) {
		failreason = "not responding; disabled or no drives?";
		goto out;
	}

	/*
	 * Now, make sure it's actually attributable to this PCI IDE
	 * channel by trying to access the channel again while the
	 * PCI IDE controller's I/O space is disabled.  (If the
	 * channel no longer appears to be there, it belongs to
	 * this controller.)  YUCK!
	 */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr & ~PCI_COMMAND_IO_ENABLE);
	if (pciide_probe_wdc(&sc->sc_channels[chan]))
		failreason = "other hardware responding at addresses";
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, csr);

out:
	return (failreason);
}

int
pciide_probe_wdc(cp)
	struct pciide_channel *cp;
{
	u_int8_t st0, st1;
	int timeout;

	/*
	 * Sanity check to see if the wdc channel responds at all.
	 * (Modeled on wdc_init_controller() and wdc_reset() in wdc.c.)
	 *
	 * Reset the channel, and make sure that it responds sanely
	 * after it's been reset.
	 */

        /* Reset the channel. */
        bus_space_write_1(cp->ctl_iot, cp->ctl_ioh, wd_aux_ctlr,
            WDCTL_RST | WDCTL_IDS); 
        delay(1000);
        bus_space_write_1(cp->ctl_iot, cp->ctl_ioh, wd_aux_ctlr,
            WDCTL_IDS);
        delay(1000);
        (void)bus_space_read_1(cp->cmd_iot, cp->cmd_ioh, wd_error);

	timeout = 0;
	while (timeout++ < PCIIDE_PROBE_WDC_NDELAY) {
		st0 = bus_space_read_1(cp->cmd_iot, cp->cmd_ioh, wd_status);
		bus_space_write_1(cp->cmd_iot, cp->cmd_ioh, wd_sdh,
		    WDSD_IBM | 0x10);
		st1 = bus_space_read_1(cp->cmd_iot, cp->cmd_ioh, wd_status);

		if ((st0 & WDCS_BSY) == 0 || (st1 & WDCS_BSY) == 0)
			return (1);

		delay(PCIIDE_PROBE_WDC_DELAY);
	}
	/* timed out; nothing there */

	return (0);	
}

int
pciide_map_channel_native(sc, pa, chan)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	int chan;
{
	struct pciide_channel *cp = &sc->sc_channels[chan];
	int rv = 1;

	cp->compat = 0;

	if (pci_mapreg_map(pa, PCIIDE_REG_CMD_BASE(chan), PCI_MAPREG_TYPE_IO,
	    0, &cp->cmd_iot, &cp->cmd_ioh, NULL, NULL) != 0) {
		printf("%s: couldn't map %s channel cmd regs\n",
		    sc->sc_dev.dv_xname, PCIIDE_CHANNEL_NAME(chan));
		rv = 0;
	}

	if (pci_mapreg_map(pa, PCIIDE_REG_CTL_BASE(chan), PCI_MAPREG_TYPE_IO,
	    0, &cp->ctl_iot, &cp->ctl_ioh, NULL, NULL) != 0) {
		printf("%s: couldn't map %s channel ctl regs\n",
		    sc->sc_dev.dv_xname, PCIIDE_CHANNEL_NAME(chan));
		rv = 0;
	}

	if ((cp->ih = sc->sc_pci_ih) == NULL) {
		printf("%s: no native-PCI interrupt for use by %s channel\n",
		    sc->sc_dev.dv_xname, PCIIDE_CHANNEL_NAME(chan));
		rv = 0;
	}

	return (rv);
}

int
pciide_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct pciide_attach_args *aa = aux;

	/* only 'wdc's can attach to 'pciide's; easy. */
	if (pnp)
		printf("wdc at %s", pnp);
	printf(" channel %d", aa->channel);
	return (UNCONF);
}

int
pciide_compat_intr(arg)
	void *arg;
{
	struct pciide_channel *cp = arg;

#ifdef DIAGNOSTIC
	/* should only be called for a compat channel */
	if (cp->compat == 0)
		panic("pciide compat intr called for non-compat chan %p\n", cp);
#endif
	/* if there's no handler, that probably means no dev attached */
	if (cp->ihand == NULL)
		return (0);

	return ((*cp->ihand)(cp->ihandarg));
}

int
pciide_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	int i, rv, crv;

	rv = 0;
	for (i = 0; i < PCIIDE_NUM_CHANNELS; i++) {
		cp = &sc->sc_channels[i];

		/* If a compat channel or there's no handler, skip. */
		if (cp->compat || cp->ihand == NULL)
			continue;

		crv = ((*cp->ihand)(cp->ihandarg));
		if (crv == 0)
			;		/* leave rv alone */
		else if (crv == 1)
			rv = 1;		/* claim the intr */
		else if (rv == 0)	/* crv should be -1 in this case */
			rv = crv;	/* if we've done no better, take it */
	}
	return (rv);
}
