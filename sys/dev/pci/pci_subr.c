/*	$NetBSD: pci_subr.c,v 1.26 1998/05/18 17:25:17 cgd Exp $	*/

/*
 * Copyright (c) 1997 Zubin D. Dittia.  All rights reserved.
 * Copyright (c) 1995, 1996, 1998
 *	Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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

/*
 * PCI autoconfiguration support functions.
 */

#include "opt_pciverbose.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#ifdef PCIVERBOSE
#include <dev/pci/pcidevs.h>
#endif

static void pci_conf_print_common __P((pci_chipset_tag_t, pcitag_t,
    const pcireg_t *regs));
static void pci_conf_print_type1 __P((pci_chipset_tag_t, pcitag_t,
    const pcireg_t *regs));

/*
 * Descriptions of known PCI classes and subclasses.
 *
 * Subclasses are described in the same way as classes, but have a
 * NULL subclass pointer.
 */
struct pci_class {
	char		*name;
	int		val;		/* as wide as pci_{,sub}class_t */
	struct pci_class *subclasses;
};

struct pci_class pci_subclass_prehistoric[] = {
	{ "miscellaneous",	PCI_SUBCLASS_PREHISTORIC_MISC,		},
	{ "VGA",		PCI_SUBCLASS_PREHISTORIC_VGA,		},
	{ 0 }
};

struct pci_class pci_subclass_mass_storage[] = {
	{ "SCSI",		PCI_SUBCLASS_MASS_STORAGE_SCSI,		},
	{ "IDE",		PCI_SUBCLASS_MASS_STORAGE_IDE,		},
	{ "floppy",		PCI_SUBCLASS_MASS_STORAGE_FLOPPY,	},
	{ "IPI",		PCI_SUBCLASS_MASS_STORAGE_IPI,		},
	{ "RAID",		PCI_SUBCLASS_MASS_STORAGE_RAID,		},
	{ "miscellaneous",	PCI_SUBCLASS_MASS_STORAGE_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_network[] = {
	{ "ethernet",		PCI_SUBCLASS_NETWORK_ETHERNET,		},
	{ "token ring",		PCI_SUBCLASS_NETWORK_TOKENRING,		},
	{ "FDDI",		PCI_SUBCLASS_NETWORK_FDDI,		},
	{ "ATM",		PCI_SUBCLASS_NETWORK_ATM,		},
	{ "miscellaneous",	PCI_SUBCLASS_NETWORK_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_display[] = {
	{ "VGA",		PCI_SUBCLASS_DISPLAY_VGA,		},
	{ "XGA",		PCI_SUBCLASS_DISPLAY_XGA,		},
	{ "miscellaneous",	PCI_SUBCLASS_DISPLAY_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_multimedia[] = {
	{ "video",		PCI_SUBCLASS_MULTIMEDIA_VIDEO,		},
	{ "audio",		PCI_SUBCLASS_MULTIMEDIA_AUDIO,		},
	{ "miscellaneous",	PCI_SUBCLASS_MULTIMEDIA_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_memory[] = {
	{ "RAM",		PCI_SUBCLASS_MEMORY_RAM,		},
	{ "flash",		PCI_SUBCLASS_MEMORY_FLASH,		},
	{ "miscellaneous",	PCI_SUBCLASS_MEMORY_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_bridge[] = {
	{ "host",		PCI_SUBCLASS_BRIDGE_HOST,		},
	{ "ISA",		PCI_SUBCLASS_BRIDGE_ISA,		},
	{ "EISA",		PCI_SUBCLASS_BRIDGE_EISA,		},
	{ "MicroChannel",	PCI_SUBCLASS_BRIDGE_MC,			},
	{ "PCI",		PCI_SUBCLASS_BRIDGE_PCI,		},
	{ "PCMCIA",		PCI_SUBCLASS_BRIDGE_PCMCIA,		},
	{ "NuBus",		PCI_SUBCLASS_BRIDGE_NUBUS,		},
	{ "CardBus",		PCI_SUBCLASS_BRIDGE_CARDBUS,		},
	{ "miscellaneous",	PCI_SUBCLASS_BRIDGE_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_communications[] = {
	{ "serial",		PCI_SUBCLASS_COMMUNICATIONS_SERIAL,	},
	{ "parallel",		PCI_SUBCLASS_COMMUNICATIONS_PARALLEL,	},
	{ "miscellaneous",	PCI_SUBCLASS_COMMUNICATIONS_MISC,	},
	{ 0 },
};

struct pci_class pci_subclass_system[] = {
	{ "8259 PIC",		PCI_SUBCLASS_SYSTEM_PIC,		},
	{ "8237 DMA",		PCI_SUBCLASS_SYSTEM_DMA,		},
	{ "8254 timer",		PCI_SUBCLASS_SYSTEM_TIMER,		},
	{ "RTC",		PCI_SUBCLASS_SYSTEM_RTC,		},
	{ "miscellaneous",	PCI_SUBCLASS_SYSTEM_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_input[] = {
	{ "keyboard",		PCI_SUBCLASS_INPUT_KEYBOARD,		},
	{ "digitizer",		PCI_SUBCLASS_INPUT_DIGITIZER,		},
	{ "mouse",		PCI_SUBCLASS_INPUT_MOUSE,		},
	{ "miscellaneous",	PCI_SUBCLASS_INPUT_MISC,		},
	{ 0 },
};

struct pci_class pci_subclass_dock[] = {
	{ "generic",		PCI_SUBCLASS_DOCK_GENERIC,		},
	{ "miscellaneous",	PCI_SUBCLASS_DOCK_MISC,			},
	{ 0 },
};

struct pci_class pci_subclass_processor[] = {
	{ "386",		PCI_SUBCLASS_PROCESSOR_386,		},
	{ "486",		PCI_SUBCLASS_PROCESSOR_486,		},
	{ "Pentium",		PCI_SUBCLASS_PROCESSOR_PENTIUM,		},
	{ "Alpha",		PCI_SUBCLASS_PROCESSOR_ALPHA,		},
	{ "PowerPC",		PCI_SUBCLASS_PROCESSOR_POWERPC,		},
	{ "Co-processor",	PCI_SUBCLASS_PROCESSOR_COPROC,		},
	{ 0 },
};

struct pci_class pci_subclass_serialbus[] = {
	{ "Firewire",		PCI_SUBCLASS_SERIALBUS_FIREWIRE,	},
	{ "ACCESS.bus",		PCI_SUBCLASS_SERIALBUS_ACCESS,		},
	{ "SSA",		PCI_SUBCLASS_SERIALBUS_SSA,		},
	{ "USB",		PCI_SUBCLASS_SERIALBUS_USB,		},
	{ "Fiber Channel",	PCI_SUBCLASS_SERIALBUS_FIBER,		},
	{ 0 },
};

struct pci_class pci_class[] = {
	{ "prehistoric",	PCI_CLASS_PREHISTORIC,
	    pci_subclass_prehistoric,				},
	{ "mass storage",	PCI_CLASS_MASS_STORAGE,
	    pci_subclass_mass_storage,				},
	{ "network",		PCI_CLASS_NETWORK,
	    pci_subclass_network,				},
	{ "display",		PCI_CLASS_DISPLAY,
	    pci_subclass_display,				},
	{ "multimedia",		PCI_CLASS_MULTIMEDIA,
	    pci_subclass_multimedia,				},
	{ "memory",		PCI_CLASS_MEMORY,
	    pci_subclass_memory,				},
	{ "bridge",		PCI_CLASS_BRIDGE,
	    pci_subclass_bridge,				},
	{ "communications",	PCI_CLASS_COMMUNICATIONS,
	    pci_subclass_communications,			},
	{ "system",		PCI_CLASS_SYSTEM,
	    pci_subclass_system,				},
	{ "input",		PCI_CLASS_INPUT,
	    pci_subclass_input,					},
	{ "dock",		PCI_CLASS_DOCK,
	    pci_subclass_dock,					},
	{ "processor",		PCI_CLASS_PROCESSOR,
	    pci_subclass_processor,				},
	{ "serial bus",		PCI_CLASS_SERIALBUS,
	    pci_subclass_serialbus,				},
	{ "undefined",		PCI_CLASS_UNDEFINED,
	    0,							},
	{ 0 },
};

#ifdef PCIVERBOSE
/*
 * Descriptions of of known vendors and devices ("products").
 */
struct pci_knowndev {
	pci_vendor_id_t		vendor;
	pci_product_id_t	product;
	int			flags;
	char			*vendorname, *productname;
};
#define	PCI_KNOWNDEV_NOPROD	0x01		/* match on vendor only */

#include <dev/pci/pcidevs_data.h>
#endif /* PCIVERBOSE */

void
pci_devinfo(id_reg, class_reg, showclass, cp)
	pcireg_t id_reg, class_reg;
	int showclass;
	char *cp;
{
	pci_vendor_id_t vendor;
	pci_product_id_t product;
	pci_class_t class;
	pci_subclass_t subclass;
	pci_interface_t interface;
	pci_revision_t revision;
	char *vendor_namep, *product_namep;
	struct pci_class *classp, *subclassp;
#ifdef PCIVERBOSE
	struct pci_knowndev *kdp;
	const char *unmatched = "unknown ";
#else
	const char *unmatched = "";
#endif

	vendor = PCI_VENDOR(id_reg);
	product = PCI_PRODUCT(id_reg);

	class = PCI_CLASS(class_reg);
	subclass = PCI_SUBCLASS(class_reg);
	interface = PCI_INTERFACE(class_reg);
	revision = PCI_REVISION(class_reg);

#ifdef PCIVERBOSE
	kdp = pci_knowndevs;
        while (kdp->vendorname != NULL) {	/* all have vendor name */
                if (kdp->vendor == vendor && (kdp->product == product ||
		    (kdp->flags & PCI_KNOWNDEV_NOPROD) != 0))
                        break;
		kdp++;
	}
        if (kdp->vendorname == NULL)
		vendor_namep = product_namep = NULL;
	else {
		vendor_namep = kdp->vendorname;
		product_namep = (kdp->flags & PCI_KNOWNDEV_NOPROD) == 0 ?
		    kdp->productname : NULL;
        }
#else /* PCIVERBOSE */
	vendor_namep = product_namep = NULL;
#endif /* PCIVERBOSE */

	classp = pci_class;
	while (classp->name != NULL) {
		if (class == classp->val)
			break;
		classp++;
	}

	subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
	while (subclassp && subclassp->name != NULL) {
		if (subclass == subclassp->val)
			break;
		subclassp++;
	}

	if (vendor_namep == NULL)
		cp += sprintf(cp, "%svendor 0x%04x product 0x%04x",
		    unmatched, vendor, product);
	else if (product_namep != NULL)
		cp += sprintf(cp, "%s %s", vendor_namep, product_namep);
	else
		cp += sprintf(cp, "%s product 0x%04x",
		    vendor_namep, product);
	if (showclass) {
		cp += sprintf(cp, " (");
		if (classp->name == NULL)
			cp += sprintf(cp, "class 0x%02x, subclass 0x%02x",
			    class, subclass);
		else {
			if (subclassp == NULL || subclassp->name == NULL)
				cp += sprintf(cp,
				    "%s subclass 0x%02x",
				    classp->name, subclass);
			else
				cp += sprintf(cp, "%s %s",
				    subclassp->name, classp->name);
		}
		if (interface != 0)
			cp += sprintf(cp, ", interface 0x%02x", interface);
		if (revision != 0)
			cp += sprintf(cp, ", revision 0x%02x", revision);
		cp += sprintf(cp, ")");
	}
}

/*
 * Print out most of the PCI configuration registers.  Typically used
 * in a device attach routine like this:
 *
 *	#ifdef MYDEV_DEBUG
 *		printf("%s: ", sc->sc_dev.dv_xname);
 *		pci_conf_print(pa->pa_pc, pa->pa_tag);
 *	#endif
 */

#define	i2o(i)	((i) * 4)
#define	o2i(o)	((o) / 4)

static void
pci_conf_print_common(pc, tag, regs)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	const pcireg_t *regs;
{
#ifdef PCIVERBOSE
	struct pci_knowndev *kdp;
#endif
	struct pci_class *classp, *subclassp;
	pcireg_t rval;

	rval = regs[o2i(PCI_ID_REG)];
#ifndef PCIVERBOSE
	printf("    Vendor ID: 0x%04x\n", PCI_VENDOR(rval));
	printf("    Device ID: 0x%04x\n", PCI_PRODUCT(rval));
#else
	for (kdp = pci_knowndevs; kdp->vendorname != NULL; kdp++) {
		if (kdp->vendor == PCI_VENDOR(rval) &&
		    (kdp->product == PCI_PRODUCT(rval) ||
		    (kdp->flags & PCI_KNOWNDEV_NOPROD) != 0)) {
			break;
		}
	}
	if (kdp->vendorname != NULL)
		printf("    Vendor Name: %s (0x%04x)\n", kdp->vendorname,
		    PCI_VENDOR(rval));
	else
		printf("    Vendor ID: 0x%04x\n", PCI_VENDOR(rval));
	if (kdp->productname != NULL && (kdp->flags & PCI_KNOWNDEV_NOPROD) == 0)
		printf("    Device Name: %s (0x%04x)\n", kdp->productname,
		    PCI_PRODUCT(rval));
	else
		printf("    Device ID: 0x%04x\n", PCI_PRODUCT(rval));
#endif /* PCIVERBOSE */

#define	onoff(str, bit)							\
	printf("      %s: %s\n", (str), (rval & (bit)) ? "on" : "off");

	rval = regs[o2i(PCI_COMMAND_STATUS_REG)];

	printf("    Command register: 0x%04x\n", rval & 0xffff);
	onoff("I/O space accesses", PCI_COMMAND_IO_ENABLE);
	onoff("Memory space accesses", PCI_COMMAND_MEM_ENABLE);
	onoff("Bus mastering", PCI_COMMAND_MASTER_ENABLE);
	onoff("Special cycles", PCI_COMMAND_SPECIAL_ENABLE);
	onoff("MWI transactions", PCI_COMMAND_INVALIDATE_ENABLE);
	onoff("Palette snooping", PCI_COMMAND_PALETTE_ENABLE);
	onoff("Parity error checking", PCI_COMMAND_PARITY_ENABLE);
	onoff("Address/data stepping", PCI_COMMAND_STEPPING_ENABLE);
	onoff("System error (SERR)", PCI_COMMAND_SERR_ENABLE);
	onoff("Fast back-to-back transactions", PCI_COMMAND_BACKTOBACK_ENABLE);

	printf("    Status register: 0x%04x\n", (rval >> 16) & 0xffff);
	onoff("66 MHz capable", PCI_STATUS_66MHZ_SUPPORT);
	onoff("User Definable Features (UDF) support", PCI_STATUS_UDF_SUPPORT);
	onoff("Fast back-to-back capable", PCI_STATUS_BACKTOBACK_SUPPORT);
	onoff("Data parity error detected", PCI_STATUS_PARITY_ERROR);

	printf("      DEVSEL timing: ");
	switch (rval & PCI_STATUS_DEVSEL_MASK) {
	case PCI_STATUS_DEVSEL_FAST:
		printf("fast");
		break;
	case PCI_STATUS_DEVSEL_MEDIUM:
		printf("medium");
		break;
	case PCI_STATUS_DEVSEL_SLOW:
		printf("slow");
		break;
	default:
		printf("unknown/reserved");	/* XXX */
		break;
	}
	printf(" (0x%x)\n", (rval & PCI_STATUS_DEVSEL_MASK) >> 25);

	onoff("Slave signaled Target Abort", PCI_STATUS_TARGET_TARGET_ABORT);
	onoff("Master received Target Abort", PCI_STATUS_MASTER_TARGET_ABORT);
	onoff("Master received Master Abort", PCI_STATUS_MASTER_ABORT);
	onoff("Asserted System Error (SERR)", PCI_STATUS_SPECIAL_ERROR);
	onoff("Parity error detected", PCI_STATUS_PARITY_DETECT);

#undef onoff

	rval = regs[o2i(PCI_CLASS_REG)];
	for (classp = pci_class; classp->name != NULL; classp++) {
		if (PCI_CLASS(rval) == classp->val)
			break;
	}
	subclassp = (classp->name != NULL) ? classp->subclasses : NULL;
	while (subclassp && subclassp->name != NULL) {
		if (PCI_SUBCLASS(rval) == subclassp->val)
			break;
		subclassp++;
	}
	if (classp->name != NULL) {
		printf("    Class Name: %s (0x%02x)\n", classp->name,
		    PCI_CLASS(rval));
		if (subclassp != NULL && subclassp->name != NULL)
			printf("    Subclass Name: %s (0x%02x)\n",
			    subclassp->name, PCI_SUBCLASS(rval));
		else
			printf("    Subclass ID: 0x%02x\n", PCI_SUBCLASS(rval));
	} else {
		printf("    Class ID: 0x%02x\n", PCI_CLASS(rval));
		printf("    Subclass ID: 0x%02x\n", PCI_SUBCLASS(rval));
	}
	printf("    Interface: 0x%02x\n", PCI_INTERFACE(rval));
	printf("    Revision ID: 0x%02x\n", PCI_REVISION(rval));

	rval = regs[o2i(PCI_BHLC_REG)];
	printf("    BIST: 0x%02x\n", PCI_BIST(rval));
	printf("    Header Type: 0x%02x%s (0x%02x)\n", PCI_HDRTYPE_TYPE(rval),
	    PCI_HDRTYPE_MULTIFN(rval) ? "+multifunction" : "",
	    PCI_HDRTYPE(rval));
	printf("    Latency Timer: 0x%02x\n", PCI_LATTIMER(rval));
	printf("    Cache Line Size: 0x%02x\n", PCI_CACHELINE(rval));
}

static void
pci_conf_print_type1(pc, tag, regs)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	const pcireg_t *regs;
{
	int off, s;
	pcireg_t mask, rval;

	for (off = PCI_MAPREG_START; off < PCI_MAPREG_END; off += 4) {
		/*
		 * Section 6.2.5.1, `Address Maps', tells us that:
		 *
		 * 1) The builtin software should have already mapped the
		 * device in a reasonable way.
		 *
		 * 2) A device which wants 2^n bytes of memory will hardwire
		 * the bottom n bits of the address to 0.  As recommended,
		 * we write all 1s and see what we get back.
		 */
		rval = regs[o2i(off)];
		if (rval != 0) {
			/*
			 * The following sequence seems to make some devices
			 * (e.g. host bus bridges, which don't normally
			 * have their space mapped) very unhappy, to
			 * the point of crashing the system.
			 *
			 * Therefore, if the mapping register is zero to
			 * start out with, don't bother trying.
			 */
			s = splhigh();
			pci_conf_write(pc, tag, off, 0xffffffff);
			mask = pci_conf_read(pc, tag, off);
			pci_conf_write(pc, tag, off, rval);
			splx(s);
		} else
			mask = 0;

		printf("    Mapping register at 0x%02x: ", off);
		if (rval == 0) {
			printf("not implemented(?)\n");
		} else if (PCI_MAPREG_TYPE(rval) == PCI_MAPREG_TYPE_MEM) {
			const char *type, *cache;

			switch (PCI_MAPREG_MEM_TYPE(rval)) {
			case PCI_MAPREG_MEM_TYPE_32BIT:
				type = "32-bit";
				break;
			case PCI_MAPREG_MEM_TYPE_32BIT_1M:
				type = "32-bit-1M";
				break;
			case PCI_MAPREG_MEM_TYPE_64BIT:
				type = "64-bit";
				break;
			default:
				type = "unknown (XXX)";
				break;
			}
			if (PCI_MAPREG_MEM_CACHEABLE(rval))
				cache = "";
			else
				cache = "non";
			printf("%s %scacheable memory\n", type, cache);
			printf("      base address: 0x%08x, size: 0x%08x\n",
			    PCI_MAPREG_MEM_ADDR(rval),
			    PCI_MAPREG_MEM_SIZE(mask));
		} else {
			printf("i/o\n");
			printf("      base address: 0x%08x, size: 0x%08x\n",
			    PCI_MAPREG_IO_ADDR(rval),
			    PCI_MAPREG_IO_SIZE(mask));
		}
	}

	printf("    Cardbus CIS Pointer: 0x%08x\n", regs[o2i(0x28)]);

	rval = regs[o2i(0x2c)];
	printf("    Subsystem vendor ID: 0x%04x\n", PCI_VENDOR(rval));
	printf("    Subsystem ID: 0x%04x\n", PCI_PRODUCT(rval));

	/* XXX */
	printf("    Expansion ROM Base Address: 0x%08x\n", regs[o2i(0x30)]);
	printf("    Reserved @ 0x34: 0x%08x\n", regs[o2i(0x34)]);
	printf("    Reserved @ 0x38: 0x%08x\n", regs[o2i(0x38)]);

	rval = regs[o2i(PCI_INTERRUPT_REG)];
	printf("    Maximum Latency: 0x%02x\n", (rval >> 24) & 0xff);
	printf("    Minimum Grant: 0x%02x\n", (rval >> 16) & 0xff);
	printf("    Interrupt pin: 0x%02x", PCI_INTERRUPT_PIN(rval));
	switch (PCI_INTERRUPT_PIN(rval)) {
	case PCI_INTERRUPT_PIN_NONE:
		printf(" (none)");
		break;
	case PCI_INTERRUPT_PIN_A:
		printf(" (pin A)");
		break;
	case PCI_INTERRUPT_PIN_B:
		printf(" (pin B)");
		break;
	case PCI_INTERRUPT_PIN_C:
		printf(" (pin C)");
		break;
	case PCI_INTERRUPT_PIN_D:
		printf(" (pin D)");
		break;
	}
	printf("\n");
	printf("    Interrupt line: 0x%02x\n", PCI_INTERRUPT_LINE(rval));
}

void
pci_conf_print(pc, tag, printfn)
	pci_chipset_tag_t pc;
	pcitag_t tag;
	void (*printfn)(pci_chipset_tag_t, pcitag_t, const pcireg_t *);
{
	pcireg_t regs[o2i(256)];
	int off, hdrtype;
	void (*typeprintfn)(pci_chipset_tag_t, pcitag_t, const pcireg_t *);

	printf("PCI configuration registers:\n");

	for (off = 0; off < 256; off += 4)
		regs[o2i(off)] = pci_conf_read(pc, tag, off);

#define print16regs(offset)						\
	printf("    0x%02x: 0x%08x 0x%08x 0x%08x 0x%08x\n", (offset),	\
	    regs[o2i((offset))], regs[o2i((offset) + 4)],		\
	    regs[o2i((offset) + 8)], regs[o2i((offset) + 12)]);

	/* common header */
	printf("  Common header:\n");
	for (off = 0; off < 16; off += 16)
		print16regs(off);
	printf("\n");
	pci_conf_print_common(pc, tag, regs);
	printf("\n");

	/* type-dependent header */
	hdrtype = PCI_HDRTYPE_TYPE(regs[o2i(PCI_BHLC_REG)]);
	printf("  Type %d header:\n", hdrtype);
	for (off = 16; off < 64; off += 16)
		print16regs(off);
	printf("\n");
	switch (hdrtype) {		/* XXX make a table, eventually */
	case 0:
		typeprintfn = &pci_conf_print_type1;
		break;
	case 1:
		/* XXX */
	default:
		typeprintfn = 0;
	}
	if (typeprintfn)
		(*typeprintfn)(pc, tag, regs);
	else
		printf("    Don't know how to pretty-print type %d header.\n",
		    hdrtype);
	printf("\n");

	/* device-dependent header */
	printf("  Device-dependent header:\n");
	for (off = 64; off < 256; off += 16)
		print16regs(off);
	printf("\n");
	if (printfn)
		(*printfn)(pc, tag, regs);
	else
		printf("    Don't know how to pretty-print device-dependent header.\n");
	printf("\n");
}
