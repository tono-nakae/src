/*	$NetBSD: pciide.c,v 1.25 1998/12/03 15:38:59 bouyer Exp $	*/

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
 */

#define DEBUG_DMA   0x01
#define DEBUG_XFERS  0x02
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10
#ifdef WDCDEBUG
int wdcdebug_pciide_mask = DEBUG_PROBE;
#define WDCDEBUG_PRINT(args, level) \
	if (wdcdebug_pciide_mask & (level)) printf args
#else
#define WDCDEBUG_PRINT(args, level)
#endif
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>
#include <dev/pci/pciide_piix_reg.h>
#include <dev/pci/pciide_apollo_reg.h>
#include <dev/pci/pciide_cmd_reg.h>
#include <dev/pci/pciide_cy693_reg.h>
#include <dev/pci/pciide_sis_reg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

/* inlines for reading/writing 8-bit PCI registers */
static __inline u_int8_t pciide_pci_read __P((pci_chipset_tag_t, pcitag_t,
		int));
static __inline u_int8_t
pciide_pci_read(pc, pa, reg)
	pci_chipset_tag_t pc;
	pcitag_t pa;
	int reg;
{
	return (
	    pci_conf_read(pc, pa, (reg & ~0x03)) >> ((reg & 0x03) * 8) & 0xff);
}


static __inline void pciide_pci_write __P((pci_chipset_tag_t, pcitag_t,
		int, u_int8_t));
static __inline void
pciide_pci_write(pc, pa, reg, val)
	pci_chipset_tag_t pc;
	pcitag_t pa;
	int reg;
	u_int8_t val;
{
	pcireg_t pcival;

	pcival = pci_conf_read(pc, pa, (reg & ~0x03));
	pcival &= ~(0xff << ((reg & 0x03) * 8));
	pcival |= (val << ((reg & 0x03) * 8));
	pci_conf_write(pc, pa, (reg & ~0x03), pcival);
}

struct pciide_softc {
	struct wdc_softc	sc_wdcdev;	/* common wdc definitions */

	void			*sc_pci_ih;	/* PCI interrupt handle */
	int			sc_dma_ok;	/* bus-master DMA info */
	bus_space_tag_t		sc_dma_iot;
	bus_space_handle_t	sc_dma_ioh;
	bus_dma_tag_t		sc_dmat;
	/* Chip description */
	const struct pciide_product_desc *sc_pp;
	/* common definitions */
	struct channel_softc *wdc_chanarray[PCIIDE_NUM_CHANNELS];
	/* internal bookkeeping */
	struct pciide_channel {			/* per-channel data */
		struct channel_softc wdc_channel; /* generic part */
		char		*name;
		int		hw_ok;		/* hardware mapped & OK? */
		int		compat;		/* is it compat? */
		void		*ih;		/* compat or pci handle */
		/* DMA tables and DMA map for xfer, for each drive */
		struct pciide_dma_maps {
			bus_dmamap_t    dmamap_table;
			struct idedma_table *dma_table;
			bus_dmamap_t    dmamap_xfer;
		} dma_maps[2];
	} pciide_channels[PCIIDE_NUM_CHANNELS];
};

void default_setup_cap __P((struct pciide_softc*));
void default_setup_chip __P((struct pciide_softc*,
		pci_chipset_tag_t, pcitag_t));
void default_channel_map __P((struct pciide_softc *,
		struct pci_attach_args *, struct pciide_channel *));

void piix_setup_cap __P((struct pciide_softc*));
void piix_setup_chip __P((struct pciide_softc*,
		pci_chipset_tag_t, pcitag_t));
void piix3_4_setup_chip __P((struct pciide_softc*,
		pci_chipset_tag_t, pcitag_t));
void piix_channel_map __P((struct pciide_softc *,
		struct pci_attach_args *, struct pciide_channel *));
static u_int32_t piix_setup_idetim_timings __P((u_int8_t, u_int8_t, u_int8_t));
static u_int32_t piix_setup_idetim_drvs __P((struct ata_drive_datas*));
static u_int32_t piix_setup_sidetim_timings __P((u_int8_t, u_int8_t, u_int8_t));

void apollo_setup_cap __P((struct pciide_softc*));
void apollo_setup_chip __P((struct pciide_softc*,
		pci_chipset_tag_t, pcitag_t));
void apollo_channel_map __P((struct pciide_softc *,
		struct pci_attach_args *, struct pciide_channel *));

void cmd0643_6_setup_cap __P((struct pciide_softc*));
void cmd0643_6_setup_chip __P((struct pciide_softc*,
		pci_chipset_tag_t, pcitag_t));
void cmd_channel_map __P((struct pciide_softc *,
		struct pci_attach_args *, struct pciide_channel *));

void cy693_setup_cap __P((struct pciide_softc*));
void cy693_setup_chip __P((struct pciide_softc*,
		pci_chipset_tag_t, pcitag_t));
void cy693_channel_map __P((struct pciide_softc *,
		struct pci_attach_args *, struct pciide_channel *));

void sis_setup_cap __P((struct pciide_softc*));
void sis_setup_chip __P((struct pciide_softc*,
		pci_chipset_tag_t, pcitag_t));
void sis_channel_map __P((struct pciide_softc *,
		struct pci_attach_args *, struct pciide_channel *));

int  pciide_dma_table_setup __P((struct pciide_softc*, int, int));
int  pciide_dma_init __P((void*, int, int, void *, size_t, int));
void pciide_dma_start __P((void*, int, int, int));
int  pciide_dma_finish __P((void*, int, int, int));
void pciide_print_modes __P((struct pciide_softc *));

struct pciide_product_desc {
    u_int32_t ide_product;
    int ide_flags;
    int ide_num_channels;
    const char *ide_name;
    /* init controller's capabilities for drives probe */
    void (*setup_cap) __P((struct pciide_softc*));
    /* init controller after drives probe */
    void (*setup_chip) __P((struct pciide_softc*, pci_chipset_tag_t, pcitag_t));
    /* map channel if possible/necessary */
    void (*channel_map) __P((struct pciide_softc *,
		struct pci_attach_args *, struct pciide_channel *));
};

/* Flags for ide_flags */
#define CMD_PCI064x_IOEN 0x01 /* CMD-style PCI_COMMAND_IO_ENABLE */
#define ONE_QUEUE         0x02 /* device need serialised access */

/* Default product description for devices not known from this controller */
const struct pciide_product_desc default_product_desc = {
    0,
    0,
    PCIIDE_NUM_CHANNELS,
    "Generic PCI IDE controller",
    default_setup_cap,
    default_setup_chip,
    default_channel_map
};


const struct pciide_product_desc pciide_intel_products[] =  {
    { PCI_PRODUCT_INTEL_82092AA,
      0,
      PCIIDE_NUM_CHANNELS,
      "Intel 82092AA IDE controller",
      default_setup_cap,
      default_setup_chip,
      default_channel_map
    },
    { PCI_PRODUCT_INTEL_82371FB_IDE,
      0,
      PCIIDE_NUM_CHANNELS,
      "Intel 82371FB IDE controller (PIIX)",
      piix_setup_cap,
      piix_setup_chip,
      piix_channel_map
    },
    { PCI_PRODUCT_INTEL_82371SB_IDE,
      0,
      PCIIDE_NUM_CHANNELS,
      "Intel 82371SB IDE Interface (PIIX3)",
      piix_setup_cap,
      piix3_4_setup_chip,
      piix_channel_map
    },
    { PCI_PRODUCT_INTEL_82371AB_IDE,
      0,
      PCIIDE_NUM_CHANNELS,
      "Intel 82371AB IDE controller (PIIX4)",
      piix_setup_cap,
      piix3_4_setup_chip,
      piix_channel_map
    },
    { 0,
      0,
      0,
      NULL,
    }
};
const struct pciide_product_desc pciide_cmd_products[] =  {
    { PCI_PRODUCT_CMDTECH_640,
      ONE_QUEUE | CMD_PCI064x_IOEN,
      PCIIDE_NUM_CHANNELS,
      "CMD Technology PCI0640",
      default_setup_cap,
      default_setup_chip,
      cmd_channel_map
    },
    { PCI_PRODUCT_CMDTECH_643,
      ONE_QUEUE | CMD_PCI064x_IOEN,
      PCIIDE_NUM_CHANNELS,
      "CMD Technology PCI0643",
      cmd0643_6_setup_cap,
      cmd0643_6_setup_chip,
      cmd_channel_map
    },
    { PCI_PRODUCT_CMDTECH_646,
      ONE_QUEUE | CMD_PCI064x_IOEN,
      PCIIDE_NUM_CHANNELS,
      "CMD Technology PCI0646",
      cmd0643_6_setup_cap,
      cmd0643_6_setup_chip,
      cmd_channel_map
    },
    { 0,
      0,
      0,
      NULL,
    }
};

const struct pciide_product_desc pciide_via_products[] =  {
    { PCI_PRODUCT_VIATECH_VT82C586_IDE,
      0,
      PCIIDE_NUM_CHANNELS,
      "VIA Technologies VT82C586 (Apollo VP) IDE Controller",
      apollo_setup_cap,
      apollo_setup_chip,
      apollo_channel_map
     },
    { PCI_PRODUCT_VIATECH_VT82C586A_IDE,
      0,
      PCIIDE_NUM_CHANNELS,
      "VIA Technologies VT82C586A IDE Controller",
      apollo_setup_cap,
      apollo_setup_chip,
      apollo_channel_map
    },
    { 0,
      0,
      0,
      NULL,
    }
};

const struct pciide_product_desc pciide_cypress_products[] =  {
    { PCI_PRODUCT_CONTAQ_82C693,
      0,
      1,
      "Contaq Microsystems CY82C693 IDE Controller",
      cy693_setup_cap,
      cy693_setup_chip,
      cy693_channel_map
    },
    { 0,
      0,
      0,
      NULL,
    }
};

const struct pciide_product_desc pciide_sis_products[] =  {
    { PCI_PRODUCT_SIS_5597_IDE,
      0,
      PCIIDE_NUM_CHANNELS,
      "Silicon Integrated System 5597/5598 IDE controller",
      sis_setup_cap,
      sis_setup_chip,
      sis_channel_map
    },
    { 0,
      0,
      0,
      NULL,
    }
};

struct pciide_vendor_desc {
    u_int32_t ide_vendor;
    const struct pciide_product_desc *ide_products;
};

const struct pciide_vendor_desc pciide_vendors[] = {
    { PCI_VENDOR_INTEL, pciide_intel_products },
    { PCI_VENDOR_CMDTECH, pciide_cmd_products },
    { PCI_VENDOR_VIATECH, pciide_via_products },
    { PCI_VENDOR_CONTAQ, pciide_cypress_products },
    { PCI_VENDOR_SIS, pciide_sis_products },
    { 0, NULL }
};


#define	PCIIDE_CHANNEL_NAME(chan)	((chan) == 0 ? "primary" : "secondary")

/* options passed via the 'flags' config keyword */
#define PCIIDE_OPTIONS_DMA	0x01

int	pciide_match __P((struct device *, struct cfdata *, void *));
void	pciide_attach __P((struct device *, struct device *, void *));

struct cfattach pciide_ca = {
	sizeof(struct pciide_softc), pciide_match, pciide_attach
};

int	pciide_mapregs_compat __P((struct pciide_softc *,
	    struct pci_attach_args *, struct pciide_channel *, int,
	    bus_size_t *, bus_size_t*));
int	pciide_mapregs_native __P((struct pciide_softc *,
	    struct pci_attach_args *, struct pciide_channel *,
	    bus_size_t *, bus_size_t *));
void	pciide_mapchan __P((struct pciide_softc *,
	    struct pci_attach_args *, struct pciide_channel *, int,
	    bus_size_t *, bus_size_t *));
int	pciiide_chan_candisable __P((struct pciide_softc *,
	    struct pci_attach_args *, struct pciide_channel *,
	    bus_size_t, bus_size_t));
void	pciide_map_compat_intr __P((struct pciide_softc *,
	    struct pci_attach_args *, struct pciide_channel *, int, int));
int	pciide_print __P((void *, const char *pnp));
int	pciide_compat_intr __P((void *));
int	pciide_pci_intr __P((void *));
const struct pciide_product_desc* pciide_lookup_product __P((u_int32_t));

const struct pciide_product_desc*
pciide_lookup_product(id)
    u_int32_t id;
{
    const struct pciide_product_desc *pp;
    const struct pciide_vendor_desc *vp;

    for (vp = pciide_vendors; vp->ide_products != NULL; vp++)
	if (PCI_VENDOR(id) == vp->ide_vendor)
	    break;

    if ((pp = vp->ide_products) == NULL)
	return NULL;

    for (; pp->ide_name != NULL; pp++)
	if (PCI_PRODUCT(id) == pp->ide_product)
	    break;
    
    if (pp->ide_name == NULL)
	return NULL;
    return pp;
}

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
	pcitag_t tag = pa->pa_tag;
	struct pciide_softc *sc = (struct pciide_softc *)self;
	struct pciide_channel *cp;
	pcireg_t class, interface, csr;
	pci_intr_handle_t intrhandle;
	const char *intrstr;
	char devinfo[256];
	int i;

        sc->sc_pp = pciide_lookup_product(pa->pa_id);
	if (sc->sc_pp == NULL) {
		sc->sc_pp = &default_product_desc;
		pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
		printf(": %s (rev. 0x%02x)\n", devinfo,
		    PCI_REVISION(pa->pa_class));
	} else {
		printf(": %s\n", sc->sc_pp->ide_name);
	}

	if ((pa->pa_flags & PCI_FLAGS_IO_ENABLED) == 0) {
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		/*
		 * For a CMD PCI064x, the use of PCI_COMMAND_IO_ENABLE
		 * and base adresses registers can be disabled at
		 * hardware level. In this case, the device is wired
		 * in compat mode and its first channel is always enabled,
		 * but we can't rely on PCI_COMMAND_IO_ENABLE.
		 * In fact, it seems that the first channel of the CMD PCI0640
		 * can't be disabled.
		 */
#ifndef PCIIDE_CMD064x_DISABLE
		if ((sc->sc_pp->ide_flags & CMD_PCI064x_IOEN) == 0) {
#else
		if (1) {
#endif
			printf("%s: device disabled (at %s)\n",
		 	   sc->sc_wdcdev.sc_dev.dv_xname,
		  	  (csr & PCI_COMMAND_IO_ENABLE) == 0 ?
			  "device" : "bridge");
			return;
		}
	}

	class = pci_conf_read(pc, tag, PCI_CLASS_REG);
	interface = PCI_INTERFACE(class);

	/*
	 * Set up PCI interrupt only if at last one channel is in native mode.
	 * At last one device (CMD PCI0640) has a default value of 14, which
	 * will be mapped even if both channels are in compat-only mode.
	 */
	if (interface & (PCIIDE_INTERFACE_PCI(0) | PCIIDE_INTERFACE_PCI(1))) {
		if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
		    pa->pa_intrline, &intrhandle) != 0) {
			printf("%s: couldn't map native-PCI interrupt\n",
			    sc->sc_wdcdev.sc_dev.dv_xname);
		} else {
			intrstr = pci_intr_string(pa->pa_pc, intrhandle);
			sc->sc_pci_ih = pci_intr_establish(pa->pa_pc,
			    intrhandle, IPL_BIO, pciide_pci_intr, sc);
			if (sc->sc_pci_ih != NULL) {
				printf("%s: using %s for native-PCI "
				    "interrupt\n",
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    intrstr ? intrstr : "unknown interrupt");
			} else {
				printf("%s: couldn't establish native-PCI "
				    "interrupt",
				    sc->sc_wdcdev.sc_dev.dv_xname);
				if (intrstr != NULL)
					printf(" at %s", intrstr); 
				printf("\n");
			}
		}
	}

	/*
	 * Map DMA registers, if DMA is supported.
	 *
	 * Note that sc_dma_ok is the right variable to test to see if
	 * DMA can be done.  If the interface doesn't support DMA,
	 * sc_dma_ok will never be non-zero.  If the DMA regs couldn't
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
		printf("%s: bus-master DMA support present",
		    sc->sc_wdcdev.sc_dev.dv_xname);
		if (sc->sc_pp == &default_product_desc &&
		    (sc->sc_wdcdev.sc_dev.dv_cfdata->cf_flags &
		    PCIIDE_OPTIONS_DMA) == 0) {
			printf(", but unused (no driver support)");
			sc->sc_dma_ok = 0;
		} else {
			sc->sc_dma_ok = (pci_mapreg_map(pa,
			    PCIIDE_REG_BUS_MASTER_DMA, PCI_MAPREG_TYPE_IO, 0,
			    &sc->sc_dma_iot, &sc->sc_dma_ioh, NULL, NULL) == 0);
			sc->sc_dmat = pa->pa_dmat;
			if (sc->sc_dma_ok == 0) {
				printf(", but unused (couldn't map registers)");
			} else {
				if (sc->sc_pp == &default_product_desc)
					printf(", used without full driver "
					    "support");
				sc->sc_wdcdev.dma_arg = sc;
				sc->sc_wdcdev.dma_init = pciide_dma_init;
				sc->sc_wdcdev.dma_start = pciide_dma_start;
				sc->sc_wdcdev.dma_finish = pciide_dma_finish;
			}
		}
	} else {
		printf("%s: pciide0: hardware does not support DMA",
		    sc->sc_wdcdev.sc_dev.dv_xname);
	}
	printf("\n");
	sc->sc_pp->setup_cap(sc);
	sc->sc_wdcdev.channels = sc->wdc_chanarray;
	sc->sc_wdcdev.nchannels = sc->sc_pp->ide_num_channels;;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA16;

	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		sc->wdc_chanarray[i] = &cp->wdc_channel;

		cp->name = PCIIDE_CHANNEL_NAME(i);

		cp->wdc_channel.channel = i;
		cp->wdc_channel.wdc = &sc->sc_wdcdev;
		if (i > 0 && (sc->sc_pp->ide_flags & ONE_QUEUE)) {
		    cp->wdc_channel.ch_queue =
			sc->pciide_channels[0].wdc_channel.ch_queue;
		} else {
		    cp->wdc_channel.ch_queue =
		        malloc(sizeof(struct channel_queue), M_DEVBUF,
			M_NOWAIT);
		}
		if (cp->wdc_channel.ch_queue == NULL) {
		    printf("%s %s channel: "
			"can't allocate memory for command queue",
			sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
			continue;
		}
		printf("%s: %s channel %s to %s mode\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
		    (interface & PCIIDE_INTERFACE_SETTABLE(i)) ?
		      "configured" : "wired",
		    (interface & PCIIDE_INTERFACE_PCI(i)) ? "native-PCI" :
		      "compatibility");

		/*
		 * sc->sc_pp->channel_map() will also call wdcattach.
		 * Eventually the channel will be  disabled if there's no
		 * drive present. sc->hw_ok will be updated accordingly.
		 */
		sc->sc_pp->channel_map(sc, pa, cp);
			
	}
	/* Now that all drives are know, setup DMA, etc ...*/
	sc->sc_pp->setup_chip(sc, pc, tag);
	if (sc->sc_dma_ok) {
		csr = pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG);
		csr |= PCI_COMMAND_MASTER_ENABLE;
		pci_conf_write(pc, tag, PCI_COMMAND_STATUS_REG, csr);
	}
	WDCDEBUG_PRINT(("pciide: command/status register=%x\n",
	    pci_conf_read(pc, tag, PCI_COMMAND_STATUS_REG)), DEBUG_PROBE);
}

int
pciide_mapregs_compat(sc, pa, cp, compatchan, cmdsizep, ctlsizep)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	int compatchan;
	bus_size_t *cmdsizep, *ctlsizep;
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	int rv = 1;

	cp->compat = 1;
	*cmdsizep = PCIIDE_COMPAT_CMD_SIZE;
	*ctlsizep = PCIIDE_COMPAT_CTL_SIZE;

	wdc_cp->cmd_iot = pa->pa_iot;
	if (bus_space_map(wdc_cp->cmd_iot, PCIIDE_COMPAT_CMD_BASE(compatchan),
	    PCIIDE_COMPAT_CMD_SIZE, 0, &wdc_cp->cmd_ioh) != 0) {
		printf("%s: couldn't map %s channel cmd regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		rv = 0;
	}

	wdc_cp->ctl_iot = pa->pa_iot;
	if (bus_space_map(wdc_cp->ctl_iot, PCIIDE_COMPAT_CTL_BASE(compatchan),
	    PCIIDE_COMPAT_CTL_SIZE, 0, &wdc_cp->ctl_ioh) != 0) {
		printf("%s: couldn't map %s channel ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh,
		    PCIIDE_COMPAT_CMD_SIZE);
		rv = 0;
	}

	return (rv);
}

int
pciide_mapregs_native(sc, pa, cp, cmdsizep, ctlsizep)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	bus_size_t *cmdsizep, *ctlsizep;
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	cp->compat = 0;

	if ((cp->ih = sc->sc_pci_ih) == NULL) {
		printf("%s: no native-PCI interrupt for use by %s channel\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return 0;
	}
	if (pci_mapreg_map(pa, PCIIDE_REG_CMD_BASE(wdc_cp->channel),
	    PCI_MAPREG_TYPE_IO, 0,
	    &wdc_cp->cmd_iot, &wdc_cp->cmd_ioh, NULL, cmdsizep) != 0) {
		printf("%s: couldn't map %s channel cmd regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return 0;
	}

	if (pci_mapreg_map(pa, PCIIDE_REG_CTL_BASE(wdc_cp->channel),
	    PCI_MAPREG_TYPE_IO, 0,
	    &wdc_cp->ctl_iot, &wdc_cp->ctl_ioh, NULL, ctlsizep) != 0) {
		printf("%s: couldn't map %s channel ctl regs\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh, *cmdsizep);
		return 0;
	}
	return (1);
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
	return (wdcintr(&cp->wdc_channel));
}

int
pciide_pci_intr(arg)
	void *arg;
{
	struct pciide_softc *sc = arg;
	struct pciide_channel *cp;
	struct channel_softc *wdc_cp;
	int i, rv, crv;

	rv = 0;
	for (i = 0; i < sc->sc_wdcdev.nchannels; i++) {
		cp = &sc->pciide_channels[i];
		wdc_cp = &cp->wdc_channel;

		/* If a compat channel skip. */
		if (cp->compat)
			continue;
		/* if this channel not waiting for intr, skip */
		if ((wdc_cp->ch_flags & WDCF_IRQ_WAIT) == 0)
			continue;

		crv = wdcintr(wdc_cp);
		if (crv == 0)
			;		/* leave rv alone */
		else if (crv == 1)
			rv = 1;		/* claim the intr */
		else if (rv == 0)	/* crv should be -1 in this case */
			rv = crv;	/* if we've done no better, take it */
	}
	return (rv);
}

int
pciide_dma_table_setup(sc, channel, drive)
	struct pciide_softc *sc;
	int channel, drive;
{
	bus_dma_segment_t seg;
	int error, rseg;
	const bus_size_t dma_table_size =
	    sizeof(struct idedma_table) * NIDEDMA_TABLES;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	/* Allocate memory for the DMA tables and map it */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, dma_table_size,
	    IDEDMA_TBL_ALIGN, IDEDMA_TBL_ALIGN, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s:%d: unable to allocate table DMA for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, rseg,
	    dma_table_size,
	    (caddr_t *)&dma_maps->dma_table,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s:%d: unable to map table DMA for"
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	WDCDEBUG_PRINT(("pciide_dma_table_setup: table at %p len %ld, "
	    "phy 0x%lx\n", dma_maps->dma_table, dma_table_size,
	    seg.ds_addr), DEBUG_PROBE);

	/* Create and load table DMA map for this disk */
	if ((error = bus_dmamap_create(sc->sc_dmat, dma_table_size,
	    1, dma_table_size, IDEDMA_TBL_ALIGN, BUS_DMA_NOWAIT,
	    &dma_maps->dmamap_table)) != 0) {
		printf("%s:%d: unable to create table DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	if ((error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_table,
	    dma_maps->dma_table,
	    dma_table_size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s:%d: unable to load table DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	WDCDEBUG_PRINT(("pciide_dma_table_setup: phy addr of table 0x%lx\n",
	    dma_maps->dmamap_table->dm_segs[0].ds_addr), DEBUG_PROBE);
	/* Create a xfer DMA map for this drive */
	if ((error = bus_dmamap_create(sc->sc_dmat, IDEDMA_BYTE_COUNT_MAX,
	    NIDEDMA_TABLES, IDEDMA_BYTE_COUNT_MAX, IDEDMA_BYTE_COUNT_ALIGN,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
	    &dma_maps->dmamap_xfer)) != 0) {
		printf("%s:%d: unable to create xfer DMA map for "
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}
	return 0;
}

int
pciide_dma_init(v, channel, drive, databuf, datalen, flags)
	void *v;
	int channel, drive;
	void *databuf;
	size_t datalen;
	int flags;
{
	struct pciide_softc *sc = v;
	int error, seg;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	error = bus_dmamap_load(sc->sc_dmat,
	    dma_maps->dmamap_xfer,
	    databuf, datalen, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s:%d: unable to load xfer DMA map for"
		    "drive %d, error=%d\n", sc->sc_wdcdev.sc_dev.dv_xname,
		    channel, drive, error);
		return error;
	}

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	for (seg = 0; seg < dma_maps->dmamap_xfer->dm_nsegs; seg++) {
#ifdef DIAGNOSTIC
		/* A segment must not cross a 64k boundary */
		{
		u_long phys = dma_maps->dmamap_xfer->dm_segs[seg].ds_addr;
		u_long len = dma_maps->dmamap_xfer->dm_segs[seg].ds_len;
		if ((phys & ~IDEDMA_BYTE_COUNT_MASK) !=
		    ((phys + len - 1) & ~IDEDMA_BYTE_COUNT_MASK)) {
			printf("pciide_dma: segment %d physical addr 0x%lx"
			    " len 0x%lx not properly aligned\n",
			    seg, phys, len);
			panic("pciide_dma: buf align");
		}
		}
#endif
		dma_maps->dma_table[seg].base_addr =
		    dma_maps->dmamap_xfer->dm_segs[seg].ds_addr;
		dma_maps->dma_table[seg].byte_count =
		    dma_maps->dmamap_xfer->dm_segs[seg].ds_len &
		    IDEDMA_BYTE_COUNT_MASK;
		WDCDEBUG_PRINT(("\t seg %d len %d addr 0x%x\n",
		   seg, dma_maps->dma_table[seg].byte_count,
		   dma_maps->dma_table[seg].base_addr), DEBUG_DMA);

	}
	dma_maps->dma_table[dma_maps->dmamap_xfer->dm_nsegs -1].byte_count |=
		IDEDMA_BYTE_COUNT_EOT;

	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_table, 0,
	    dma_maps->dmamap_table->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* Maps are ready. Start DMA function */
#ifdef DIAGNOSTIC
	if (dma_maps->dmamap_table->dm_segs[0].ds_addr & ~IDEDMA_TBL_MASK) {
		printf("pciide_dma_init: addr 0x%lx not properly aligned\n",
		    dma_maps->dmamap_table->dm_segs[0].ds_addr);
		panic("pciide_dma_init: table align");
	}
#endif

	/* Clear status bits */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL + IDEDMA_SCH_OFFSET * channel,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CTL + IDEDMA_SCH_OFFSET * channel));
	/* Write table addr */
	bus_space_write_4(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_TBL + IDEDMA_SCH_OFFSET * channel,
	    dma_maps->dmamap_table->dm_segs[0].ds_addr);
	/* set read/write */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel,
	    (flags & WDC_DMA_READ) ? IDEDMA_CMD_WRITE: 0);
	return 0;
}

void
pciide_dma_start(v, channel, drive, flags)
	void *v;
	int channel, drive, flags;
{
	struct pciide_softc *sc = v;

	WDCDEBUG_PRINT(("pciide_dma_start\n"),DEBUG_XFERS);
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel) | IDEDMA_CMD_START);
}

int
pciide_dma_finish(v, channel, drive, flags)
	void *v;
	int channel, drive;
	int flags;
{
	struct pciide_softc *sc = v;
	u_int8_t status;
	struct pciide_dma_maps *dma_maps =
	    &sc->pciide_channels[channel].dma_maps[drive];

	/* Unload the map of the data buffer */
	bus_dmamap_sync(sc->sc_dmat, dma_maps->dmamap_xfer, 0,
	    dma_maps->dmamap_xfer->dm_mapsize,
	    (flags & WDC_DMA_READ) ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, dma_maps->dmamap_xfer);

	status = bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL + IDEDMA_SCH_OFFSET * channel);
	WDCDEBUG_PRINT(("pciide_dma_finish: status 0x%x\n", status),
	    DEBUG_XFERS);

	/* stop DMA channel */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel,
	    bus_space_read_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		IDEDMA_CMD + IDEDMA_SCH_OFFSET * channel) & ~IDEDMA_CMD_START);

	/* Clear status bits */
	bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
	    IDEDMA_CTL + IDEDMA_SCH_OFFSET * channel,
	    status);

	if ((status & IDEDMA_CTL_ERR) != 0) {
		printf("%s:%d:%d: Bus-Master DMA error: status=0x%x\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, channel, drive, status);
		return -1;
	}

	if ((flags & WDC_DMA_POLL) == 0 && (status & IDEDMA_CTL_INTR) == 0) {
		printf("%s:%d:%d: Bus-Master DMA error: missing interrupt, "
		    "status=0x%x\n", sc->sc_wdcdev.sc_dev.dv_xname, channel,
		    drive, status);
		return -1;
	}

	if ((status & IDEDMA_CTL_ACT) != 0) {
		/* data underrun, may be a valid condition for ATAPI */
		return 1;
	}

	return 0;
}

/* some common code used by several chip channel_map */
void
pciide_mapchan(sc, pa, cp, interface, cmdsizep, ctlsizep)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	int interface;
	struct pciide_channel *cp;
	bus_size_t *cmdsizep, *ctlsizep;
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if (interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel))
		cp->hw_ok = pciide_mapregs_native(sc, pa, cp,
		    cmdsizep, ctlsizep);
	else
		cp->hw_ok = pciide_mapregs_compat(sc, pa, cp, wdc_cp->channel,
		    cmdsizep, ctlsizep);
	if (cp->hw_ok == 0)
		return;
	wdc_cp->data32iot = wdc_cp->cmd_iot;
	wdc_cp->data32ioh = wdc_cp->cmd_ioh;
	wdcattach(wdc_cp);
}

/*
 * Generic code to call to know if a channel can be disabled. Return 1
 * if channel can be disabled, 0 if not
 */
int
pciiide_chan_candisable(sc, pa, cp, cmdsize, ctlsize)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	bus_size_t cmdsize, ctlsize;
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if ((wdc_cp->ch_drive[0].drive_flags & DRIVE) == 0 &&
	    (wdc_cp->ch_drive[1].drive_flags & DRIVE) == 0) {
		printf("%s: disabling %s channel (no drives)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		cp->hw_ok = 0;
		return 1;
	}
	return 0;
}

/*
 * generic code to map the compat intr if hw_ok=1 and it is a compat channel.
 * Set hw_ok=0 on failure
 */
void
pciide_map_compat_intr(sc, pa, cp, compatchan, interface)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
	int compatchan, interface;
{
	struct channel_softc *wdc_cp = &cp->wdc_channel;

	if (cp->hw_ok == 0)
		return;
	if ((interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel)) != 0)
		return;

	cp->ih = pciide_machdep_compat_intr_establish(&sc->sc_wdcdev.sc_dev,
	    pa, compatchan, pciide_compat_intr, cp);
	if (cp->ih == NULL) {
		printf("%s: no compatibility interrupt for use by %s "
		    "channel\n", sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		cp->hw_ok = 0;
	}
}

void
pciide_print_modes(sc)
	struct pciide_softc *sc;
{
	int channel, drive;
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		chp = &sc->pciide_channels[channel].wdc_channel;
		for (drive = 0; drive < 2; drive++) {
			drvp = &chp->ch_drive[drive];
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			printf("%s(%s:%d:%d): using PIO mode %d",
			    drvp->drv_softc->dv_xname,
			    sc->sc_wdcdev.sc_dev.dv_xname,
			    channel, drive, drvp->PIO_mode);
			if (drvp->drive_flags & DRIVE_DMA)
				printf(", DMA mode %d", drvp->DMA_mode);
			if (drvp->drive_flags & DRIVE_UDMA)
				printf(", Ultra-DMA mode %d", drvp->UDMA_mode);
			if (drvp->drive_flags & (DRIVE_DMA | DRIVE_UDMA))
				printf(" (using DMA data transfers)");
			printf("\n");
		}
	}
}

void
default_setup_cap(sc)
	struct pciide_softc *sc;
{
	if (sc->sc_dma_ok)
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_DMA;
	sc->sc_wdcdev.pio_mode = 0;
	sc->sc_wdcdev.dma_mode = 0;
}

void
default_setup_chip(sc, pc, tag)
	struct pciide_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	int channel, drive, idedma_ctl;
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;

	if (sc->sc_dma_ok == 0)
		return; /* nothing to do */

	/* Allocate DMA maps */
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		idedma_ctl = 0;
		chp = &sc->pciide_channels[channel].wdc_channel;
		for (drive = 0; drive < 2; drive++) {
			drvp = &chp->ch_drive[drive];
			/* If no drive, skip */
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			if ((drvp->drive_flags & DRIVE_DMA) == 0)
				continue;
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				printf("%s:%d:%d: can't allocate DMA maps, "
				    "using PIO transfers\n",
				    sc->sc_wdcdev.sc_dev.dv_xname,
				    channel, drive);
				drvp->drive_flags &= ~DRIVE_DMA;
			}
			printf("%s:%d:%d: using DMA data tranferts\n",
			    sc->sc_wdcdev.sc_dev.dv_xname,
			    channel, drive);
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * channel),
			    idedma_ctl);
		}
	}

}

void
default_channel_map(sc, pa, cp)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
{	
	bus_size_t cmdsize, ctlsize;
	pcireg_t csr;
	const char *failreason = NULL;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	int interface =
	    PCI_INTERFACE(pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG));

	if (interface & PCIIDE_INTERFACE_PCI(wdc_cp->channel))
		cp->hw_ok = pciide_mapregs_native(sc, pa, cp,
		    &cmdsize, &ctlsize);
	else
		cp->hw_ok = pciide_mapregs_compat(sc, pa, cp, wdc_cp->channel,
		    &cmdsize, &ctlsize);
	if (cp->hw_ok == 0)
		return;

	/*
	 * Check to see if something appears to be there.
	 */
	if (!wdcprobe(wdc_cp)) {
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
	if (wdcprobe(wdc_cp))
		failreason = "other hardware responding at addresses";
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, csr);

out:
	if (failreason) {
		printf("%s: %s channel ignored (%s)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name,
		    failreason);
		cp->hw_ok = 0;
		bus_space_unmap(wdc_cp->cmd_iot, wdc_cp->cmd_ioh, cmdsize);
		bus_space_unmap(wdc_cp->ctl_iot, wdc_cp->ctl_ioh, ctlsize);
	}
	pciide_map_compat_intr(sc, pa, cp, wdc_cp->channel, interface);
	if (cp->hw_ok) {
		wdc_cp->data32iot = wdc_cp->cmd_iot;
		wdc_cp->data32ioh = wdc_cp->cmd_ioh;
		wdcattach(wdc_cp);
	}
}

void
piix_setup_cap(sc)
	struct pciide_softc *sc;
{
	if (sc->sc_pp->ide_product == PCI_PRODUCT_INTEL_82371AB_IDE)
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE |
	    WDC_CAPABILITY_DMA;
	sc->sc_wdcdev.pio_mode = 4;
	sc->sc_wdcdev.dma_mode = 2;
	sc->sc_wdcdev.udma_mode = 2;
}

void
piix_setup_chip(sc, pc, tag)
	struct pciide_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	struct channel_softc *chp;
	u_int8_t mode[2];
	u_int8_t channel, drive;
	u_int32_t oidetim, idetim, sidetim, idedma_ctl;
	struct ata_drive_datas *drvp;

	oidetim = pci_conf_read(pc, tag, PIIX_IDETIM);
	idetim = sidetim = 0;

	WDCDEBUG_PRINT(("piix_setup_chip: old idetim=0x%x, sidetim=0x%x\n",
	    oidetim,
	    pci_conf_read(pc, tag, PIIX_SIDETIM)), DEBUG_PROBE);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		chp = &sc->pciide_channels[channel].wdc_channel;
		drvp = chp->ch_drive;
		idedma_ctl = 0;
		/* If channel disabled, no need to go further */
		if ((PIIX_IDETIM_READ(oidetim, channel) & PIIX_IDETIM_IDE) == 0)
			continue;
		/* set up new idetim: Enable IDE registers decode */
		idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE,
		    channel);

		/* setup DMA if needed */
		for (drive = 0; drive < 2; drive++) {
			if (drvp[drive].drive_flags & DRIVE_DMA &&
			    pciide_dma_table_setup(sc, channel, drive) != 0) {
				drvp[drive].drive_flags &= ~DRIVE_DMA;
			}
		}

		/*
		 * Here we have to mess up with drives mode: PIIX can't have
		 * different timings for master and slave drives.
		 * We need to find the best combination.
		 */

		/* If both drives supports DMA, takes the lower mode */
		if ((drvp[0].drive_flags & DRIVE_DMA) &&
		    (drvp[1].drive_flags & DRIVE_DMA)) {
			mode[0] = mode[1] =
			    min(drvp[0].DMA_mode, drvp[1].DMA_mode);
			    drvp[0].DMA_mode = mode[0];
			goto ok;
		}
		/*
		 * If only one drive supports DMA, use its mode, and
		 * put the other one in PIO mode 0 if mode not compatible
		 */
		if (drvp[0].drive_flags & DRIVE_DMA) {
			mode[0] = drvp[0].DMA_mode;
			mode[1] = drvp[1].PIO_mode;
			if (piix_isp_pio[mode[1]] != piix_isp_dma[mode[0]] ||
			    piix_rtc_pio[mode[1]] != piix_rtc_dma[mode[0]])
				mode[1] = 0;
			goto ok;
		}
		if (drvp[1].drive_flags & DRIVE_DMA) {
			mode[1] = drvp[1].DMA_mode;
			mode[0] = drvp[0].PIO_mode;
			if (piix_isp_pio[mode[0]] != piix_isp_dma[mode[1]] ||
			    piix_rtc_pio[mode[0]] != piix_rtc_dma[mode[1]])
				mode[0] = 0;
			goto ok;
		}
		/*
		 * If both drives are not DMA, takes the lower mode, unless
		 * one of them is PIO mode < 2
		 */
		if (drvp[0].PIO_mode < 2) {
			mode[0] = 0;
			mode[1] = drvp[1].PIO_mode;
		} else if (drvp[1].PIO_mode < 2) {
			mode[1] = 0;
			mode[0] = drvp[0].PIO_mode;
		} else {
			mode[0] = mode[1] =
			    min(drvp[1].PIO_mode, drvp[0].PIO_mode);
		}
ok:		/* The modes are setup */
		for (drive = 0; drive < 2; drive++) {
			if (drvp[drive].drive_flags & DRIVE_DMA) {
				drvp[drive].DMA_mode = mode[drive];
				idetim |= piix_setup_idetim_timings(
				    mode[drive], 1, channel);
				goto end;
			} else
				drvp[drive].PIO_mode = mode[drive];
		}
		/* If we are there, none of the drives are DMA */
		if (mode[0] >= 2)
			idetim |= piix_setup_idetim_timings(
			    mode[0], 0, channel);
		else 
			idetim |= piix_setup_idetim_timings(
			    mode[1], 0, channel);
end:		/*
		 * timing mode is now set up in the controller. Enable
		 * it per-drive
		 */
		for (drive = 0; drive < 2; drive++) {
			/* If no drive, skip */
			if ((drvp[drive].drive_flags & DRIVE) == 0)
				continue;
			idetim |= piix_setup_idetim_drvs(&drvp[drive]);
			if (drvp[drive].drive_flags & DRIVE_DMA)
				idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * channel),
			    idedma_ctl);
		}
	}
	pciide_print_modes(sc);
	WDCDEBUG_PRINT(("piix_setup_chip: idetim=0x%x, sidetim=0x%x\n",
	    idetim, sidetim), DEBUG_PROBE);
	pci_conf_write(pc, tag, PIIX_IDETIM, idetim);
	pci_conf_write(pc, tag, PIIX_SIDETIM, sidetim);
}

void
piix3_4_setup_chip(sc, pc, tag)
	struct pciide_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	int channel, drive;
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;
	u_int32_t oidetim, idetim, sidetim, udmareg, idedma_ctl;

	idetim = sidetim = udmareg = 0;
	oidetim = pci_conf_read(pc, tag, PIIX_IDETIM);

	WDCDEBUG_PRINT(("piix3_4_setup_chip: old idetim=0x%x, sidetim=0x%x",
	    oidetim,
	    pci_conf_read(pc, tag, PIIX_SIDETIM)), DEBUG_PROBE);
	if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
		WDCDEBUG_PRINT((", udamreg 0x%x",
		    pci_conf_read(pc, tag, PIIX_UDMAREG)),
		    DEBUG_PROBE);
	}
	WDCDEBUG_PRINT(("\n"), DEBUG_PROBE);

	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		chp = &sc->pciide_channels[channel].wdc_channel;
		idedma_ctl = 0;
		/* If channel disabled, no need to go further */
		if ((PIIX_IDETIM_READ(oidetim, channel) & PIIX_IDETIM_IDE) == 0)
			continue;
		/* set up new idetim: Enable IDE registers decode */
		idetim = PIIX_IDETIM_SET(idetim, PIIX_IDETIM_IDE,
		    channel);
		for (drive = 0; drive < 2; drive++) {
			drvp = &chp->ch_drive[drive];
			/* If no drive, skip */
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			/* add timing values, setup DMA if needed */
			if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
			    (drvp->drive_flags & DRIVE_UDMA) == 0) ||
			    sc->sc_dma_ok == 0) {
				drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
				goto pio;
			}
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
				goto pio;
			}
			if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
			    (drvp->drive_flags & DRIVE_UDMA)) {
				/* use Ultra/DMA */
				drvp->drive_flags &= ~DRIVE_DMA;
				udmareg |= PIIX_UDMACTL_DRV_EN(
				    channel, drive);
				udmareg |= PIIX_UDMATIM_SET(
				    piix4_sct_udma[drvp->UDMA_mode],
				    channel, drive);
			} else {
				/* use Multiword DMA */
				drvp->drive_flags &= ~DRIVE_UDMA;
				if (drive == 0) {
					idetim |= piix_setup_idetim_timings(
					    drvp->DMA_mode, 1, channel);
				} else {
					sidetim |= piix_setup_sidetim_timings(
						drvp->DMA_mode, 1, channel);
					idetim =PIIX_IDETIM_SET(idetim,
					    PIIX_IDETIM_SITRE, channel);
				}
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		
pio:			/* use PIO mode */
			idetim |= piix_setup_idetim_drvs(drvp);
			if (drive == 0) {
				idetim |= piix_setup_idetim_timings(
				    drvp->PIO_mode, 0, channel);
			} else {
				sidetim |= piix_setup_sidetim_timings(
					drvp->PIO_mode, 0, channel);
				idetim =PIIX_IDETIM_SET(idetim,
				    PIIX_IDETIM_SITRE, channel);
			}
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * channel),
			    idedma_ctl);
		}
	}

	pciide_print_modes(sc);
	WDCDEBUG_PRINT(("piix3_4_setup_chip: idetim=0x%x, sidetim=0x%x",
	    idetim, sidetim), DEBUG_PROBE);
	if (sc->sc_wdcdev.cap & WDC_CAPABILITY_UDMA) {
		WDCDEBUG_PRINT((", udmareg=0x%x", udmareg), DEBUG_PROBE);
		pci_conf_write(pc, tag, PIIX_UDMAREG, udmareg);
	}
	WDCDEBUG_PRINT(("\n"), DEBUG_PROBE);
	pci_conf_write(pc, tag, PIIX_IDETIM, idetim);
	pci_conf_write(pc, tag, PIIX_SIDETIM, sidetim);
}

/* setup ISP and RTC fields, based on mode */
static u_int32_t
piix_setup_idetim_timings(mode, dma, channel)
	u_int8_t mode;
	u_int8_t dma;
	u_int8_t channel;
{
	
	if (dma)
		return PIIX_IDETIM_SET(0,
		    PIIX_IDETIM_ISP_SET(piix_isp_dma[mode]) | 
		    PIIX_IDETIM_RTC_SET(piix_rtc_dma[mode]),
		    channel);
	else 
		return PIIX_IDETIM_SET(0,
		    PIIX_IDETIM_ISP_SET(piix_isp_pio[mode]) | 
		    PIIX_IDETIM_RTC_SET(piix_rtc_pio[mode]),
		    channel);
}

/* setup DTE, PPE, IE and TIME field based on PIO mode */
static u_int32_t
piix_setup_idetim_drvs(drvp)
	struct ata_drive_datas *drvp;
{
	u_int32_t ret = 0;
	struct channel_softc *chp = drvp->chnl_softc;
	u_int8_t channel = chp->channel;
	u_int8_t drive = drvp->drive;

	/*
	 * If drive is using UDMA, timings setups are independant
	 * So just check DMA and PIO here.
	 */
	if (drvp->drive_flags & DRIVE_DMA) {
		/* if mode = DMA mode 0, use compatible timings */
		if ((drvp->drive_flags & DRIVE_DMA) &&
		    drvp->DMA_mode == 0) {
			drvp->PIO_mode = 0;
			return ret;
		}
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_TIME(drive), channel);
		/*
		 * PIO and DMA timings are the same, use fast timings for PIO
		 * too, else use compat timings.
		 */
		if ((piix_isp_pio[drvp->PIO_mode] !=
		    piix_isp_dma[drvp->DMA_mode]) ||
		    (piix_rtc_pio[drvp->PIO_mode] !=
		    piix_rtc_dma[drvp->DMA_mode]))
			drvp->PIO_mode = 0;
		/* if PIO mode <= 2, use compat timings for PIO */
		if (drvp->PIO_mode <= 2) {
			ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_DTE(drive),
			    channel);
			return ret;
		}
	}

	/*
	 * Now setup PIO modes. If mode < 2, use compat timings.
	 * Else enable fast timings. Enable IORDY and prefetch/post
	 * if PIO mode >= 3.
	 */

	if (drvp->PIO_mode < 2)
		return ret;

	ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_TIME(drive), channel);
	if (drvp->PIO_mode >= 3) {
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_IE(drive), channel);
		ret = PIIX_IDETIM_SET(ret, PIIX_IDETIM_PPE(drive), channel);
	}
	return ret;
}

/* setup values in SIDETIM registers, based on mode */
static u_int32_t
piix_setup_sidetim_timings(mode, dma, channel)
	u_int8_t mode;
	u_int8_t dma;
	u_int8_t channel;
{
	if (dma)
		return PIIX_SIDETIM_ISP_SET(piix_isp_dma[mode], channel) |
		    PIIX_SIDETIM_RTC_SET(piix_rtc_dma[mode], channel);
	else 
		return PIIX_SIDETIM_ISP_SET(piix_isp_pio[mode], channel) |
		    PIIX_SIDETIM_RTC_SET(piix_rtc_pio[mode], channel);
}

void
piix_channel_map(sc, pa, cp)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
{
	bus_size_t cmdsize, ctlsize;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	u_int32_t idetim = pci_conf_read(pa->pa_pc, pa->pa_tag, PIIX_IDETIM);

	if ((PIIX_IDETIM_READ(idetim, wdc_cp->channel) & PIIX_IDETIM_IDE) == 0) {
		printf("%s: %s channel ignored (disabled)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return;
	}

	/* PIIX are compat-only pciide devices */
	pciide_mapchan(sc, pa, cp, 0, &cmdsize, &ctlsize);
	if (cp->hw_ok == 0)
		return;
	if (pciiide_chan_candisable(sc, pa, cp, cmdsize, ctlsize)) {
		idetim = PIIX_IDETIM_CLEAR(idetim, PIIX_IDETIM_IDE,
					   wdc_cp->channel);
		pci_conf_write(pa->pa_pc, pa->pa_tag, PIIX_IDETIM, idetim);
	}
	pciide_map_compat_intr(sc, pa, cp, wdc_cp->channel, 0);
}

void
apollo_setup_cap(sc)
	struct pciide_softc *sc;
{
	if (sc->sc_pp->ide_product == PCI_PRODUCT_VIATECH_VT82C586A_IDE)
		sc->sc_wdcdev.cap |= WDC_CAPABILITY_UDMA;
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE |
	    WDC_CAPABILITY_DMA;
	sc->sc_wdcdev.pio_mode = 4;
	sc->sc_wdcdev.dma_mode = 2;
	sc->sc_wdcdev.udma_mode = 2;

}
void
apollo_setup_chip(sc, pc, tag)
	struct pciide_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	u_int32_t udmatim_reg, datatim_reg;
	u_int8_t idedma_ctl;
	int mode;
	int channel, drive;
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;

	WDCDEBUG_PRINT(("apollo_setup_chip: old APO_IDECONF=0x%x, "
	    "APO_CTLMISC=0x%x, APO_DATATIM=0x%x, APO_UDMA=0x%x\n",
	    pci_conf_read(pc, tag, APO_IDECONF),
	    pci_conf_read(pc, tag, APO_CTLMISC),
	    pci_conf_read(pc, tag, APO_DATATIM),
	    pci_conf_read(pc, tag, APO_UDMA)),
	    DEBUG_PROBE);

	datatim_reg = 0;
	udmatim_reg = 0;
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		chp = &sc->pciide_channels[channel].wdc_channel;
		idedma_ctl = 0;
		for (drive = 0; drive < 2; drive++) {
			drvp = &chp->ch_drive[drive];
			/* If no drive, skip */
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			/* add timing values, setup DMA if needed */
			if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
			    (drvp->drive_flags & DRIVE_UDMA) == 0) ||
			    sc->sc_dma_ok == 0) {
				drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
				mode = drvp->PIO_mode;
				goto pio;
			}
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
				mode = drvp->PIO_mode;
				goto pio;
			}
			if ((chp->wdc->cap & WDC_CAPABILITY_UDMA) &&
			    (drvp->drive_flags & DRIVE_UDMA)) {
				/* use Ultra/DMA */
				drvp->drive_flags &= ~DRIVE_DMA;
				udmatim_reg |= APO_UDMA_EN(channel, drive) |
				    APO_UDMA_EN_MTH(channel, drive) |
				    APO_UDMA_TIME(channel, drive,
					apollo_udma_tim[drvp->UDMA_mode]);
				/* can use PIO timings, MW DMA unused */
				mode = drvp->PIO_mode;
			} else {
				/* use Multiword DMA */
				drvp->drive_flags &= ~DRIVE_UDMA;
				/* mode = min(pio, dma+2) */
				if (drvp->PIO_mode <= (drvp->DMA_mode +2))
					mode = drvp->PIO_mode;
				else
					mode = drvp->DMA_mode;
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

pio:			/* setup PIO mode */
			datatim_reg |=
			    APO_DATATIM_PULSE(channel, drive,
				apollo_pio_set[mode]) |
			    APO_DATATIM_RECOV(channel, drive,
				apollo_pio_rec[mode]);
			drvp->PIO_mode = mode;
			drvp->DMA_mode = mode - 2;
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * channel),
			    idedma_ctl);
		}
	}
	pciide_print_modes(sc);
	WDCDEBUG_PRINT(("apollo_setup_chip: APO_DATATIM=0x%x, APO_UDMA=0x%x\n",
	    datatim_reg, udmatim_reg), DEBUG_PROBE);
	pci_conf_write(pc, tag, APO_DATATIM, datatim_reg);
	pci_conf_write(pc, tag, APO_UDMA, udmatim_reg);
}

void
apollo_channel_map(sc, pa, cp)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
{
	bus_size_t cmdsize, ctlsize;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	u_int32_t ideconf = pci_conf_read(pa->pa_pc, pa->pa_tag, APO_IDECONF);
	int interface =
	    PCI_INTERFACE(pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG));

	if ((ideconf & APO_IDECONF_EN(wdc_cp->channel)) == 0) {
		printf("%s: %s channel ignored (disabled)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return;
	}

	pciide_mapchan(sc, pa, cp, interface, &cmdsize, &ctlsize);
	if (cp->hw_ok == 0)
		return;
	if (pciiide_chan_candisable(sc, pa, cp, cmdsize, ctlsize)) {
		ideconf &= ~APO_IDECONF_EN(wdc_cp->channel);
		pci_conf_write(pa->pa_pc, pa->pa_tag, APO_IDECONF, ideconf);
	}
	pciide_map_compat_intr(sc, pa, cp, wdc_cp->channel, interface);
}

void
cmd_channel_map(sc, pa, cp)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
{
	bus_size_t cmdsize, ctlsize;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	u_int8_t ctrl = pciide_pci_read(pa->pa_pc, pa->pa_tag, CMD_CTRL);
	int interface =
	    PCI_INTERFACE(pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG));

	/*
	 * with a CMD PCI64x, if we get here, the first channel is enabled:
	 * there's no way to disable the first channel without disabling
	 * the whole device
	 */
	if (wdc_cp->channel != 0 && (ctrl & CMD_CTRL_2PORT) == 0) {
		printf("%s: %s channel ignored (disabled)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return;
	}

	pciide_mapchan(sc, pa, cp, interface, &cmdsize, &ctlsize);
	if (cp->hw_ok == 0)
		return;
	if (wdc_cp->channel == 1) {
		if (pciiide_chan_candisable(sc, pa, cp, cmdsize, ctlsize)) {
			ctrl &= ~CMD_CTRL_2PORT;
			pciide_pci_write(pa->pa_pc, pa->pa_tag,
			    CMD_CTRL, ctrl);
		}
	}
	pciide_map_compat_intr(sc, pa, cp, wdc_cp->channel, interface);
}

void
cmd0643_6_setup_cap(sc)
	struct pciide_softc *sc;
{
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE |
	    WDC_CAPABILITY_DMA;
	sc->sc_wdcdev.pio_mode = 4;
	sc->sc_wdcdev.dma_mode = 2;
}

void
cmd0643_6_setup_chip(sc, pc, tag)
	struct pciide_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;
	int channel, drive;
	u_int8_t tim;
	u_int32_t idedma_ctl;

	WDCDEBUG_PRINT(("cmd0643_6_setup_chip: old timings reg 0x%x 0x%x\n",
		pci_conf_read(pc, tag, 0x54), pci_conf_read(pc, tag, 0x58)),
		DEBUG_PROBE);
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		chp = &sc->pciide_channels[channel].wdc_channel;
		idedma_ctl = 0;
		for (drive = 0; drive < 2; drive++) {
			drvp = &chp->ch_drive[drive];
			/* If no drive, skip */
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			/* add timing values, setup DMA if needed */
			tim = cmd0643_6_data_tim_pio[drvp->PIO_mode];
			if ((drvp->drive_flags & DRIVE_DMA) == 0 ||
			    sc->sc_dma_ok == 0) {
				drvp->drive_flags &= ~DRIVE_DMA;
				goto end;
			}
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				drvp->drive_flags &= ~DRIVE_DMA;
				goto end;
			}
			/*
			 * use Multiword DMA.
			 * Timings will be used for both PIO and DMA, so adjust
			 * DMA mode if needed
			 */
			if (drvp->PIO_mode >= 3 &&
			    (drvp->DMA_mode + 2) > drvp->PIO_mode) {
				drvp->DMA_mode = drvp->PIO_mode - 2;
			}
			tim = cmd0643_6_data_tim_dma[drvp->DMA_mode];
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);

end:			pciide_pci_write(pc, tag,
			    CMD_DATA_TIM(channel, drive), tim);
		}
		if (idedma_ctl != 0) {
			/* Add software bits in status register */
			bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
			    IDEDMA_CTL + (IDEDMA_SCH_OFFSET * channel),
			    idedma_ctl);
		}
	}
	/* print modes */
	pciide_print_modes(sc);
	/* configure for DMA read multiple */
	pciide_pci_write(pc, tag, CMD_DMA_MODE, CMD_DMA_MULTIPLE);
	WDCDEBUG_PRINT(("cmd0643_6_setup_chip: timings reg now 0x%x 0x%x\n",
	    pci_conf_read(pc, tag, 0x54), pci_conf_read(pc, tag, 0x58)),
	    DEBUG_PROBE);
}

void
cy693_setup_cap(sc)
	struct pciide_softc *sc;
{
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE |
	    WDC_CAPABILITY_DMA;
	sc->sc_wdcdev.pio_mode = 4;
	sc->sc_wdcdev.dma_mode = 2;
}

void
cy693_setup_chip(sc, pc, tag)
	struct pciide_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;
	int drive;
	u_int32_t cy_cmd_ctrl;
	u_int32_t idedma_ctl;

	WDCDEBUG_PRINT(("cy693_setup_chip: old timings reg 0x%x\n",
		pci_conf_read(pc, tag, CY_CMD_CTRL)), DEBUG_PROBE);
	cy_cmd_ctrl = idedma_ctl = 0;
	chp = &sc->pciide_channels[0].wdc_channel; /* Only one channel */
	for (drive = 0; drive < 2; drive++) {
		drvp = &chp->ch_drive[drive];
		/* If no drive, skip */
		if ((drvp->drive_flags & DRIVE) == 0)
			continue;
		/* add timing values, setup DMA if needed */
		if ((drvp->drive_flags & DRIVE_DMA) == 0 ||
		    sc->sc_dma_ok == 0) {
			drvp->drive_flags &= ~DRIVE_DMA;
			goto pio;
		}
		if (pciide_dma_table_setup(sc, 0, drive) != 0) {
			/* Abort DMA setup */
			drvp->drive_flags &= ~DRIVE_DMA;
			goto pio;
		}
		idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
		/*
		 * use Multiword DMA
		 * Timings will be used for both PIO and DMA, so adjust
		 * DMA mode if needed
		 */
		if (drvp->PIO_mode > (drvp->DMA_mode + 2))
			drvp->PIO_mode = drvp->DMA_mode + 2;
		if (drvp->DMA_mode == 0)
			drvp->PIO_mode = 0;
pio:		cy_cmd_ctrl |= (cy_pio_pulse[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_PULSE_OFF(drive));
		cy_cmd_ctrl |= (cy_pio_rec[drvp->PIO_mode] <<
		    CY_CMD_CTRL_IOW_REC_OFF(drive));
	}
	WDCDEBUG_PRINT(("cy693_setup_chip: new timings reg 0x%x\n",
	    cy_cmd_ctrl), DEBUG_PROBE);
	pci_conf_write(pc, tag, CY_CMD_CTRL, cy_cmd_ctrl);
	pciide_print_modes(sc);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL, idedma_ctl);
	}
}

void
cy693_channel_map(sc, pa, cp)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
{
	bus_size_t cmdsize, ctlsize;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	int interface =
	    PCI_INTERFACE(pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG));
	int compatchan;

#ifdef DIAGNOSTIC
	if (wdc_cp->channel != 0)
		panic("cy693_channel_map: channel %d", wdc_cp->channel);
#endif

	/*
	 * this chip has 2 PCI IDE functions, one for primary and one for
	 * secondary. So we need to call pciide_mapregs_compat() with
	 * the real channel
	 */
	if (pa->pa_function == 1) {
		compatchan = 0;
	} else if (pa->pa_function == 2) {
		compatchan = 1;
	} else {
		printf("%s: unexpected PCI function %d\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, pa->pa_function);
		cp->hw_ok = 0;
		return;
	}
		
	/* Only one channel for this chip; if we are here it's enabled */
	if (interface & PCIIDE_INTERFACE_PCI(0))
		cp->hw_ok = pciide_mapregs_native(sc, pa, cp,
		    &cmdsize, &ctlsize);
	else
		cp->hw_ok = pciide_mapregs_compat(sc, pa, cp, compatchan,
		    &cmdsize, &ctlsize);
	if (cp->hw_ok == 0)
		return;
	wdc_cp->data32iot = wdc_cp->cmd_iot;
	wdc_cp->data32ioh = wdc_cp->cmd_ioh;
	wdcattach(wdc_cp);
	if (pciiide_chan_candisable(sc, pa, cp, cmdsize, ctlsize)) {
		pci_conf_write(pa->pa_pc, pa->pa_tag,
		    PCI_COMMAND_STATUS_REG, 0);
	}
	pciide_map_compat_intr(sc, pa, cp, compatchan, interface);
}

void
sis_setup_cap(sc)
	struct pciide_softc *sc;
{
	sc->sc_wdcdev.cap |= WDC_CAPABILITY_DATA32 | WDC_CAPABILITY_MODE |
	    WDC_CAPABILITY_DMA | WDC_CAPABILITY_UDMA;
	sc->sc_wdcdev.pio_mode = 4;
	sc->sc_wdcdev.dma_mode = 2;
	sc->sc_wdcdev.udma_mode = 2;
}

void
sis_setup_chip(sc, pc, tag)
	struct pciide_softc *sc;
	pci_chipset_tag_t pc;
	pcitag_t tag;
{
	struct channel_softc *chp;
	struct ata_drive_datas *drvp;
	int channel, drive;
	u_int32_t sis_tim;
	u_int32_t idedma_ctl;

	idedma_ctl = 0;
	for (channel = 0; channel < sc->sc_wdcdev.nchannels; channel++) {
		chp = &sc->pciide_channels[channel].wdc_channel;
		WDCDEBUG_PRINT(("sis_setup_chip: old timings reg for "
		    "channel %d 0x%x\n", channel, 
		    pci_conf_read(pc, tag, SIS_TIM(channel))), DEBUG_PROBE);
		sis_tim = 0;
		for (drive = 0; drive < 2; drive++) {
			drvp = &chp->ch_drive[drive];
			/* If no drive, skip */
			if ((drvp->drive_flags & DRIVE) == 0)
				continue;
			/* add timing values, setup DMA if needed */
			if (((drvp->drive_flags & DRIVE_DMA) == 0 &&
			    (drvp->drive_flags & DRIVE_UDMA) == 0) ||
			    sc->sc_dma_ok == 0) {
				drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
				goto pio;
			}
			if (pciide_dma_table_setup(sc, channel, drive) != 0) {
				/* Abort DMA setup */
				drvp->drive_flags &= ~(DRIVE_DMA | DRIVE_UDMA);
				goto pio;
			}
			if (drvp->drive_flags & DRIVE_UDMA) {
				/* use Ultra/DMA */
				drvp->drive_flags &= ~DRIVE_DMA;
				sis_tim |= sis_udma_tim[drvp->UDMA_mode] << 
				    SIS_TIM_UDMA_TIME_OFF(drive);
				sis_tim |= SIS_TIM_UDMA_EN(drive);
			} else {
				/*
				 * use Multiword DMA
				 * Timings will be used for both PIO and DMA,
				 * so adjust DMA mode if needed
				 */
				if (drvp->PIO_mode > (drvp->DMA_mode + 2))
					drvp->PIO_mode = drvp->DMA_mode + 2;
				if (drvp->DMA_mode == 0)
					drvp->PIO_mode = 0;
			}
			idedma_ctl |= IDEDMA_CTL_DRV_DMA(drive);
pio:			sis_tim |= sis_pio_act[drvp->PIO_mode] <<
			    SIS_TIM_ACT_OFF(drive);
			sis_tim |= sis_pio_rec[drvp->PIO_mode] <<
			    SIS_TIM_REC_OFF(drive);
		}
		WDCDEBUG_PRINT(("sis_setup_chip: new timings reg for "
		    "channel %d 0x%x\n", channel, sis_tim), DEBUG_PROBE);
		pci_conf_write(pc, tag, SIS_TIM(channel), sis_tim);
	}
	pciide_print_modes(sc);
	pciide_pci_write(pc, tag, SIS_MISC,
	    pciide_pci_read(pc, tag, SIS_MISC) |
	    SIS_MISC_TIM_SEL | SIS_MISC_FIFO_SIZE);
	if (idedma_ctl != 0) {
		/* Add software bits in status register */
		bus_space_write_1(sc->sc_dma_iot, sc->sc_dma_ioh,
		    IDEDMA_CTL, idedma_ctl);
	}
}

void
sis_channel_map(sc, pa, cp)
	struct pciide_softc *sc;
	struct pci_attach_args *pa;
	struct pciide_channel *cp;
{
	bus_size_t cmdsize, ctlsize;
	struct channel_softc *wdc_cp = &cp->wdc_channel;
	u_int8_t sis_ctr0 = pciide_pci_read(pa->pa_pc, pa->pa_tag, SIS_CTRL0);
	int interface =
	    PCI_INTERFACE(pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG));

	if ((wdc_cp->channel == 0 && (sis_ctr0 & SIS_CTRL0_CHAN0_EN) == 0) ||
	    (wdc_cp->channel == 1 && (sis_ctr0 & SIS_CTRL0_CHAN1_EN) == 0)) {
		printf("%s: %s channel ignored (disabled)\n",
		    sc->sc_wdcdev.sc_dev.dv_xname, cp->name);
		return;
	}

	pciide_mapchan(sc, pa, cp, interface, &cmdsize, &ctlsize);
	if (cp->hw_ok == 0)
		return;
	if (pciiide_chan_candisable(sc, pa, cp, cmdsize, ctlsize)) {
		if (wdc_cp->channel == 0)
			sis_ctr0 &= ~SIS_CTRL0_CHAN0_EN;
		else
			sis_ctr0 &= ~SIS_CTRL0_CHAN1_EN;
		pciide_pci_write(pa->pa_pc, pa->pa_tag, SIS_CTRL0, sis_ctr0);
	}
	pciide_map_compat_intr(sc, pa, cp, wdc_cp->channel, interface);
}
