/*	$NetBSD: isa_machdep.c,v 1.2 2000/03/04 13:43:19 takemura Exp $	*/

/*
 * Copyright (c) 1999, by UCHIYAMA Yasushi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isareg.h>

#include <machine/platid.h>
#include <machine/platid_mask.h>

#include <hpcmips/vr/vripreg.h>
#include <hpcmips/vr/vripvar.h>
#include <hpcmips/vr/vrgiureg.h>

#include "locators.h"

int	vrisabprint __P((void*, const char*));
int	vrisabmatch __P((struct device*, struct cfdata*, void*));
void	vrisabattach __P((struct device*, struct device*, void*));

struct vrisab_softc {
	struct device sc_dev;
	vrgiu_chipset_tag_t sc_gc;
	vrgiu_function_tag_t sc_gf;
	int sc_intr_map[MAX_GPIO_INOUT]; /* ISA <-> GIU inerrupt line mapping */
};

struct cfattach vrisab_ca = {
	sizeof(struct vrisab_softc), vrisabmatch, vrisabattach
};

#ifdef DEBUG_FIND_PCIC
#include <mips/cpuregs.h>
#warning DEBUG_FIND_PCIC
static void __find_pcic __P((void));
#endif

int
vrisabmatch(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct gpbus_attach_args *gpa = aux;
	platid_mask_t mask;
    
	if (strcmp(gpa->gpa_busname, match->cf_driver->cd_name))
		return 0;
	if (match->cf_loc[VRISABIFCF_PLATFORM] == VRISABIFCF_PLATFORM_DEFAULT) 
		return 1;
	mask = PLATID_DEREF(match->cf_loc[VRISABIFCF_PLATFORM]);
	if (platid_match(&platid, &mask))
		return 2;
	return 0;
}

void
vrisabattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct gpbus_attach_args *gpa = aux;
	struct vrgiu_softc *chipset; 
	struct vrisab_softc *sc = (void*)self;
	struct isabus_attach_args iba;
	bus_addr_t offset;
	int i;
    
	sc->sc_gc = chipset = gpa->gpa_gc;
	sc->sc_gf = gpa->gpa_gf;

	iba.iba_busname = "isa";
	iba.iba_ic	    = sc;
	iba.iba_dmat    = 0; /* XXX not yet */

	/* Allocate ISA memory space */
	iba.iba_memt    = hpcmips_alloc_bus_space_tag();
	strcpy(iba.iba_memt->t_name, "ISA mem");
	offset = sc->sc_dev.dv_cfdata->cf_loc[VRISABIFCF_ISAMEMOFFSET];
	iba.iba_memt->t_base = VR_ISA_MEM_BASE + offset;
	iba.iba_memt->t_size = VR_ISA_MEM_SIZE - offset;
	hpcmips_init_bus_space_extent(iba.iba_memt);

	/* Allocate ISA port space */
	iba.iba_iot     = hpcmips_alloc_bus_space_tag();
	strcpy(iba.iba_iot->t_name, "ISA port");
	/* Platform dependent setting. */
	offset = sc->sc_dev.dv_cfdata->cf_loc[VRISABIFCF_ISAPORTOFFSET];
	iba.iba_iot->t_base = VR_ISA_PORT_BASE + offset;
	iba.iba_iot->t_size = VR_ISA_PORT_SIZE - offset;

	hpcmips_init_bus_space_extent(iba.iba_iot);

#ifdef DEBUG_FIND_PCIC
#warning DEBUG_FIND_PCIC
	__find_pcic();
#else
	/* Initialize ISA IRQ <-> GPIO mapping */
	for (i = 0; i < MAX_GPIO_INOUT; i++)
		sc->sc_intr_map[i] = -1;
	printf (":ISA port %#x-%#x mem %#x-%#x\n",
		iba.iba_iot->t_base, iba.iba_iot->t_base + iba.iba_iot->t_size,
		iba.iba_memt->t_base, iba.iba_memt->t_base + iba.iba_memt->t_base);
	config_found(self, &iba, vrisabprint);
#endif
}

int
vrisabprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	if (pnp)
		return (QUIET);
	return (UNCONF);
}

void
isa_attach_hook(parent, self, iba)
	struct device *parent, *self;
	struct isabus_attach_args *iba;
{
}

void *
isa_intr_establish(ic, intr, type, level, ih_fun, ih_arg)
	isa_chipset_tag_t ic;
	int intr;
	int type;  /* XXX not yet */
	int level;  /* XXX not yet */
	int (*ih_fun) __P((void*));
	void *ih_arg;
{
	struct vrisab_softc *sc = (void*)ic;
	int port, irq;

	/*
	 * 'intr' encoding:
	 *
	 * 0x0000000f ISA IRQ#
	 * 0x00ff0000 GPIO port# (if port# is 0xff, it means 'not specified')
	 * 0x01000000 interrupt signal hold/through	(1:hold/0:though)
	 * 0x02000000 interrupt detection level		(1:low /0:high	)
	 * 0x04000000 interrupt detection trigger	(1:edge/0:level	)
	 */
#define INTR_IRQ(i)	(((i)>> 0) & 0x0f)
#define INTR_PORT(i)	(((i)>>16) & 0xff)
#define INTR_MODE(i)	(((i)>>24) & 0x07)
#define INTR_PORTISNULL(p)	((p) == 0xff)
#define INTR_PORTNULL	0x00ff0000
	static int intr_modes[8] = {
		VRGIU_INTR_LEVEL_HIGH_THROUGH,
		VRGIU_INTR_LEVEL_HIGH_HOLD,
		VRGIU_INTR_LEVEL_LOW_THROUGH,
		VRGIU_INTR_LEVEL_LOW_HOLD,
		VRGIU_INTR_EDGE_THROUGH,
		VRGIU_INTR_EDGE_HOLD,
		VRGIU_INTR_EDGE_THROUGH,
		VRGIU_INTR_EDGE_HOLD,
	};
	static char* intr_mode_names[8] = {
		"level high through",
		"level high hold",
		"level low through",
		"level low hold",
		"edge through",
		"edge hold",
		"edge through",
		"edge hold",
	};

	/* 
	 * ISA IRQ <-> GPIO port mapping
	 */
	irq = INTR_IRQ(intr);
	port = INTR_PORT(intr);
	if (INTR_PORTISNULL(port)) {
		/* GPIO port not specfied */
		port = sc->sc_intr_map[irq]; /* Use Already mapped port */
	} else {
		/* GPIO port specified. */
		if (sc->sc_intr_map[irq] != -1)
			panic("isa_intr_establish: conflict GPIO line. %d <-> %d",
			      port, sc->sc_intr_map[irq]);
		sc->sc_intr_map[irq] = port; /* Register it */
		printf("ISA IRQ %d -> GPIO port %d, %s\n",
		       irq, port, intr_mode_names[INTR_MODE(intr)]);
	}
	if (port == -1)
		panic("isa_intr_establish: can't ISA IRQ to GIU port.");
	/* Call Vr routine */
	return sc->sc_gf->gf_intr_establish(sc->sc_gc, port,
					    intr_modes[INTR_MODE(intr)],
					    level, ih_fun, ih_arg);
}

void
isa_intr_disestablish(ic, arg)
	isa_chipset_tag_t ic;
	void *arg;
{
	struct vrisab_softc *sc = (void*)ic;
	/* Call Vr routine */
	sc->sc_gf->gf_intr_disestablish(sc->sc_gc, arg);
}

int
isa_intr_alloc(ic, mask, type, irq)
	isa_chipset_tag_t ic;
	int mask;
	int type;
	int *irq;
{
	/* XXX not coded yet. this is temporary XXX */
	printf ("isa_intr_alloc:");
	bitdisp32(mask);
	*irq = ((ffs(mask) -1) | INTR_PORTNULL); /* XXX */
	return 0;
}

#ifdef DEBUG_FIND_PCIC
#warning DEBUG_FIND_PCIC
static void
__find_pcic(void)
{
	int i, j, step, found;
	u_int32_t addr;
	u_int8_t reg;
	int __read_revid (u_int32_t port) 
		{
			addr = MIPS_PHYS_TO_KSEG1(i + port);
			printf("%#x\r", i);
			for (found = 0, j = 0; j < 0x100; j += 0x40) {
				*((volatile u_int8_t*)addr) = j;
				reg = *((volatile u_int8_t*)(addr + 1));
#ifdef DEBUG_FIND_PCIC_I82365SL_ONLY
				if (reg == 0x82 || reg == 0x83) {
#else
				if ((reg & 0xc0) == 0x80) {
#endif
					found++;
				}
			}
			if (found)
				printf("\nfound %d socket at %#x (base from %#x)\n", found, addr,
				       i + port - VR_ISA_PORT_BASE);
		};
	step = 0x1000000;
	printf("\nFinding PCIC. Trying ISA port %#x-%#x step %#x\n", 
	       VR_ISA_PORT_BASE, VR_ISA_PORT_BASE + VR_ISA_PORT_SIZE, step);
	for (i = VR_ISA_PORT_BASE; i < VR_ISA_PORT_BASE+VR_ISA_PORT_SIZE; i+= step) {
		__read_revid (0x3e0);
		__read_revid (0x3e2);
	}
}
#endif

	
