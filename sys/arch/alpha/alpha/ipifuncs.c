/* $NetBSD: ipifuncs.c,v 1.1 1998/09/26 00:03:52 thorpej Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

__KERNEL_RCSID(0, "$NetBSD: ipifuncs.c,v 1.1 1998/09/26 00:03:52 thorpej Exp $");

/*
 * Interprocessor interrupt handlers.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/alpha_cpu.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/rpb.h>

#include <alpha/alpha/cpuvar.h>

void	alpha_ipi_halt __P((void));
void	alpha_ipi_imb __P((void));
void	alpha_ipi_tbia __P((void));
void	alpha_ipi_tbiap __P((void));

ipifunc_t ipifuncs[ALPHA_NIPIS] = {
	alpha_ipi_halt,
	alpha_ipi_imb,
	alpha_ipi_tbia,
	alpha_ipi_tbiap,
};

/*
 * Send an interprocessor interrupt.
 */
void
alpha_send_ipi(cpu_id, ipinum)
	u_long cpu_id, ipinum;
{
	struct cpu_softc *sc;
	u_long ipimask;

#ifdef DIAGNOSTIC
	if (ipinum >= ALPHA_NIPIS)
		panic("alpha_sched_ipi: bogus ipinum");

	if (cpu_id >= hwrpb->rpb_pcs_cnt || (sc = cpus[cpu_id]) == NULL)
		panic("alpha_sched_ipi: bogus cpu_id");
#endif

	ipimask = (1UL << ipinum);
	alpha_atomic_setbits_q(&sc->sc_ipis, ipimask);
	alpha_pal_wripir(cpu_id);
}

void
alpha_ipi_halt()
{

	/* XXX Implement me! */
}

void
alpha_ipi_imb()
{

	alpha_pal_imb();
}

void
alpha_ipi_tbia()
{

	ALPHA_TBIA();
}

void
alpha_ipi_tbiap()
{

	ALPHA_TBIAP();
}
