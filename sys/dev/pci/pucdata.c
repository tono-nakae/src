/*	$NetBSD: pucdata.c,v 1.1 1998/06/26 18:52:41 cgd Exp $	*/

/*
 * Copyright (c) 1998 Christopher G. Demetriou.  All rights reserved.
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
 * PCI "universal" communications card driver configuration data (used to
 * match/attach the cards).
 *
 * Author: Christopher G. Demetriou, May 14, 1998.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pucvar.h>

const struct puc_device_description puc_devices[] = {
	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin 4006 (single parallel)
	 */

	/*
	 * Dolphin 4014 (dual parallel port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   "Dolphin 4014",
	    {	0x10b5,	0x9050,	0xd84d,	0x6810	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_LPT, 0x20 },
		{ PUC_PORT_TYPE_LPT, 0x24 },
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX Dolphin 4025 (single serial)
	 */

	/*
	 * Dolphin 4035 (dual serial port) card.  PLX 9050, with
	 * a seemingly-lame EEPROM setup that puts the Dolphin IDs
	 * into the subsystem fields, and claims that it's a
	 * network/misc (0x02/0x80) device.
	 */
	{   "Dolphin 4035",
	    {	0x10b5,	0x9050,	0xd84d,	0x6808	},
	    {	0xffff,	0xffff,	0xffff,	0xffff	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18 },
		{ PUC_PORT_TYPE_COM, 0x1c },
	    },
	},

	/*
	 * XXX no entry because I have no data:
	 * XXX SIIG CyberParallel PCI (single parallel)
	 */

	/*
	 * SIIG CyberSerial PCI (single serial port) card.  PLX 9052, with
	 * a more sensible EEPROM setup that reports "normal"-looking
	 * vendor and product IDs, and sensible class/subclass info,
	 * communications/serial (0x07/0x00), interface 0x02.
	 */
	{   "SIIG CyberSerial PCI",
	    {	0x131f,	0x1000,	0,	0	},
	    {	0xffff,	0xffff,	0,	0	},
	    {
		{ PUC_PORT_TYPE_COM, 0x18 },
	    },
	},

	{ 0 }
};
