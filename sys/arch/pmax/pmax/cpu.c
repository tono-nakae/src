/*	$NetBSD: cpu.c,v 1.3 1996/03/17 01:47:04 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/device.h>

#include <machine/autoconf.h>
/*#include <machine/rpb.h>*/

/* Definition of the driver for autoconfig. */
static int	cpumatch(struct device *, void *, void *);
static void	cpuattach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof (struct device), cpumatch, cpuattach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

static int	cpuprint __P((void *, char *pnp));

extern void cpu_configure __P((void));
static int
cpumatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

	/* make sure that we're looking for a CPU. */
	if (strcmp(ca->ca_name, cpu_cd.cd_name) != 0)
		return (0);

	return (1);
}

static void
cpuattach(parent, dev, aux)
	struct device *parent;
	struct device *dev;
	void *aux;
{

	/* Identify cpu. */

	cpu_configure();
	printf("\n");

	/* Work out what kind of FPU is present. */

#if 0
	if (major == PCS_PROC_LCA4) {
		struct confargs nca;

		/*
		 * If the processor is an KN01, it's got no bus,
		 * but a fixed set of onboard devices.
		 * Attach it here. (!!!)
		 */
		nca.ca_name = "kn01";
		nca.ca_slot = 0;
		nca.ca_offset = 0;
		if (!config_found(dev, &nca, cpuprint))
			panic("cpuattach: couldn't attach LCA bus interface");
	}
#endif
}

static int
cpuprint(aux, pnp)
	void *aux;
	char *pnp;
{
	register struct confargs *ca = aux;

	if (pnp)
		printf("%s at %s", ca->ca_name, pnp);
	return (UNCONF);
}
