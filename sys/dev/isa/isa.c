/*	$NetBSD: isa.c,v 1.62 1994/11/04 02:55:32 mycroft Exp $	*/

/*-
 * Copyright (c) 1993, 1994 Charles Hannum.  All rights reserved.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <i386/isa/isareg.h>
#include <i386/isa/isavar.h>

/* sorry, has to be here, no place else really suitable */
#include <machine/pc/display.h>
u_short *Crtat = (u_short *)MONO_BUF;

int isamatch __P((struct device *, void *, void *));
void isaattach __P((struct device *, struct device *, void *));

struct cfdriver isacd = {
	NULL, "isa", isamatch, isaattach, DV_DULL, sizeof(struct device), 1
};

int
isamatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{

	return (1);
}

int
isaprint(aux, isa)
	void *aux;
	char *isa;
{
	struct isa_attach_args *ia = aux;

	if (ia->ia_iosize)
		printf(" port 0x%x", ia->ia_iobase);
	if (ia->ia_iosize > 1)
		printf("-0x%x", ia->ia_iobase + ia->ia_iosize - 1);
	if (ia->ia_msize)
		printf(" iomem 0x%x", ia->ia_maddr - atdevbase + 0xa0000);
	if (ia->ia_msize > 1)
		printf("-0x%x",
		    ia->ia_maddr - atdevbase + 0xa0000 + ia->ia_msize - 1);
	if (ia->ia_irq)
		printf(" irq %d", ffs(ia->ia_irq) - 1);
	if (ia->ia_drq != (u_short)-1)
		printf(" drq %d", ia->ia_drq);
	/* XXXX print flags */
	return (QUIET);
}

int
isasubmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct device *self = match;
	struct cfdata *cf = self->dv_cfdata;
	struct isa_attach_args ia;

	ia.ia_iobase = cf->cf_loc[0];
	ia.ia_iosize = 0x666;
	ia.ia_maddr = cf->cf_loc[2] - 0xa0000 + atdevbase;
	ia.ia_msize = cf->cf_loc[3];
	ia.ia_irq = (cf->cf_loc[4] == -1) ? IRQUNK : (1 << cf->cf_loc[4]);
	ia.ia_drq = cf->cf_loc[5];

	if ((*cf->cf_driver->cd_match)(parent, match, &ia) <= 0) {
		/*
		 * If we don't do this, isaattach() will repeatedly try to
		 * probe devices that weren't found.  But we need to be careful
		 * to do it only for the ISA bus, or we would cause things like
		 * `com0 at ast? slave ?' to not probe on the second ast.
		 */
		if (parent->dv_cfdata->cf_driver == &isacd)
			cf->cf_fstate = FSTATE_FOUND;
		return (0);
	}

	config_attach(parent, match, &ia, isaprint);
	return (1);
}

void
isaattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{

	printf("\n");

	while (config_search(isasubmatch, self, NULL) != NULL);
}
