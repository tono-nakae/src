/*	$NetBSD: conf.c,v 1.9 1994/12/14 19:11:17 mycroft Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)conf.c	8.2 (Berkeley) 11/14/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>

int	rawread		__P((dev_t, struct uio *, int));
int	rawwrite	__P((dev_t, struct uio *, int));
void	swstrategy	__P((struct buf *)); /* why not include <vm/vm_extern.h>? */
int	ttselect	__P((dev_t, int, struct proc *));

#define	dev_type_open(n)	int n __P((dev_t, int, int, struct proc *))
#define	dev_type_close(n)	int n __P((dev_t, int, int, struct proc *))
#define	dev_type_strategy(n)	void n __P((struct buf *))
#define	dev_type_ioctl(n) \
	int n __P((dev_t, u_long, caddr_t, int, struct proc *))

/* bdevsw-specific types */
#define	dev_type_dump(n)	int n __P((dev_t))
#define	dev_type_size(n)	int n __P((dev_t))

#define	dev_decl(n,t)	__CONCAT(dev_type_,t)(__CONCAT(n,t))
#define	dev_init(c,n,t) \
	(c > 0 ? __CONCAT(n,t) : (__CONCAT(dev_type_,t)((*))) enxio)

/* bdevsw-specific initializations */
#define	dev_size_init(c,n)	(c > 0 ? __CONCAT(n,size) : 0)

#define	bdev_decl(n) \
	dev_decl(n,open); dev_decl(n,close); dev_decl(n,strategy); \
	dev_decl(n,ioctl); dev_decl(n,dump); dev_decl(n,size)

#define	bdev_disk_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, \
	dev_init(c,n,strategy), dev_init(c,n,ioctl), \
	dev_init(c,n,dump), dev_size_init(c,n), 0 }

#define	bdev_tape_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), \
	dev_init(c,n,strategy), dev_init(c,n,ioctl), \
	dev_init(c,n,dump), 0, B_TAPE }

#define	bdev_swap_init() { \
	(dev_type_open((*))) enodev, (dev_type_close((*))) enodev, \
	swstrategy, (dev_type_ioctl((*))) enodev, \
	(dev_type_dump((*))) enodev, 0, 0 }

#define	bdev_notdef()	bdev_tape_init(0,no)
bdev_decl(no);	/* dummy declarations */

#include "rz.h"
#include "vn.h"

bdev_decl(rz);
bdev_decl(vn);

struct bdevsw	bdevsw[] =
{
	bdev_notdef(),		/* 0: SCSI disk */
	bdev_notdef(),		/* 1: vax ht */
	bdev_disk_init(NVN,vn),	/* 2: vnode disk driver (swap to files) */
	bdev_notdef(),		/* 3: vax rk*/
	bdev_swap_init(),	/* 4: swap pseudo-device*/
	bdev_notdef(),		/* 5: vax tm */
	bdev_notdef(),		/* 6: vax ts */
	bdev_notdef(),		/* 7: vax mt */
	bdev_notdef(),		/* 8: vax tu */
	bdev_notdef(),		/* 9: ?? */
	bdev_notdef(),		/*10: ut */
	bdev_notdef(),		/*11: 11/730 idc */
	bdev_notdef(),		/*12: rx */
	bdev_notdef(),		/*13: uu */
	bdev_notdef(),		/*14: rl */
	bdev_notdef(),		/*15: tmscp */
	bdev_notdef(),		/*16: cs */
	bdev_notdef(),		/*17: md */
	bdev_notdef(),		/*18: st */
	bdev_notdef(),		/*19: sd */
	bdev_notdef(),		/*20: tz */
	bdev_disk_init(NRZ,rz),	/*21: SCSI disk */
	bdev_notdef(),		/*22: ?? */
	bdev_notdef(),		/*23: mscp */
};

int	nblkdev = sizeof (bdevsw) / sizeof (bdevsw[0]);

/* cdevsw-specific types */
#define	dev_type_read(n)	int n __P((dev_t, struct uio *, int))
#define	dev_type_write(n)	int n __P((dev_t, struct uio *, int))
#define	dev_type_stop(n)	int n __P((struct tty *, int))
#define	dev_type_reset(n)	int n __P((int))
#define	dev_type_select(n)	int n __P((dev_t, int, struct proc *))
#define	dev_type_map(n)		int n __P(())

#define	cdev_decl(n) \
	dev_decl(n,open); dev_decl(n,close); dev_decl(n,read); \
	dev_decl(n,write); dev_decl(n,ioctl); dev_decl(n,stop); \
	dev_decl(n,reset); dev_decl(n,select); dev_decl(n,map); \
	dev_decl(n,strategy); extern struct tty *__CONCAT(n,_tty)[]

#define	dev_tty_init(c,n)	(c > 0 ? __CONCAT(n,_tty) : 0)

/* open, read, write, ioctl, strategy */
#define	cdev_disk_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, \
	dev_init(c,n,strategy) }

/* open, close, read, write, ioctl, strategy */
#define	cdev_tape_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), rawread, \
	rawwrite, dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, \
	dev_init(c,n,strategy) }

/* open, close, read, write, ioctl, stop, tty */
#define	cdev_tty_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), dev_init(c,n,stop), \
	(dev_type_reset((*))) nullop, dev_tty_init(c,n), ttselect, \
	(dev_type_map((*))) enodev, 0 }

/* open, close, read, write, ioctl, select -- XXX should be a tty */
#define	cdev_cn_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, 0, dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

#define	cdev_notdef() { \
	(dev_type_open((*))) enodev, (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, \
	(dev_type_map((*))) enodev, 0 }

cdev_decl(no);			/* dummy declarations */

cdev_decl(cn);		/* console interface */

cdev_decl(ctty);
/* open, read, write, ioctl, select -- XXX should be a tty */
#define	cdev_ctty_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, 0, dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

dev_type_read(mmrw);
/* read/write */
#define	cdev_mm_init(c,n) { \
	(dev_type_open((*))) nullop, (dev_type_close((*))) nullop, mmrw, \
	mmrw, (dev_type_ioctl((*))) enodev, (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, 0 }

/* read, write, strategy */
#define	cdev_swap_init(c,n) { \
	(dev_type_open((*))) nullop, (dev_type_close((*))) nullop, rawread, \
	rawwrite, (dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, (dev_type_select((*))) enodev, \
	(dev_type_map((*))) enodev, dev_init(c,n,strategy) }

#include "pty.h"
#define	pts_tty		pt_tty
#define	ptsioctl	ptyioctl
cdev_decl(pts);
#define	ptc_tty		pt_tty
#define	ptcioctl	ptyioctl
cdev_decl(ptc);

/* open, close, read, write, ioctl, tty, select */
#define	cdev_ptc_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) nullop, \
	(dev_type_reset((*))) nullop, dev_tty_init(c,n), dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

cdev_decl(log);
/* open, close, read, ioctl, select -- XXX should be a generic device */
#define	cdev_log_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, (dev_type_reset((*))) nullop, 0, \
	dev_init(c,n,select), (dev_type_map((*))) enodev, 0 }

dev_type_open(fdopen);
/* open */
#define	cdev_fd_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) enodev, \
	(dev_type_read((*))) enodev, (dev_type_write((*))) enodev, \
	(dev_type_ioctl((*))) enodev, (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_map((*))) enodev, 0 }

#include "pm.h"
cdev_decl(pm);
#define	cdev_pm_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), \
	(dev_type_read((*))) nullop, (dev_type_write((*))) nullop, \
	dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, dev_init(c,n,select), \
	dev_init(c,n,map), 0 }

cdev_decl(rz);

#include "tz.h"
cdev_decl(tz);

cdev_decl(vn);
/* open, read, write, ioctl -- XXX should be a disk */
#define	cdev_vn_init(c,n) { \
	dev_init(c,n,open), (dev_type_close((*))) nullop, dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) nullop, 0, seltrue, (dev_type_map((*))) enodev, \
	0 }

#include "bpfilter.h"
cdev_decl(bpf);
/* open, close, read, write, ioctl, select -- XXX should be generic device */
#define	cdev_bpf_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), dev_init(c,n,read), \
	dev_init(c,n,write), dev_init(c,n,ioctl), (dev_type_stop((*))) enodev, \
	(dev_type_reset((*))) enodev, 0, dev_init(c,n,select), \
	(dev_type_map((*))) enodev, 0 }

#include "cfb.h"
cdev_decl(cfb);

#include "xcfb.h"
cdev_decl(xcfb);

#include "mfb.h"
cdev_decl(mfb);

#include "dtop.h"
cdev_decl(dtop);

#include "scc.h"
cdev_decl(scc);

#include "dc.h"
cdev_decl(dc);

struct cdevsw	cdevsw[] =
{
	cdev_cn_init(1,cn),		/* 0: virtual console */
	cdev_swap_init(1,sw),		/* 1: /dev/drum (swap pseudo-device) */
	cdev_ctty_init(1,ctty),		/* 2: controlling terminal */
	cdev_mm_init(1,mm),		/* 3: /dev/{null,mem,kmem,...} */
        cdev_tty_init(NPTY,pts),        /* 4: pseudo-tty slave */
        cdev_ptc_init(NPTY,ptc),        /* 5: pseudo-tty master */
	cdev_log_init(1,log),		/* 6: /dev/klog */
	cdev_fd_init(1,fd),		/* 7: file descriptor pseudo-dev */
	cdev_pm_init(NPM,pm),		/* 8: frame buffer */
	cdev_notdef(),			/* 9: old slot for SCSI disk */
	cdev_tape_init(NTZ,tz),		/* 10: SCSI tape */
	cdev_vn_init(NVN,vn),		/* 11: vnode disk */
	cdev_bpf_init(NBPFILTER,bpf),	/* 12: berkeley packet filter */
	cdev_pm_init(NCFB,cfb),		/* 13: color frame buffer */
	cdev_pm_init(NXCFB,xcfb),	/* 14: maxine color frame buffer */
	cdev_tty_init(NDTOP,dtop),	/* 15: desktop bus interface */
	cdev_tty_init(NDC,dc),		/* 16: dc7085 serial interface */
	cdev_tty_init(NSCC,scc),	/* 17: scc 82530 serial interface */
	cdev_pm_init(NMFB,mfb),		/* 18: mono frame buffer */
        cdev_notdef(),		        /* 19: mt */
	cdev_tty_init(NPTY,pts),	/* 20: pty slave  */
        cdev_ptc_init(NPTY,ptc),        /* 21: pty master */
	cdev_notdef(),			/* 22: dmf */
	cdev_notdef(),			/* 23: vax 730 idc */
	cdev_notdef(),			/* 24: dn-11 */

		/* 25-28 CSRG reserved to local sites, DEC sez: */
	cdev_notdef(),		/* 25: gpib */
	cdev_notdef(),		/* 26: lpa */
	cdev_notdef(),		/* 27: psi */
	cdev_notdef(),		/* 28: ib */
	cdev_notdef(),		/* 29: ad */
	cdev_notdef(),		/* 30: rx */
	cdev_notdef(),		/* 31: ik */
	cdev_notdef(),		/* 32: rl-11 */
	cdev_notdef(),		/* 33: dhu/dhv */
	cdev_notdef(),		/* 34: Vax Able dmz, mips dc  */
	cdev_notdef(),		/* 35: qv */
	cdev_notdef(),		/* 36: tmscp */
	cdev_notdef(),		/* 37: vs */
	cdev_notdef(),		/* 38: vax cn console */
	cdev_notdef(),		/* 39: lta */
	cdev_notdef(),		/* 40: crl (Venus, aka 8600 aka 11/790 console RL02) */
	cdev_notdef(),		/* 41: cs */
	cdev_notdef(),		/* 42: qd, Qdss, vcb02 */
	cdev_mm_init(1,mm),	/* 43: errlog (VMS-lookalike puke) */
	cdev_notdef(),		/* 44: dmb */
	cdev_notdef(),		/* 45:  vax ss, mips scc */
	cdev_notdef(),		/* 46: st */
	cdev_notdef(),		/* 47: sd */
	cdev_notdef(),		/* 48: Ultrix /dev/trace */
	cdev_notdef(),		/* 49: sm (sysV shm?) */
	cdev_notdef(),		/* 50 sg */
	cdev_notdef(),		/* 51: sh tty */
	cdev_notdef(),		/* 52: its */
	cdev_notdef(),		/* 53: nodev */
	cdev_notdef(),		/* 54: nodev */
	cdev_notdef(),		/* 55: tz */
	cdev_disk_init(NRZ,rz), /* 56: rz scsi, Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 57: nodev */
	cdev_notdef(),		/* 58: fc */
	cdev_notdef(),		/* 59: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 60: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 61: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 62: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 63: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 64: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 65: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 66: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 67: mscp, again Ultrix gross coupling to PrestoServe driver */
	cdev_notdef(),		/* 68: ld */
	cdev_notdef(),		/* 69: /dev/audit */
	cdev_notdef(),		/* 70: Mogul (nee' CMU) packetfilter */
	cdev_notdef(),		/* 71: xcons, mips Ultrix /dev/xcons virtual console nightmare */
	cdev_notdef(),		/* 72: xa */
	cdev_notdef(),		/* 73: utx */
	cdev_notdef(),		/* 74: sp */
	cdev_notdef(),		/* 75: pr Ultrix PrestoServe NVRAM pseudo-device control device */
	cdev_notdef(),		/* 76: ultrix disk shadowing */
	cdev_notdef(),		/* 77: ek */
	cdev_notdef(),		/* 78: msdup ? */
	cdev_notdef(),		/* 79: so-called multimedia audio A */
	cdev_notdef(),		/* 80: so-called multimedia audio B */
	cdev_notdef(),		/* 81: so-called multimedia video in */
	cdev_notdef(),		/* 82: so-called multimedia video out */
	cdev_notdef(),		/* 83: fd */
	cdev_notdef(),		/* 84: DTi */
};

int	nchrdev = sizeof (cdevsw) / sizeof (cdevsw[0]);

int	mem_no = 2; 	/* major device number of memory special file */

/*
 * Swapdev is a fake device implemented
 * in sw.c used only internally to get to swstrategy.
 * It cannot be provided to the users, because the
 * swstrategy routine munches the b_dev and b_blkno entries
 * before calling the appropriate driver.  This would horribly
 * confuse, e.g. the hashing routines. Instead, /dev/drum is
 * provided as a character (raw) device.
 */
dev_t	swapdev = makedev(1, 0);

/*
 * Routine that identifies /dev/mem and /dev/kmem.
 *
 * A minimal stub routine can always return 0.
 */
iskmemdev(dev)
	dev_t dev;
{

#ifdef COMPAT_BSD44
	if (major(dev) == 2 && (minor(dev) == 0 || minor(dev) == 1))
#else
	if (major(dev) == 3 && (minor(dev) == 0 || minor(dev) == 1))
#endif
		return (1);
	return (0);
}

iszerodev(dev)
	dev_t dev;
{
#ifdef COMPAT_BSD44
	return (major(dev) == 2 && minor(dev) == 12);
#else
	return (major(dev) == 3 && minor(dev) == 12);
#endif
}

/*
 * Routine to determine if a device is a disk.
 *
 * A minimal stub routine can always return 0.
 */
isdisk(dev, type)
	dev_t dev;
	int type;
{

	switch (major(dev)) {
	case 21: /* NetBSD rz */
#ifdef COMPAT_BSD44
	case 0: /*4.4bsd rz */
#endif
	case 2:	/* vnode disk */
		if (type == VBLK)
			return (1);
		return (0);
	case 56:/* NetBSD rz */
#ifdef COMPAT_BSD44
	case 9: /*4.4bsd rz*/
#endif
	case 11:	/* vnode disk */
		if (type == VCHR)
			return (1);
		/* FALLTHROUGH */
	default:
		return (0);
	}
	/* NOTREACHED */
}

#define MAXDEV	60
static int chrtoblktbl[MAXDEV] =  {
      /* VCHR */      /* VBLK */
	/* 0 */		NODEV,
	/* 1 */		NODEV,
	/* 2 */		NODEV,
	/* 3 */		NODEV,
	/* 4 */		NODEV,
	/* 5 */		NODEV,
	/* 6 */		NODEV,
	/* 7 */		NODEV,
	/* 8 */		NODEV,
	/* 9 */		0,
	/* 10 */	NODEV,
	/* 11 */	NODEV,
	/* 12 */	NODEV,
	/* 13 */	NODEV,
	/* 14 */	NODEV,
	/* 15 */	NODEV,
	/* 16 */	NODEV,
	/* 17 */	NODEV,
	/* 18 */	NODEV,
	/* 19 */	NODEV,
	/* 20 */	NODEV,
	/* 21 */	NODEV,
	/* 22 */	NODEV,
	/* 23 */	NODEV,
	/* 24 */	NODEV,
	/* 25 */	NODEV,
	/* 26 */	NODEV,
	/* 27 */	NODEV,
	/* 28 */	NODEV,
	/* 29 */	NODEV,
	/* 30 */	NODEV,
	/* 31 */	NODEV,
	/* 32 */	NODEV,
	/* 33 */	NODEV,
	/* 34 */	NODEV,
	/* 35 */	NODEV,
	/* 36 */	NODEV,
	/* 37 */	NODEV,
	/* 38 */	NODEV,
	/* 39 */	NODEV,
	/* 40 */	NODEV,
	/* 41 */	NODEV,
	/* 42 */	NODEV,
	/* 43 */	NODEV,
	/* 44 */	NODEV,
	/* 45 */	NODEV,
	/* 46 */	NODEV,
	/* 47 */	NODEV,
	/* 48 */	NODEV,
	/* 49 */	NODEV,
	/* 50 */	NODEV,
	/* 51 */	NODEV,
	/* 52 */	NODEV,
	/* 53 */	NODEV,
	/* 54 */	NODEV,
	/* 55 */	NODEV,
	/* 56 */	21,
	/* 57 */	NODEV,
	/* 58 */	NODEV,
	/* 59 */	NODEV,
};
/*
 * Routine to convert from character to block device number.
 *
 * A minimal stub routine can always return NODEV.
 */
chrtoblk(dev)
	dev_t dev;
{
	int blkmaj;

	if (major(dev) >= MAXDEV || (blkmaj = chrtoblktbl[major(dev)]) == NODEV)
		return (NODEV);
	return (makedev(blkmaj, minor(dev)));
}
