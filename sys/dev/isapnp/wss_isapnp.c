/*	$NetBSD: wss_isapnp.c,v 1.1 1998/06/30 17:28:00 augustss Exp $	*/

/*
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <machine/bus.h>

#include <sys/audioio.h>
#include <dev/audio_if.h>

#include <dev/isa/isavar.h>
#include <dev/isa/isadmavar.h>

#include <dev/isapnp/isapnpreg.h>
#include <dev/isapnp/isapnpvar.h>

#include <dev/isa/ad1848var.h>
#include <dev/isa/madreg.h>
#include <dev/isa/wssreg.h>
#include <dev/isa/wssvar.h>

int	wss_isapnp_match __P((struct device *, struct cfdata *, void *));
void	wss_isapnp_attach __P((struct device *, struct device *, void *));

struct cfattach wss_isapnp_ca = {
	sizeof(struct wss_softc), wss_isapnp_match, wss_isapnp_attach
};


/*
 * Probe / attach routines.
 */

/*
 * Probe for the WSS hardware.
 */
int
wss_isapnp_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isapnp_attach_args *ipa = aux;

	if (strcmp(ipa->ipa_devlogic, "CSC0000") == 0)
		return (1);

	return (0);
}



/*
 * Attach hardware to driver, attach hardware driver to audio
 * pseudo-device driver.
 */
void
wss_isapnp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wss_softc *sc = (struct wss_softc *)self;
	struct isapnp_attach_args *ipa = aux;

	printf("\n");

	if (isapnp_config(ipa->ipa_iot, ipa->ipa_memt, ipa)) {
		printf("%s: error in region allocation\n", 
		       sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_iot = ipa->ipa_iot;
        sc->sc_ioh = ipa->ipa_io[1].h;

        sc->mad_chip_type = MAD_NONE;

	/* Set up AD1848 I/O handle. */
	sc->sc_ad1848.sc_ic  = ipa->ipa_ic;
	sc->sc_ad1848.sc_iot = ipa->ipa_iot;
	sc->sc_ad1848.sc_ioh = sc->sc_ioh;
	sc->sc_ad1848.sc_iooffs = WSS_CODEC;
	sc->sc_ad1848.mode = 2;

        sc->wss_ic  = ipa->ipa_ic;
	sc->wss_irq = ipa->ipa_irq[0].num;
	sc->wss_drq = ipa->ipa_drq[0].num;
	sc->wss_recdrq = 
		ipa->ipa_ndrq > 1 ? ipa->ipa_drq[1].num : ipa->ipa_drq[0].num;

	if (!ad1848_probe(&sc->sc_ad1848)) {
		printf("%s: ad1848_probe failed\n", sc->sc_dev.dv_xname);
		return;
	}

	printf("%s: %s %s", sc->sc_dev.dv_xname, ipa->ipa_devident,
	       ipa->ipa_devclass);

	wssattach(sc);
}
