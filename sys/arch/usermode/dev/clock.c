/* $NetBSD: clock.c,v 1.22 2011/12/13 22:22:08 jmcneill Exp $ */

/*-
 * Copyright (c) 2007 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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
__KERNEL_RCSID(0, "$NetBSD: clock.c,v 1.22 2011/12/13 22:22:08 jmcneill Exp $");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/lwp.h>
#include <sys/cpu.h>
#include <sys/malloc.h>
#include <sys/timetc.h>
#include <sys/time.h>

#include <machine/pcb.h>
#include <machine/mainbus.h>
#include <machine/thunk.h>

#include <dev/clock_subr.h>

static int	clock_match(device_t, cfdata_t, void *);
static void	clock_attach(device_t, device_t, void *);

static void	clock(void);
static void	clock_signal(int sig, siginfo_t *info, void *ctx);
static unsigned int clock_getcounter(struct timecounter *);

static int	clock_todr_gettime(struct todr_chip_handle *, struct timeval *);

typedef struct clock_softc {
	device_t		sc_dev;
	struct todr_chip_handle	sc_todr;
} clock_softc_t;

static struct timecounter clock_timecounter = {
	clock_getcounter,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	1000000000ULL,		/* frequency */
	"CLOCK_MONOTONIC",	/* name */
	-100,			/* quality */
	NULL,			/* prev */
	NULL,			/* next */
};

static struct clock_softc *clock_sc;



CFATTACH_DECL_NEW(clock, sizeof(clock_softc_t),
    clock_match, clock_attach, NULL, NULL);

static int
clock_match(device_t parent, cfdata_t match, void *opaque)
{
	struct thunkbus_attach_args *taa = opaque;

	if (taa->taa_type != THUNKBUS_TYPE_CLOCK)
		return 0;

	return 1;
}

static void
clock_attach(device_t parent, device_t self, void *opaque)
{
	static struct sigaction sa;
	clock_softc_t *sc = device_private(self);
	long tcres;

	aprint_naive("\n");
	aprint_normal("\n");

	KASSERT(clock_sc == NULL);
	clock_sc = sc;

	sc->sc_dev = self;

	sc->sc_todr.todr_gettime = clock_todr_gettime;
	todr_attach(&sc->sc_todr);

	memset(&sa, 0, sizeof(sa));
	thunk_sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = clock_signal;
	sa.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
	if (thunk_sigaction(SIGALRM, &sa, NULL) == -1)
		panic("couldn't register SIGALRM handler : %d",
		    thunk_geterrno());

	tcres = thunk_clock_getres_monotonic();
	if (tcres > 0) {
		clock_timecounter.tc_quality = 1000;
	}
	tc_init(&clock_timecounter);
}

static void
clock(void)
{
	struct clockframe cf;

	curcpu()->ci_idepth++;
	spl_intr(IPL_SOFTCLOCK, (void (*)(void *)) hardclock, &cf);
	curcpu()->ci_idepth--;
}

static void
clock_signal(int sig, siginfo_t *info, void *ctx)
{
#if 0
	ucontext_t *uct = ctx;
	struct lwp *l;
	struct pcb *pcb;

	l = curlwp;
	pcb = lwp_getpcb(l);

	/* copy this state as where the lwp was XXX NEEDED? */
	memcpy(&pcb->pcb_ucp, uct, sizeof(ucontext_t));
#endif

	clock();
}

static unsigned int
clock_getcounter(struct timecounter *tc)
{
	return thunk_getcounter();
}

static int
clock_todr_gettime(struct todr_chip_handle *tch, struct timeval *tv)
{
	struct thunk_timeval ttv;
	int error;

	error = thunk_gettimeofday(&ttv, NULL);
	if (error)
		return error;

	tv->tv_sec = ttv.tv_sec;
	tv->tv_usec = ttv.tv_usec;

	return 0;
}
