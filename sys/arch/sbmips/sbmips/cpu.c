/* $NetBSD: cpu.c,v 1.1 2002/03/06 02:13:51 simonb Exp $ */

/*
 * Copyright 2000, 2001
 * Broadcom Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and copied only
 * in accordance with the following terms and conditions.  Subject to these
 * conditions, you may download, copy, install, use, modify and distribute
 * modified or unmodified copies of this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce and
 *    retain this copyright notice and list of conditions as they appear in
 *    the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Broadcom Corporation. Neither the "Broadcom Corporation" name nor any
 *    trademark or logo of Broadcom Corporation may be used to endorse or
 *    promote products derived from this software without the prior written
 *    permission of Broadcom Corporation.
 *
 * 3) THIS SOFTWARE IS PROVIDED "AS-IS" AND ANY EXPRESS OR IMPLIED
 *    WARRANTIES, INCLUDING BUT NOT LIMITED TO, ANY IMPLIED WARRANTIES OF
 *    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
 *    NON-INFRINGEMENT ARE DISCLAIMED. IN NO EVENT SHALL BROADCOM BE LIABLE
 *    FOR ANY DAMAGES WHATSOEVER, AND IN PARTICULAR, BROADCOM SHALL NOT BE
 *    LIABLE FOR DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *    OR OTHERWISE), EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <mips/locore.h>
#include <mips/cache.h>

#include <machine/cpu.h>

#include <mips/sibyte/include/zbbusvar.h>
#include <mips/sibyte/include/sb1250_regs.h>
#include <mips/sibyte/dev/sbscdvar.h>

static int	cpu_match(struct device *, struct cfdata *, void *);
static void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

static int
cpu_match(struct device *parent, struct cfdata *match, void *aux)
{
	struct zbbus_attach_args *zap = aux;

	if (zap->za_locs.za_type != ZBBUS_ENTTYPE_CPU)
		return (0);

	return 1;
}

static void
cpu_attach(struct device *parent, struct device *self, void *aux)
{
	uint32_t config;

	/* XXX this code must run on the target cpu */
	config = mips3_cp0_config_read();
	config &= ~MIPS3_CONFIG_K0_MASK;
	config |= 0x05;				/* XXX.  cacheable coherent */
	mips3_cp0_config_write(config);

	/*
	 * Flush all of the caches, so that any lines marked non-coherent will
	 * be flushed.  Don't need to worry about L2; it's always
	 * coherent (XXX???).
	 */
	mips_icache_sync_all();
	mips_dcache_wbinv_all();

	printf("%s: ", self->dv_xname);
	cpu_identify();

	/* make sure processor is available for use */
	/* XXXCGD */
}
