/* $NetBSD: wskbd.c,v 1.1 1998/03/22 14:24:03 drochner Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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

static const char _copyright[] __attribute__ ((unused)) =
    "Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.";
static const char _rcsid[] __attribute__ ((unused)) =
    "$NetBSD: wskbd.c,v 1.1 1998/03/22 14:24:03 drochner Exp $";

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)kbd.c	8.2 (Berkeley) 10/30/93
 */

/*
 * Keyboard driver (/dev/wskbd*).  Translates incoming bytes to ASCII or
 * to `wscons_events' and passes them up to the appropriate reader.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/errno.h>
#include <sys/fcntl.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wseventvar.h>
#include <dev/wscons/wscons_callbacks.h>

struct wskbd_softc {
	struct device	sc_dv;

	const struct wskbd_accessops *sc_accessops;
	void		*sc_accesscookie;

	int		sc_ready;	/* accepting events */
	struct wseventvar sc_events;	/* event queue state */

	int	sc_isconsole;
	struct device	*sc_displaydv;

	struct wskbd_bell_data sc_bell_data;
	struct wskbd_keyrepeat_data sc_keyrepeat_data;

	int	sc_repeating;		/* we've called timeout() */
	const u_char *sc_repeatstr;	/* repeated character (string) */
	u_int	sc_repeatstrlen;	/* repeated character (string) len */

	int	sc_translating;		/* xlate to chars for emulation */
};

#ifdef __BROKEN_INDIRECT_CONFIG
int	wskbd_match __P((struct device *, void *, void *));
#else
int	wskbd_match __P((struct device *, struct cfdata *, void *));
#endif
void	wskbd_attach __P((struct device *, struct device *, void *));

struct cfattach wskbd_ca = {
	sizeof (struct wskbd_softc), wskbd_match, wskbd_attach,
};

extern struct cfdriver wskbd_cd;

#ifndef WSKBD_DEFAULT_BELL_PITCH
#define	WSKBD_DEFAULT_BELL_PITCH	1500	/* 1500Hz */
#endif
#ifndef WSKBD_DEFAULT_BELL_PERIOD
#define	WSKBD_DEFAULT_BELL_PERIOD	100	/* 100ms */
#endif
#ifndef WSKBD_DEFAULT_BELL_VOLUME
#define	WSKBD_DEFAULT_BELL_VOLUME	50	/* 50% volume */
#endif

struct wskbd_bell_data wskbd_default_bell_data = {
	WSKBD_BELL_DOALL,
	WSKBD_DEFAULT_BELL_PITCH,
	WSKBD_DEFAULT_BELL_PERIOD,
	WSKBD_DEFAULT_BELL_VOLUME,
};

#ifndef WSKBD_DEFAULT_KEYREPEAT_DEL1
#define	WSKBD_DEFAULT_KEYREPEAT_DEL1	400	/* 400ms to start repeating */
#endif
#ifndef WSKBD_DEFAULT_KEYREPEAT_DELN
#define	WSKBD_DEFAULT_KEYREPEAT_DELN	100	/* 100ms to between repeats */
#endif

struct wskbd_keyrepeat_data wskbd_default_keyrepeat_data = {
	WSKBD_KEYREPEAT_DOALL,
	WSKBD_DEFAULT_KEYREPEAT_DEL1,
	WSKBD_DEFAULT_KEYREPEAT_DELN,
};

cdev_decl(wskbd);
static void wskbd_repeat __P((void *v));

static int wskbd_console_initted;
static struct wskbd_softc *wskbd_console_device;
static const struct wskbd_consops *wskbd_console_ops;
static void *wskbd_console_cookie;

/*
 * Print function (for parent devices).
 */
int
wskbddevprint(aux, pnp)
	void *aux;
	const char *pnp;
{
#if 0
	struct wskbddev_attach_args *ap = aux;
#endif

	if (pnp)
		printf("wskbd at %s", pnp);
#if 0
	printf(" console %d", ap->console);
#endif

	return (UNCONF);
}

int
#ifdef __BROKEN_INDIRECT_CONFIG
wskbd_match(parent, matchv, aux)
#else
wskbd_match(parent, match, aux)
#endif
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *matchv;
#else
	struct cfdata *match;
#endif
	void *aux;
{
#ifdef __BROKEN_INDIRECT_CONFIG
	struct cfdata *match = matchv;
#endif
	struct wskbddev_attach_args *ap = aux;

	if (match->wskbddevcf_console != WSKBDDEVCF_CONSOLE_UNK) {
		/*
		 * If console-ness of device specified, either match
		 * exactly (at high priority), or fail.
		 */
		if (match->wskbddevcf_console != 0 && ap->console != 0)
			return (10);
		else
			return (0);
	}

	/* If console-ness unspecified, it wins. */
	return (1);
}

void
wskbd_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)self;
	struct wskbddev_attach_args *ap = aux;

	if (ap->console)
		printf(": console keyboard");
	printf("\n");

	sc->sc_accessops = ap->accessops;
	sc->sc_accesscookie = ap->accesscookie;
	sc->sc_ready = 0;				/* sanity */
	sc->sc_repeating = 0;
	sc->sc_translating = 1;

	if (ap->console) {
		KASSERT(wskbd_console_initted); 
		KASSERT(wskbd_console_device == NULL);
		wskbd_console_device = sc;
	}
	sc->sc_isconsole = ap->console;
	sc->sc_displaydv = NULL;

	/* set default bell and key repeat data */
	sc->sc_bell_data = wskbd_default_bell_data;
	sc->sc_keyrepeat_data = wskbd_default_keyrepeat_data;
}

void    
wskbd_cnattach(consops, cookie)
	const struct wskbd_consops *consops;
	void *cookie;
{

	KASSERT(!wskbd_console_initted);

	wskbd_console_ops = consops;
	wskbd_console_cookie = cookie;

	wsdisplay_set_cons_kbd(wskbd_cngetc, wskbd_cnpollc);

	wskbd_console_initted = 1;
}

static void
wskbd_repeat(v)
	void *v;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)v;
	int s = spltty();

	KASSERT(sc->sc_repeating);
	if (sc->sc_repeatstrlen != 0) {
		if (sc->sc_displaydv != NULL)
			wsdisplay_kbdinput(sc->sc_displaydv, sc->sc_repeatstr,
			    sc->sc_repeatstrlen);
		timeout(wskbd_repeat, sc,
		    (hz * sc->sc_keyrepeat_data.delN) / 1000);
	}
	splx(s);
}

void
wskbd_input(dev, type, value)
	struct device *dev;
	u_int type;
	int value;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dev; 
	struct wscons_event *ev;
	struct timeval xxxtime;
	const char *cp;
	int put;

	if (sc->sc_repeating) {
		sc->sc_repeating = 0;
		untimeout(wskbd_repeat, sc);
	}

	/*
	 * If /dev/kbd is not connected in event mode translate and
	 * send upstream.
	 */
	if (sc->sc_translating) {
		cp = (*sc->sc_accessops->translate)(sc->sc_accesscookie,
		    type, value);
		if (cp != NULL) {
			sc->sc_repeatstr = cp;
			sc->sc_repeatstrlen = strlen(cp);
			if (sc->sc_repeatstrlen != 0) {
				if (sc->sc_displaydv != NULL)
					wsdisplay_kbdinput(sc->sc_displaydv,
					    sc->sc_repeatstr,
					    sc->sc_repeatstrlen);
	
				sc->sc_repeating = 1;
				timeout(wskbd_repeat, sc,
				    (hz * sc->sc_keyrepeat_data.del1) / 1000);
			}
		}
		return;
	}

	/*
	 * Keyboard is generating events.  Turn this keystroke into an
	 * event and put it in the queue.  If the queue is full, the
	 * keystroke is lost (sorry!).
	 */

	/* no one to receive; punt!*/
	if (!sc->sc_ready)
		return;

	put = sc->sc_events.put;
	ev = &sc->sc_events.q[put];
	put = (put + 1) % WSEVENT_QSIZE;
	if (put == sc->sc_events.get) {
		log(LOG_WARNING, "%s: event queue overflow\n",
		    sc->sc_dv.dv_xname);
		return;
	}
	ev->type = type;
	ev->value = value;
	microtime(&xxxtime);
	TIMEVAL_TO_TIMESPEC(&xxxtime, &ev->time);
	sc->sc_events.put = put;
	WSEVENT_WAKEUP(&sc->sc_events);
}

void wskbd_ctlinput(dev, val)
	struct device *dev;
	int val;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dev;

	if (sc->sc_displaydv != NULL)
		wsdisplay_switch(sc->sc_displaydv, val); /* XXX XXX */
}

void
wskbd_holdscreen(dev, hold)
	struct device *dev;
	int hold;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dev;

	if (sc->sc_displaydv != NULL)
		wsdisplay_kbdholdscreen(sc->sc_displaydv, hold);
}

int
wskbdopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct wskbd_softc *sc;
	int unit;

	unit = minor(dev);
	if (unit >= wskbd_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wskbd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	if (sc->sc_events.io)			/* and that it's not in use */
		return (EBUSY);

	sc->sc_events.io = p;
	wsevent_init(&sc->sc_events);		/* may cause sleep */

	sc->sc_ready = 1;			/* start accepting events */

	/* XXX ENABLE THE DEVICE IF NOT CONSOLE? */

	return (0);
}

int
wskbdclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct wskbd_softc *sc;
	int unit;

	unit = minor(dev);
	if (unit >= wskbd_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wskbd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	/* XXX DISABLE THE DEVICE IF NOT CONSOLE? */

	sc->sc_ready = 0;			/* stop accepting events */
	wsevent_fini(&sc->sc_events);
	sc->sc_events.io = NULL;
	return (0);
}

int
wskbdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct wskbd_softc *sc;
	int unit;

	unit = minor(dev);
	if (unit >= wskbd_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wskbd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	return (wsevent_read(&sc->sc_events, uio, flags));
}

int
wskbdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct wskbd_softc *sc;
	int unit, error;

	unit = minor(dev);
	if (unit >= wskbd_cd.cd_ndevs ||	/* make sure it was attached */
	    (sc = wskbd_cd.cd_devs[unit]) == NULL)
		return (ENXIO);

	/*      
	 * Try the generic ioctls that the wskbd interface supports.
	 */
	switch (cmd) {
	case FIONBIO:		/* we will remove this someday (soon???) */
		return (0);

	case FIOASYNC:
		sc->sc_events.async = *(int *)data != 0;
		return (0);

	case TIOCSPGRP:
		if (*(int *)data != sc->sc_events.io->p_pgid)
			return (EPERM);
		return (0);
	}

	/*
	 * Try the keyboard driver for WSKBDIO ioctls.  It returns -1
	 * if it didn't recognize the request.
	 */
	error = wskbd_displayioctl((struct device *)sc, cmd, data, flag, p);
	return (error != -1 ? error : ENOTTY);
}

/*
 * WSKBDIO ioctls, handled in both emulation mode and in ``raw'' mode.
 * Some of these have no real effect in raw mode, however.
 */
int
wskbd_displayioctl(dev, cmd, data, flag, p)
	struct device *dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dev;
	struct wskbd_bell_data *ubdp, *kbdp;
	struct wskbd_keyrepeat_data *ukdp, *kkdp;
	int error;

	switch (cmd) {
#define	SETBELL(dstp, srcp, dfltp)					\
    do {								\
	(dstp)->pitch = ((srcp)->which & WSKBD_BELL_DOPITCH) ?		\
	    (srcp)->pitch : (dfltp)->pitch;				\
	(dstp)->period = ((srcp)->which & WSKBD_BELL_DOPERIOD) ?	\
	    (srcp)->period : (dfltp)->period;				\
	(dstp)->volume = ((srcp)->which & WSKBD_BELL_DOVOLUME) ?	\
	    (srcp)->volume : (dfltp)->volume;				\
	(dstp)->which = WSKBD_BELL_DOALL;				\
    } while (0)

	case WSKBDIO_BELL:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie,
		    WSKBDIO_COMPLEXBELL, (caddr_t)&sc->sc_bell_data, flag, p));

	case WSKBDIO_COMPLEXBELL:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		ubdp = (struct wskbd_bell_data *)data;
		SETBELL(ubdp, ubdp, &sc->sc_bell_data);
		return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie,
		    WSKBDIO_COMPLEXBELL, (caddr_t)ubdp, flag, p));

	case WSKBDIO_SETBELL:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		kbdp = &sc->sc_bell_data;
setbell:
		ubdp = (struct wskbd_bell_data *)data;
		SETBELL(kbdp, ubdp, kbdp);
		return (0);

	case WSKBDIO_GETBELL:
		kbdp = &sc->sc_bell_data;
getbell:
		ubdp = (struct wskbd_bell_data *)data;
		SETBELL(ubdp, kbdp, kbdp);
		return (0);

	case WSKBDIO_SETDEFAULTBELL:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		kbdp = &wskbd_default_bell_data;
		goto setbell;


	case WSKBDIO_GETDEFAULTBELL:
		kbdp = &wskbd_default_bell_data;
		goto getbell;

#undef SETBELL

#define	SETKEYREPEAT(dstp, srcp, dfltp)					\
    do {								\
	(dstp)->del1 = ((srcp)->which & WSKBD_KEYREPEAT_DODEL1) ?	\
	    (srcp)->del1 : (dfltp)->del1;				\
	(dstp)->delN = ((srcp)->which & WSKBD_KEYREPEAT_DODELN) ?	\
	    (srcp)->delN : (dfltp)->delN;				\
	(dstp)->which = WSKBD_KEYREPEAT_DOALL;				\
    } while (0)

	case WSKBDIO_SETKEYREPEAT:
		if ((flag & FWRITE) == 0)
			return (EACCES);
		kkdp = &sc->sc_keyrepeat_data;
setkeyrepeat:
		ukdp = (struct wskbd_keyrepeat_data *)data;
		SETKEYREPEAT(kkdp, ukdp, kkdp);
		return (0);

	case WSKBDIO_GETKEYREPEAT:
		kkdp = &sc->sc_keyrepeat_data;
getkeyrepeat:
		ukdp = (struct wskbd_keyrepeat_data *)data;
		SETKEYREPEAT(ukdp, kkdp, kkdp);
		return (0);

	case WSKBDIO_SETDEFAULTKEYREPEAT:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return (error);
		kkdp = &wskbd_default_keyrepeat_data;
		goto setkeyrepeat;


	case WSKBDIO_GETDEFAULTKEYREPEAT:
		kkdp = &wskbd_default_keyrepeat_data;
		goto getkeyrepeat;

#undef SETKEYREPEAT
	}

	/*
	 * Try the keyboard driver for WSKBDIO ioctls.  It returns -1
	 * if it didn't recognize the request, and in turn we return
	 * -1 if we didn't recognize the request.
	 */
/* printf("kbdaccess\n"); */
	return ((*sc->sc_accessops->ioctl)(sc->sc_accesscookie, cmd,
	    data, flag, p));
}

int
wskbdpoll(dev, events, p)
	dev_t dev;
	int events;
	struct proc *p;
{
	struct wskbd_softc *sc = wskbd_cd.cd_devs[minor(dev)];

	return (wsevent_poll(&sc->sc_events, events, p));
}

int
wskbd_is_console(dv)
	struct device *dv;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dv;

	KASSERT(sc != NULL);
	return (sc->sc_isconsole);
}

struct device *
wskbd_display(dv)
	struct device *dv;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dv;

	KASSERT(sc != NULL);
	return (sc->sc_displaydv);
}

void
wskbd_set_display(dv, displaydv)
	struct device *dv, *displaydv;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dv;

	KASSERT(sc != NULL);
	sc->sc_displaydv = displaydv;
}

void
wskbd_set_translation(dv, on)
	struct device *dv;
	int on;
{
	struct wskbd_softc *sc = (struct wskbd_softc *)dv;

	KASSERT(sc != NULL);
	sc->sc_translating = on;
}

/*
 * Console interface.
 */
int
wskbd_cngetc(dev)
	dev_t dev;
{

	if (!wskbd_console_initted)
		return 0;

	if (wskbd_console_device != NULL &&
	    !wskbd_console_device->sc_translating)
		return 0;

	return ((*wskbd_console_ops->getc)(wskbd_console_cookie));
}

void
wskbd_cnpollc(dev, poll)
	dev_t dev;
	int poll;
{

	if (!wskbd_console_initted)
		return;

	if (wskbd_console_device != NULL &&
	    !wskbd_console_device->sc_translating)
		return;

	(*wskbd_console_ops->pollc)(wskbd_console_cookie, poll);
}
