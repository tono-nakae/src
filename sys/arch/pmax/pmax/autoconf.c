/*	$NetBSD: autoconf.c,v 1.50 2000/02/29 04:41:50 nisimura Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: autoconf.c 1.31 91/01/21
 *
 *	@(#)autoconf.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
__KERNEL_RCSID(0, "$NetBSD: autoconf.c,v 1.50 2000/02/29 04:41:50 nisimura Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/intr.h>
#include <machine/sysconf.h>

#include <pmax/dev/device.h>

#include "rz.h"
#include "xasc_ioasic.h"
#include "xasc_pmaz.h"

#if NRZ > 0 && (NXASC_PMAZ + NXASC_IOASIC) > 0
#error  MI SCSI can not coexist with pmax old SCSI.
#endif

struct intrhand intrtab[MAX_DEV_NCOOKIES];
struct device *booted_device;
int	booted_slot, booted_unit, booted_partition;
char	*booted_protocol;

/*
 * Configure all devices on system
 */     
void
cpu_configure()
{
	/* Kick off autoconfiguration. */
	(void)splhigh();
	if (config_rootfound("mainbus", "mainbus") == NULL)
		panic("no mainbus found");

	/* Reset any bus errors due to probing nonexistent devices. */
	(*platform.bus_reset)();

	/* Configuration is finished, turn on interrupts. */
	_splnone();	/* enable all source forcing SOFT_INTs cleared */
#if NRZ > 0
	printf("Beginning old-style SCSI device autoconfiguration\n");
	configure_scsi();
#endif
}

/*
 * Look at the string 'cp' and decode the boot device.  Boot names
 * can be something like 'rz(0,0,0)vmunix' or '5/rz0/vmunix'.
 *
 * 3100 allows abbrivation;
 *	dev(controller[,uni-number[,partition-number]]])[filename]
 */
void
makebootdev(cp)
	char *cp;
{
	booted_device = NULL;
	booted_slot = booted_unit = booted_partition = 0;
	booted_protocol = NULL;

	if (cp[0] == 'r' && cp[1] == 'z' && cp[2] == '(') {
		cp += 3;
		if (*cp >= '0' && *cp <= '9')
			booted_slot = *cp++ - '0';
		if (*cp == ',')
			cp += 1;
		if (*cp >= '0' && *cp <= '9')
			booted_unit = *cp++ - '0';
		if (*cp == ',')
			cp += 1;
		if (*cp >= '0' && *cp <= '9')
			booted_partition = *cp - '0';
		booted_protocol = "SCSI";
		return;
	}
	if (cp[0] >= '0' && cp[0] <= '9' && cp[1] == '/') {
		booted_slot = cp[0] - '0';
		if (cp[2] == 'r' && cp[3] == 'z'
		    && cp[4] >= '0' && cp[4] <= '9') {
			booted_protocol = "SCSI";
			booted_unit = cp[4] - '0';
		}
		else if (strncmp(cp+2, "tftp", 4) == 0)
			booted_protocol = "BOOTP";
		else if (strncmp(cp+2, "mop", 3) == 0)
			booted_protocol = "MOP";
	}
}

void
cpu_rootconf()
{
#if NRZ > 0
	struct device *dv;
	char name[4];

	/*
	 * N.B., below works for rz drive on primary SCSI controller.
	 */
	booted_device = NULL;
	snprintf(name, sizeof(name), "rz%d", booted_unit);
	for (dv = TAILQ_FIRST(&alldevs); dv; dv = TAILQ_NEXT(dv, dv_list)) {
		if (dv->dv_class == DV_DISK && !strcmp(dv->dv_xname, name)) {
			booted_device = dv;
			break;
		}
	}
#endif
	printf("boot device: %s\n",
	    booted_device ? booted_device->dv_xname : "<unknown>");

	setroot(booted_device, booted_partition);
}

#if (NXASC_PMAZ + NXASC_IOASIC) > 0

#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <sys/disk.h>

struct xx_softc {
	struct device sc_dev;
	struct disk sc_dk;
	int flag;
	struct scsipi_link *sc_link;
};	
#define	SCSITARGETID(dev) ((struct xx_softc *)dev)->sc_link->scsipi_scsi.target

int slot_in_progress; /* XXX - TC slot being probed, ugly backdoor interface */

void
dk_establish(dk, dev)
	struct disk *dk;
	struct device *dev;
{
	if (booted_device || strcmp(booted_protocol, "SCSI"))
		return;
	if (booted_slot != slot_in_progress)
		return;
	if (booted_unit != SCSITARGETID(dev))
		return;
	booted_device = dev;
}
#endif
