/*	$NetBSD: setterm.c,v 1.29 2001/12/02 09:14:23 blymn Exp $	*/

/*
 * Copyright (c) 1981, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)setterm.c	8.8 (Berkeley) 10/25/94";
#else
__RCSID("$NetBSD: setterm.c,v 1.29 2001/12/02 09:14:23 blymn Exp $");
#endif
#endif /* not lint */

#include <sys/ioctl.h>		/* TIOCGWINSZ on old systems. */

#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "curses.h"
#include "curses_private.h"

static int zap(SCREEN *screen);

struct tinfo *_cursesi_genbuf;

static char	*sflags[] = {
/*	      am        bs        cc        da        eo */
	 &__tc_am, &__tc_bs, &__tc_cc, &__tc_da, &__tc_eo,
/*	      hc        hl        in        mi        ms */
	&__tc_hc, &__tc_hl, &__tc_in, &__tc_mi, &__tc_ms,
/*	      nc        ns        os        ul        ut */
	&__tc_nc, &__tc_ns, &__tc_os, &__tc_ul, &__tc_ut,
/*	      xb        xn        xt        xs        xx */
	&__tc_xb, &__tc_xn, &__tc_xt, &__tc_xs, &__tc_xx
};

static int	*svals[] = {
/*	      pa        Co        NC */
	&__tc_pa, &__tc_Co, &__tc_NC
};

static char	*_PC,
	**sstrs[] = {
	/*	      AB        ac        ae        AF        AL */
		&__tc_AB, &__tc_ac, &__tc_ae, &__tc_AF, &__tc_AL,
	/*	      al        as        bc        bl        bt */
		&__tc_al, &__tc_as, &__tc_bc, &__tc_bl, &__tc_bt,
	/*	      cd        ce        cl        cm        cr */
		&__tc_cd, &__tc_ce, &__tc_cl, &__tc_cm, &__tc_cr,
	/*	      cs        dc        DL        dl        dm */
		&__tc_cs, &__tc_dc, &__tc_DL, &__tc_dl, &__tc_dm,
	/*	      DO        do        eA        ed        ei */
		&__tc_DO, &__tc_do, &__tc_eA, &__tc_ed, &__tc_ei,
	/*	      ho        Ic        ic        im        Ip */
		&__tc_ho, &__tc_Ic, &__tc_ic, &__tc_im, &__tc_Ip,
	/*	      ip        k0        k1        k2        k3 */
		&__tc_ip, &__tc_k0, &__tc_k1, &__tc_k2, &__tc_k3,
	/*	      k4        k5        k6        k7        k8 */
		&__tc_k4, &__tc_k5, &__tc_k6, &__tc_k7, &__tc_k8,
	/*	      k9        kd        ke        kh        kl */
		&__tc_k9, &__tc_kd, &__tc_ke, &__tc_kh, &__tc_kl,
	/*	      kr        ks        ku        LE        ll */
		&__tc_kr, &__tc_ks, &__tc_ku, &__tc_LE, &__tc_ll,
	/*	      ma        mb        md        me        mh */
		&__tc_ma, &__tc_mb, &__tc_md, &__tc_me, &__tc_mh,
	/*	      mk        mm        mo        mp        mr */
		&__tc_mk, &__tc_mm, &__tc_mo, &__tc_mp, &__tc_mr,
	/*	      nd        nl        oc        op    pc     */
		&__tc_nd, &__tc_nl, &__tc_oc, &__tc_op, &_PC,
	/*	      rc        RI        sc        Sb        se */
		&__tc_rc, &__tc_RI, &__tc_Sb, &__tc_sc, &__tc_se,
	/*	      SF        Sf        sf        so        sp */
		&__tc_SF, &__tc_Sf, &__tc_sf, &__tc_so, &__tc_sp,
	/*	      SR        sr        ta        te        ti */
		&__tc_SR, &__tc_sr, &__tc_ta, &__tc_te, &__tc_ti,
	/*	      uc        ue        UP        up        us */
		&__tc_uc, &__tc_ue, &__tc_UP, &__tc_up, &__tc_us,
	/*	      vb        ve        vi        vs           */
		&__tc_vb, &__tc_ve, &__tc_vi, &__tc_vs
	};

attr_t	 __mask_op, __mask_me, __mask_ue, __mask_se;

int
setterm(char *type)
{
	return _cursesi_setterm(type, _cursesi_screen);
}

int
_cursesi_setterm(char *type, SCREEN *screen)
{
	static char cm_buff[1024], tc[1024], *tcptr;
	int unknown;
	size_t limit;
	struct winsize win;
	char *p;

#ifdef DEBUG
	__CTRACE("setterm: (\"%s\")\nLINES = %d, COLS = %d\n",
	    type, LINES, COLS);
#endif
	if (type[0] == '\0')
		type = "xx";
	unknown = 0;
	if (t_getent(&screen->cursesi_genbuf, type) != 1) {
		unknown++;
	}
#ifdef DEBUG
	__CTRACE("setterm: tty = %s\n", type);
#endif

	/* Try TIOCGWINSZ, and, if it fails, the termcap entry. */
	if (ioctl(fileno(screen->outfd), TIOCGWINSZ, &win) != -1 &&
	    win.ws_row != 0 && win.ws_col != 0) {
		screen->LINES = win.ws_row;
		screen->COLS = win.ws_col;
	}  else {
		if (unknown) {
			screen->LINES = -1;
			screen->COLS = -1;
		} else {
			screen->LINES = t_getnum(screen->cursesi_genbuf, "li");
			screen->COLS = t_getnum(screen->cursesi_genbuf, "co");
		}
		
	}

	/* POSIX 1003.2 requires that the environment override. */
	if ((p = getenv("LINES")) != NULL)
		screen->LINES = (int) strtol(p, NULL, 10);
	if ((p = getenv("COLUMNS")) != NULL)
		screen->COLS = (int) strtol(p, NULL, 10);

	/*
	 * Want cols > 4, otherwise things will fail.
	 */
	if (screen->COLS <= 4)
		return (ERR);

#ifdef DEBUG
	__CTRACE("setterm: LINES = %d, COLS = %d\n", LINES, COLS);
#endif
	if (!unknown) {
		if (zap(screen) == ERR) /* Get terminal description.*/
			return ERR;
	}
	
	/* If we can't tab, we can't backtab, either. */
	if (!screen->GT)
		screen->tc_bt = NULL;

	/*
	 * Test for cursor motion capability.
	 *
	 */
	if (t_goto(NULL, screen->tc_cm, 0, 0, cm_buff, 1023) < 0) {
		screen->CA = 0;
		screen->tc_cm = 0;
	} else
		screen->CA = 1;

        /*
	 * set the pad char, only take the first char of the pc capability
	 * as this is all we can use.
	 */
	screen->pad_char = screen->tc_pc ? screen->tc_pc[0] : 0; 
	
	if (unknown) {
		strcpy(screen->ttytype, "dumb");
	} else {
		tcptr = tc;
		limit = 1023;
		if (t_getterm(screen->cursesi_genbuf, &tcptr, &limit) < 0)
			return ERR;
		__longname(tc, screen->ttytype);
	}
	
	/* If no scrolling commands, no quick change. */
	screen->noqch =
  	    (screen->tc_cs == NULL || screen->tc_ho == NULL ||
	    (screen->tc_SF == NULL && screen->tc_sf == NULL) ||
	     (screen->tc_SR == NULL && screen->tc_sr == NULL)) &&
	    ((screen->tc_AL == NULL && screen->tc_al == NULL) ||
	     (screen->tc_DL == NULL && screen->tc_dl == NULL));

	/* Precalculate conflict info for color/attribute end commands. */
	screen->mask_op = __ATTRIBUTES & ~__COLOR;
	if (screen->tc_op != NULL) {
		if (screen->tc_se != NULL &&
		    !strcmp(screen->tc_op, screen->tc_se))
			screen->mask_op &= ~__STANDOUT;
		if (screen->tc_ue != NULL &&
		    !strcmp(screen->tc_op, screen->tc_ue))
			screen->mask_op &= ~__UNDERSCORE;
		if (screen->tc_me != NULL &&
		    !strcmp(screen->tc_op, screen->tc_me))
			screen->mask_op &= ~__TERMATTR;
	}
	screen->mask_me = __ATTRIBUTES & ~__TERMATTR;
	if (screen->tc_me != NULL) {
		if (screen->tc_se != NULL &&
		    !strcmp(screen->tc_me, screen->tc_se))
			screen->mask_me &= ~__STANDOUT;
		if (screen->tc_ue != NULL &&
		    !strcmp(screen->tc_me, screen->tc_ue))
			screen->mask_me &= ~__UNDERSCORE;
		if (screen->tc_op != NULL &&
		    !strcmp(screen->tc_me, screen->tc_op))
			screen->mask_me &= ~__COLOR;
	}
	screen->mask_ue = __ATTRIBUTES & ~__UNDERSCORE;
	if (screen->tc_ue != NULL) {
		if (screen->tc_se != NULL &&
		    !strcmp(screen->tc_ue, screen->tc_se))
			screen->mask_ue &= ~__STANDOUT;
		if (screen->tc_me != NULL &&
		    !strcmp(screen->tc_ue, screen->tc_me))
			screen->mask_ue &= ~__TERMATTR;
		if (screen->tc_op != NULL &&
		    !strcmp(screen->tc_ue, screen->tc_op))
			screen->mask_ue &= ~__COLOR;
	}
	screen->mask_se = __ATTRIBUTES & ~__STANDOUT;
	if (screen->tc_se != NULL) {
		if (screen->tc_ue != NULL &&
		    !strcmp(screen->tc_se, screen->tc_ue))
			screen->mask_se &= ~__UNDERSCORE;
		if (screen->tc_me != NULL &&
		    !strcmp(screen->tc_se, screen->tc_me))
			screen->mask_se &= ~__TERMATTR;
		if (screen->tc_op != NULL &&
		    !strcmp(screen->tc_se, screen->tc_op))
			screen->mask_se &= ~__COLOR;
	}

	return (unknown ? ERR : OK);
}

/*
 * _cursesi_resetterm --
 *  Copy the terminal instance data for the given screen to the global
 *  variables.
 */
void
_cursesi_resetterm(SCREEN *screen)
{
	char ***sp, **scr_sp;
	int  **vp, *scr_vp, i;
	char **fp, *scr_fp;

	LINES = screen->LINES;
	COLS = screen->COLS;

	  /* reset terminal capabilities */
	fp = sflags;
	scr_fp = &screen->tc_am;
	for (i = 0; i < screen->flag_count; i++)
		*(*fp++) = *scr_fp++;

	vp = svals;
	scr_vp = &screen->tc_pa;
	for (i = 0; i < screen->int_count; i++)
		*(*vp++) = *scr_vp++;

	sp = sstrs;
	scr_sp = &screen->tc_AB;
	for (i = 0; i < screen->str_count; i++)
		*(*sp++) = *scr_sp++;
		
	__GT = screen->GT;
	__CA = screen->CA;

	PC = screen->pad_char;
	
	__noqch = screen->noqch;
	__mask_op = screen->mask_op;
	__mask_me = screen->mask_me;
	__mask_ue = screen->mask_ue;
	__mask_se = screen->mask_se;
}

/*
 * zap --
 *	Gets all the terminal flags from the termcap database.
 */
static int
zap(SCREEN *screen)
{
	const char *nampstr, *namp;
        char **sp;
	int  *vp;
	char *fp;
	char tmp[3];
#ifdef DEBUG
	char	*cp;
#endif
	tmp[2] = '\0';

	namp = "ambsccdaeohchlinmimsncnsosulutxbxnxtxsxx";
	fp = &screen->tc_am;
	screen->flag_count = 0;
	do {
		*tmp = *namp;
		*(tmp + 1) = *(namp + 1);
		*fp++ = t_getflag(screen->cursesi_genbuf, tmp);
#ifdef DEBUG
		__CTRACE("%2.2s = %s\n", namp, fp[-1] ? "TRUE" : "FALSE");
#endif
		namp += 2;
		screen->flag_count++;
	} while (*namp);
	namp = "paCoNC";
	vp = &screen->tc_pa;
	screen->int_count = 0;
	do {
		*tmp = *namp;
		*(tmp + 1) = *(namp + 1);
		*vp++ = t_getnum(screen->cursesi_genbuf, tmp);
#ifdef DEBUG
		__CTRACE("%2.2s = %d\n", namp, *vp[-1]);
#endif
		namp += 2;
		screen->int_count++;
	} while (*namp);

	  /* calculate the size of tspace.... */
  	nampstr = "ABacaeAFALalasbcblbtcdceclcmcrcsdcDLdldmDOdoeAedeihoIcicimIpipk0k1k2k3k4k5k6k7k8k9kdkekhklkrkskuLEllmambmdmemhmkmmmompmrndnlocoppcrcRISbscseSFSfsfsospSRsrtatetiucueUPupusvbvevivs";
/*  	namp = nampstr; */
/*  	tspace_size = 0; */
/*  	do { */
/*  		*tmp = *namp; */
/*  		*(tmp + 1) = *(namp + 1); */
/*  		t_getstr(tinfo, tmp, NULL, &i); */
/*  		tspace_size += i + 1; */
/*  		namp += 2; */
/*  	} while (*namp); */

/*  	if ((screen->tspace = (char *) malloc(tspace_size)) == NULL) */
/*  		return ERR; */
/*  #ifdef DEBUG */
/*  	__CTRACE("Allocated %d (0x%x) size buffer for tspace\n", tspace_size, */
/*  		 tspace_size); */
/*  #endif */
/*  	screen->aoftspace = screen->tspace; */
	
	namp = nampstr;
	sp = &screen->tc_AB;
	screen->str_count = 0;
	do {
		*tmp = *namp;
		*(tmp + 1) = *(namp + 1);
		*sp++ = t_agetstr(screen->cursesi_genbuf, tmp);
#ifdef DEBUG
		__CTRACE("%2.2s = %s", namp, *sp[-1] == NULL ? "NULL\n" : "\"");
		if (sp[-1] != NULL) {
			for (cp = sp[-1]; *cp; cp++)
				__CTRACE("%s", unctrl(*cp));
			__CTRACE("\"\n");
		}
#endif
		namp += 2;
		screen->str_count++;
	} while (*namp);
	if (screen->tc_xs)
		screen->tc_so = screen->tc_se = NULL;
	else {
		if (t_getnum(screen->cursesi_genbuf, "sg") > 0)
			screen->tc_so = NULL;
		if (t_getnum(screen->cursesi_genbuf, "ug") > 0)
			screen->tc_us = NULL;
		if (!screen->tc_so && screen->tc_us) {
			screen->tc_so = screen->tc_us;
			screen->tc_se = screen->tc_ue;
		}
	}

	return OK;
}

/*
 * getcap --
 *	Return a capability from termcap.
 */
char	*
getcap(char *name)
{
	return (t_agetstr(_cursesi_genbuf, name));
}
