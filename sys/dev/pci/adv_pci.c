/*	$NetBSD: adv_pci.c,v 1.3 1998/08/29 13:54:50 dante Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc. All rights reserved.
 * 
 * Author: Baldassare Dante Profeta <dante@mclink.it>
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
 *    This product includes software developed by the NetBSD
 *    Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Device probe and attach routines for the following
 * Advanced Systems Inc. SCSI controllers:
 *
 *    Connectivity Products:
 *      ABP510/5150 - Bus-Master ISA (240 CDB) (Footnote 1)
 *      ABP5140 - Bus-Master ISA PnP (16 CDB) (Footnote 1, 3)
 *      ABP5142 - Bus-Master ISA PnP with floppy (16 CDB) (Footnote 4)
 *      ABP920 - Bus-Master PCI (16 CDB)
 *      ABP930 - Bus-Master PCI (16 CDB) (Footnote 5)
 *      ABP930U - Bus-Master PCI Ultra (16 CDB)
 *      ABP930UA - Bus-Master PCI Ultra (16 CDB)
 *      ABP960 - Bus-Master PCI MAC/PC (16 CDB) (Footnote 2)
 *      ABP960U - Bus-Master PCI MAC/PC Ultra (16 CDB) (Footnote 2)
 *
 *   Single Channel Products:
 *      ABP542 - Bus-Master ISA with floppy (240 CDB)
 *      ABP742 - Bus-Master EISA (240 CDB)
 *      ABP842 - Bus-Master VL (240 CDB)
 *      ABP940 - Bus-Master PCI (240 CDB)
 *      ABP940U - Bus-Master PCI Ultra (240 CDB)
 *      ABP970 - Bus-Master PCI MAC/PC (240 CDB)
 *      ABP970U - Bus-Master PCI MAC/PC Ultra (240 CDB)
 *      ABP940UW - Bus-Master PCI Ultra-Wide (240 CDB)
 *
 *   Multi Channel Products:
 *      ABP752 - Dual Channel Bus-Master EISA (240 CDB Per Channel)
 *      ABP852 - Dual Channel Bus-Master VL (240 CDB Per Channel)
 *      ABP950 - Dual Channel Bus-Master PCI (240 CDB Per Channel)
 *      ABP980 - Four Channel Bus-Master PCI (240 CDB Per Channel)
 *      ABP980U - Four Channel Bus-Master PCI Ultra (240 CDB Per Channel)
 *
 *   Footnotes:
 *     1. This board has been shipped by HP with the 4020i CD-R drive.
 *        The board has no BIOS so it cannot control a boot device, but
 *        it can control any secondary SCSI device.
 *     2. This board has been sold by Iomega as a Jaz Jet PCI adapter.
 *     3. This board has been sold by SIIG as the i540 SpeedMaster.
 *     4. This board has been sold by SIIG as the i542 SpeedMaster.
 *     5. This board has been sold by SIIG as the Fast SCSI Pro PCI.
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/adv.h>
#include <dev/ic/advlib.h>

/******************************************************************************/

#define PCI_BASEADR_IO        0x10

/******************************************************************************/

#ifdef	__BROKEN_INDIRECT_CONFIG
int adv_pci_probe __P((struct device *, void *, void *));
#else
int adv_pci_probe __P((struct device *, struct cfdata *, void *));
#endif
void adv_pci_attach __P((struct device *, struct device *, void *));

struct cfattach adv_pci_ca =
{
	sizeof(ASC_SOFTC), adv_pci_probe, adv_pci_attach
};

/******************************************************************************/
/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
#ifdef	__BROKEN_INDIRECT_CONFIG
int 
adv_pci_probe(struct device * parent, void *match, void *aux)
#else
int 
adv_pci_probe(struct device * parent, struct cfdata * match, void *aux)
#endif
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADVSYS)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADVSYS_1200A:
		case PCI_PRODUCT_ADVSYS_1200B:
		case PCI_PRODUCT_ADVSYS_ULTRA:
			return (1);
		case PCI_PRODUCT_ADVSYS_2300:
			return (0);
		}

	return 0;
}


void 
adv_pci_attach(struct device * parent, struct device * self, void *aux)
{
	struct pci_attach_args *pa = aux;
	ASC_SOFTC      *sc = (void *) self;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pci_intr_handle_t ih;
	pci_chipset_tag_t pc = pa->pa_pc;
	u_int32_t       command;
	const char     *intrstr;


	sc->sc_flags = 0x0;
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_ADVSYS)
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_ADVSYS_1200A:
			printf(": AdvanSys ASC1200A SCSI\n");
			break;

		case PCI_PRODUCT_ADVSYS_1200B:
			printf(": AdvanSys ASC1200B SCSI\n");
			break;

		case PCI_PRODUCT_ADVSYS_ULTRA:
			switch (PCI_REVISION(pa->pa_class)) {
			case ASC_PCI_REVISION_3050:
				printf(": AdvanSys ASC3150 Ultra SCSI\n");
				break;

			case ASC_PCI_REVISION_3150:
				printf(": AdvanSys ASC3050 Ultra SCSI\n");
				break;
			}
			break;

		case PCI_PRODUCT_ADVSYS_2300:
			sc->sc_flags |= ASC_WIDE_BOARD;
			printf("adv_pci_attach: Wide boards are not"
					" supported yet");
			break;

		default:
			printf(": unknown model!\n");
			return;
		}


	/*
	 * Make sure IO/MEM/MASTER are enabled
	 */
	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	if ((command & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
			PCI_COMMAND_MASTER_ENABLE)) !=
	    (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	     PCI_COMMAND_MASTER_ENABLE)) {
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		 command | (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
			    PCI_COMMAND_MASTER_ENABLE));
	}
	/*
	 * Latency timer settings.
	 */
	{
		u_int32_t       bhlcr;

		bhlcr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);

		if (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_1200A ||
		    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_1200B) {
			bhlcr &= 0xFFFF00FFul;
			pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, bhlcr);
		} else if ((PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_ADVSYS_ULTRA) &&
			   (PCI_LATTIMER(bhlcr) < 0x20)) {
			bhlcr &= 0xFFFF00FFul;
			bhlcr |= 0x00002000ul;
			pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, bhlcr);
		}
	}


	/*
	 * Map Device Registers for I/O
	 */
	if (pci_mapreg_map(pa, PCI_BASEADR_IO, PCI_MAPREG_TYPE_IO, 0,
			&iot, &ioh, NULL, NULL)) {
		printf("%s: unable to map device registers\n",
		       sc->sc_dev.dv_xname);
		return;
	}
	sc->sc_iot = iot;
	sc->sc_ioh = ioh;
	sc->sc_dmat = pa->pa_dmat;
	sc->pci_device_id = pa->pa_id;
	sc->bus_type = ASC_IS_PCI;

	/*
	 * Initialize the board
	 */
	if (adv_init(sc))
		panic("adv_pci_attach: adv_init failed");

	/*
	 * Map Interrupt line
	 */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);

	/*
	 * Establish Interrupt handler
	 */
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_BIO, adv_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt", sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/*
	 * Attach all the sub-devices we can find
	 */
	adv_attach(sc);
}
/******************************************************************************/
