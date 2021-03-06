/*	$NetBSD: bcm2835_rng.c,v 1.6 2013/08/01 11:30:38 skrll Exp $ */

/*-
 * Copyright (c) 2013 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jared D. McNeill
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: bcm2835_rng.c,v 1.6 2013/08/01 11:30:38 skrll Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/rnd.h>
#include <sys/atomic.h>

#include <arm/broadcom/bcm_amba.h>
#include <arm/broadcom/bcm2835reg.h>
#include <arm/broadcom/bcm2835_intr.h>

#define RNG_CTRL		0x00
#define  RNG_CTRL_EN		__BIT(0)
#define RNG_STATUS		0x04
#define  RNG_STATUS_CNT_MASK	__BITS(31,24)
#define  RNG_STATUS_CNT_SHIFT	24
#define RNG_DATA		0x08

#define RNG_DATA_MAX		256

struct bcm2835rng_softc {
	device_t sc_dev;

	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_ioh;

	krndsource_t sc_rnd;

	kmutex_t sc_mutex;

	uint32_t sc_data[RNG_DATA_MAX];
};

static void bcmrng_get(size_t, void *);
static int bcmrng_match(device_t, cfdata_t, void *);
static void bcmrng_attach(device_t, device_t, void *);

CFATTACH_DECL_NEW(bcmrng_amba, sizeof(struct bcm2835rng_softc),
    bcmrng_match, bcmrng_attach, NULL, NULL);

/* ARGSUSED */
static int
bcmrng_match(device_t parent, cfdata_t match, void *aux)
{
	struct amba_attach_args *aaa = aux;

	if (strcmp(aaa->aaa_name, "bcmrng") != 0)
		return 0;

	return 1;
}

static void
bcmrng_attach(device_t parent, device_t self, void *aux)
{
	struct bcm2835rng_softc *sc = device_private(self);
 	struct amba_attach_args *aaa = aux;
	uint32_t ctrl;

	aprint_naive("\n");
	aprint_normal(": RNG\n");

	sc->sc_dev = self;
	sc->sc_iot = aaa->aaa_iot;

	if (bus_space_map(aaa->aaa_iot, aaa->aaa_addr, BCM2835_RNG_SIZE, 0,
	    &sc->sc_ioh)) {
		aprint_error_dev(sc->sc_dev, "unable to map device\n");
		return;
	}

	mutex_init(&sc->sc_mutex, MUTEX_DEFAULT, IPL_VM);

	rndsource_setcb(&sc->sc_rnd, bcmrng_get, sc);
	rnd_attach_source(&sc->sc_rnd, device_xname(self), RND_TYPE_RNG,
	    RND_FLAG_NO_ESTIMATE|RND_FLAG_HASCB);

	/* discard initial numbers, broadcom says they are "less random" */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, RNG_STATUS, 0x40000);

	/* enable rng */
	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, RNG_CTRL);
	ctrl |= RNG_CTRL_EN;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, RNG_CTRL, ctrl);
}

static void
bcmrng_get(size_t bytes, void *priv)
{
	struct bcm2835rng_softc *sc = priv;
	uint32_t status;
	int need = bytes, cnt;

	mutex_spin_enter(&sc->sc_mutex);

	if (__predict_false(need < 1)) {
		return;
	}

	while (need > 0) {
		status = bus_space_read_4(sc->sc_iot, sc->sc_ioh, RNG_STATUS);
		cnt = (status & RNG_STATUS_CNT_MASK) >> RNG_STATUS_CNT_SHIFT;
		if (cnt > 0) {
			bus_space_read_multi_4(sc->sc_iot, sc->sc_ioh,
					       RNG_DATA, sc->sc_data, cnt);
			rnd_add_data(&sc->sc_rnd, sc->sc_data,
		    		     cnt * 4, cnt * 4 * NBBY);
		}

		need -= cnt * 4;
	}
	mutex_spin_exit(&sc->sc_mutex);
}
