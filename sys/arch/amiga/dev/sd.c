/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	@(#)sd.c	7.8 (Berkeley) 6/9/91
 *	$Id: sd.c,v 1.14 1994/05/08 05:53:43 chopps Exp $
 */

/*
 * SCSI CCS (Command Command Set) disk driver.
 */
#include "sd.h"
#if NSD > 0

/*
 * Current driver can only handle 8 disk devices, so if more were
 * configured, redefine NSD to reduce unusable data space.
 */

#if NSD > 8
#undef NSD
#define NSD	8	/* Can't handle more than 8 devices */
#endif


#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/disklabel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/file.h>

#include <amiga/dev/device.h>
#include <amiga/dev/scsidefs.h>
#include <vm/vm_param.h>
#include <vm/lock.h>
#include <vm/pmap.h>
#include <vm/vm_prot.h>

#include <amiga/dev/rdb.h>

struct sd_softc;

int sdinit __P((struct amiga_device *ad));
int sdopen __P((dev_t, int, int, struct proc *));
int sdioctl __P((dev_t, int, caddr_t, int, struct proc *));
int sdsize __P((dev_t));
int sddump __P((dev_t));
int scsi_unlock_floptical __P((int, int, int));
void sdreset __P((struct sd_softc *, struct amiga_device *));
void sdstrategy __P((struct buf *));
void sdustart __P((int));
void sdstart __P((int));
void sdgo __P((int));
void sdintr __P((int, int));

static int sdident __P((struct sd_softc *, struct amiga_device *));
static int sderror __P((int, struct sd_softc *, struct amiga_device *, int));
static void sdlblkstrat __P((struct buf *, int));
static void sdfinish __P((int, struct sd_softc *, struct buf *));

extern void disksort();
extern int physio();
extern void TBIS();

struct	driver sddriver = {
	(int (*)(void *))sdinit, "sd", (int (*)(int)) sdstart, (int (*)(int,...))sdgo,
	(int (*)(int,int)) sdintr, 0,
};

struct sd_softc {
	struct device dev;
	struct scsibus_softc *sc_scsibus;
	struct devqueue sc_dq;
	int sc_format_pid;	/* process using "format" mode */
	short sc_flags;
	short sc_type;	/* drive type */
	short sc_punit;	/* physical unit (scsi lun) */
	u_short	sc_bshift;	/* convert device blocks to DEV_BSIZE blks */
	u_int	sc_blks;	/* number of blocks on device */
	int	sc_blksize;	/* device block size in bytes */
	u_int	sc_wpms;	/* average xfer rate in 16 bit wds/sec. */
	char	sc_idstr[32];	/* XXX */
	struct  disklabel sc_label;  /* drive partition table & label info */
	int	sc_have_label;
	int	sc_write_label;
};
struct sd_softc sd_softc[NSD];

/* sc_flags values */
#define	SDF_ALIVE	0x1

#ifdef DEBUG
int sddebug = 0;
#define SDB_ERROR	0x01
#define SDB_PARTIAL	0x02
#define SDB_BOOTDEV	0x04
#define SDB_QUASEL	0x10
#define QUASEL
#endif

#ifdef QUASEL
#define QPRINTF(a) if (sddebug & SDB_QUASEL) printf a
#else
#define QPRINTF(a)
#endif

struct sdstats {
	long	sdresets;
	long	sdtransfers;
	long	sdpartials;
};
struct sdstats sdstats[NSD];

struct	buf sdtab[NSD];
struct	scsi_fmt_cdb sdcmd[NSD];
struct	scsi_fmt_sense sdsense[NSD];

static struct scsi_fmt_cdb sd_read_cmd = { 10, CMD_READ_EXT };
static struct scsi_fmt_cdb sd_write_cmd = { 10, CMD_WRITE_EXT };

#define	sdunit(x)	(minor (x) >> 3 & 0x7)
#define sdpart(x)	(minor (x) & 0x7 | minor (x) >> 3 & 0x18)
#define	sdpunit(x)	((x) & 7)
#define	b_cylin		b_resid

#define	SDRETRY		2

/*
 * Table of scsi commands users are allowed to access via "format"
 * mode.  0 means not legal.  1 means "immediate" (doesn't need dma).
 * -1 means needs dma and/or wait for intr.
 */
static char legal_cmds[256] = {
/*****  0   1   2   3   4   5   6   7     8   9   A   B   C   D   E   F */
/*00*/	0,  0,  0,  0, -1,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*10*/	0,  0,  1,  0,  0,  1,  0,  0,    0,  0,  1,  0,  0,  0,  0,  0,
/*20*/	0,  0,  0,  0,  0,  1,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*30*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*40*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*50*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*60*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*70*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*80*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*90*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*a0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*b0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*c0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*d0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*e0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*f0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
};

static struct scsi_inquiry inqbuf;
static struct scsi_fmt_cdb inq = {
	6,
	CMD_INQUIRY, 0, 0, 0, sizeof(inqbuf), 0
};

static u_char capbuf[8];
struct scsi_fmt_cdb cap = {
	10,
	CMD_READ_CAPACITY, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int
sdident(sc)
	struct sd_softc *sc;
{
	struct scsibus_softc *sbsc;
	int unit, stat;
	int i;
	int tries = 10;
	int ismo = 0;

	sbsc = sc->sc_scsibus;
	unit = sc->sc_punit;
	scsidelay(-1);

	/*
	 * See if unit exists and is a disk then read block size & nblocks.
	 */
	while ((i = scsitur(sbsc, unit)) != 0) {
retry_TUR:
		if (i == -1 || --tries < 0) {
			if (ismo)
				break;
			/* doesn't exist or not a CCS device */
			goto failed;
		}
		if (i == STS_CHECKCOND) {
			u_char sensebuf[128];
			struct scsi_xsense *sp = (struct scsi_xsense *)sensebuf;

			i = scsirqs(sbsc, unit, sensebuf, sizeof(sensebuf));
			if (sp->class == 7)
				switch (sp->key) {
				/* not ready -- might be MO with no media */
				case 2:
					if (sp->len == 12 &&
					    sensebuf[12] == 10)	/* XXX */
						ismo = 1;
					break;
				/* drive doing an RTZ -- give it a while */
				case 6:
					DELAY(1000000);
					break;
				default:
					break;
				}
		}
		DELAY(1000);
	}
	/*
	 * Find out about device
	 */
	if (i = scsiicmd(sbsc, unit, &inq, &inqbuf, sizeof(inqbuf), B_READ)) {
		if (i == STS_CHECKCOND)
			goto retry_TUR;
		goto failed;
	}
	switch (inqbuf.type) {
	case 0:		/* disk */
	case 4:		/* WORM */
	case 5:		/* CD-ROM */
	case 7:		/* Magneto-optical */
		break;
	default:	/* not a disk */
#ifdef DEBUG
		if (sddebug)
		  printf ("Unit %d unknown scsi-type: %d\n", slave, inqbuf.type);
#endif
		goto failed;
	}
	/*
	 * Get a usable id string
	 */
	if (inqbuf.version < 1) {
		bcopy("UNKNOWN", &sc->sc_idstr[0], 8);
		bcopy("DRIVE TYPE", &sc->sc_idstr[8], 11);
	} else {
		bcopy((caddr_t)&inqbuf.vendor_id, (caddr_t)sc->sc_idstr, 28);
		for (i = 27; i > 23; --i)
			if (sc->sc_idstr[i] != ' ')
				break;
		sc->sc_idstr[i+1] = 0;
		for (i = 23; i > 7; --i)
			if (sc->sc_idstr[i] != ' ')
				break;
		sc->sc_idstr[i+1] = 0;
		for (i = 7; i >= 0; --i)
			if (sc->sc_idstr[i] != ' ')
				break;
		sc->sc_idstr[i+1] = 0;
	}

	/* for those that have drives they only turn on for use under BSD... */
	(ad->amiga_cdriver->d_ssu)(ctlr, slave, unit, 1);

	i = (ad->amiga_cdriver->d_immcmd)(ctlr, slave, unit, &cap, 
			       (u_char *)&capbuf, sizeof(capbuf), B_READ);
	if (i) {
		if (i != STS_CHECKCOND ||
		    bcmp(&sc->sc_idstr[0], "HP", 3) ||
		    bcmp(&sc->sc_idstr[8], "S6300.650A", 11))
			goto failed;
		/* XXX unformatted or non-existant MO media; fake it */
		sc->sc_blks = 318664;
		sc->sc_blksize = 1024;
	} else {
		sc->sc_blks = *(u_int *)&capbuf[0];
		sc->sc_blksize = *(int *)&capbuf[4];
	}
	/* return value of read capacity is last valid block number */
	sc->sc_blks++;

	if (inqbuf.version < 1)
		printf("sd%d: type 0x%x, qual 0x%x, ver %d", ad->amiga_unit,
			inqbuf.type, inqbuf.qual, inqbuf.version);
	else
		printf("sd%d: %s %s rev %s", ad->amiga_unit, sc->sc_idstr, &sc->sc_idstr[8],
			&sc->sc_idstr[24]);
	printf(", %d %d byte blocks\n", sc->sc_blks, sc->sc_blksize);
	if (sc->sc_blksize != DEV_BSIZE) {
		if (sc->sc_blksize < DEV_BSIZE) {
			printf("sd%d: need %d byte blocks - drive ignored\n",
				unit, DEV_BSIZE);
			goto failed;
		}
		for (i = sc->sc_blksize; i > DEV_BSIZE; i >>= 1)
			++sc->sc_bshift;
		sc->sc_blks <<= sc->sc_bshift;
	}
	sc->sc_wpms = 32 * (60 * DEV_BSIZE / 2);	/* XXX */
	 
 	if (!bcmp(&sc->sc_idstr[0],"IOMEGA",6) &&
 	    !bcmp(&sc->sc_idstr[8],"Io20S",5)) {
 		int stat;
		/* unlock FLOPTICAL */
#if defined (DAIGNOSTIC)
 		printf("Floptical drive detected, trying to unlock...");
#endif
 		stat = scsi_unlock_floptical(ctlr,slave,unit);
#if defined (DAIGNOSTIC)
 		if(!stat) 
 			printf("success.\n");
 		else {
 			printf("unlock failed: %d\n",stat);
 			sderror(unit,sc,sc->sc_ad,stat);
		}
#endif
 	}
	
	(ad->amiga_cdriver->d_delay)(0);
	return(inqbuf.type);
failed:
	(ad->amiga_cdriver->d_delay)(0);
	return(-1);
}

int
sdinit(ad)
	register struct amiga_device *ad;
{
	register struct sd_softc *sc = &sd_softc[ad->amiga_unit];
	static u_int block [DEV_BSIZE / 4];
	struct RigidDiskBlock *rdb = (struct RigidDiskBlock *) block;
	struct PartitionBlock *pb  = (struct PartitionBlock *) block;
	int bnum, npbnum, num_bsd_part = 0, num_part = 0, non_bsd_part = 8;
	struct disklabel *dl;
	int s;

	if (sc->sc_flags & SDF_ALIVE)
		return (0);		/* this unit already configured */
	/* I seem to be interrupted every leap year while in here..
	   
	   Note: do this *only* if you don't need interrupt driven
	         SCSI in here. We currently don't, as all I/O in here
		 is done with PIO, but beware.. */
	s = splbio();

	sc->sc_ad = ad;
	sc->sc_punit = sdpunit(ad->amiga_flags);
	sc->sc_type = sdident(sc, ad);
	if (sc->sc_type < 0)
	  {
	    splx (s);
	    return(0);
	  }
	sc->sc_dq.dq_ctlr = ad->amiga_ctlr;
	sc->sc_dq.dq_unit = ad->amiga_unit;
	sc->sc_dq.dq_slave = ad->amiga_slave;
	sc->sc_dq.dq_driver = &sddriver;

	/* fill in as much as we can of the disklabel */
	dl = &sc->sc_label;
	bzero (dl, sizeof (*dl));
	dl->d_magic = DISKMAGIC;
	dl->d_type  = DTYPE_SCSI;

	strncpy (dl->d_typename, sc->sc_idstr, sizeof (dl->d_typename) - 1);
	strcpy (dl->d_packname, "some pack");
	
	/* if drive doesn't use DEV_BSIZE blocks, it's (probably) not an amiga
	   drive, but a CDROM, for example. In that case, just set up one
	   partition, C, containing the whole disk. */
	if (sc->sc_bshift != 0)
	  {
no_rdb:
	    dl->d_magic2 = DISKMAGIC;
	    /* A and B come out offset=0, size=0. That way, we keep the 
	       meaning of C being the whole drive, even if there's no A and
	       B partition */
	    dl->d_partitions[2].p_offset = 0;
	    dl->d_partitions[2].p_size = sc->sc_blks;
	    dl->d_partitions[2].p_fstype = FS_UNUSED;
	    dl->d_npartitions = 3; /* just #2 used */
	    goto do_chksum;
	  }

	/* read RDB and partition blocks to find out about which partitions
	   are ok to be scribbled upon by BSD. This is quite Amiga specific. */

	for (bnum = 0; bnum < RDB_LOCATION_LIMIT; bnum++)
	  {
	    if ((ad->amiga_cdriver->d_ttread) (ad->amiga_ctlr, ad->amiga_slave, sc->sc_punit,
			      (char *) block, DEV_BSIZE, bnum, sc->sc_bshift))
	      /* read error on block */
	      goto no_rdb;

	    if (rdb->rdb_ID == IDNAME_RIGIDDISK)
	      break;
	  }
	if (bnum == RDB_LOCATION_LIMIT)
	  /* no RDB on this disk */
	  goto no_rdb;

	/* now for more serious information.. */
	dl->d_secsize	 = rdb->rdb_BlockBytes;
	dl->d_nsectors   = rdb->rdb_Sectors;
	dl->d_ntracks    = rdb->rdb_Heads;
	dl->d_ncylinders = rdb->rdb_Cylinders;
	dl->d_secpercyl  = rdb->rdb_CylBlocks;
	dl->d_secperunit = (rdb->rdb_CylBlocks 
			    * (rdb->rdb_HiCylinder - rdb->rdb_LoCylinder));

	/* makes sense? */
	dl->d_acylinders = rdb->rdb_Cylinders - (rdb->rdb_HiCylinder - rdb->rdb_LoCylinder);

	dl->d_magic2 = DISKMAGIC;

	/* ok, go thru the partition blocks, and check for BSD ID's */
	
	for (npbnum = rdb->rdb_PartitionList; npbnum != 0xffffffff; )
	  {
	    u_int start_block, end_block, nblocks;
	    u_int reserved;
	    int part, bsd_part;

	    if ((ad->amiga_cdriver->d_ttread) (ad->amiga_ctlr, ad->amiga_slave, sc->sc_punit,
			      (char *) block, DEV_BSIZE, npbnum, sc->sc_bshift))
	      break;
	  
	    if (pb->pb_ID != IDNAME_PARTITION)
	      break;

	    /* remember those values, they're destroyed when reading the first
	       partition block */
	    npbnum      = pb->pb_Next;
	    start_block = pb->pb_Environment[DE_LOWCYL] * dl->d_secpercyl;
	    reserved    = pb->pb_Environment[DE_RESERVEDBLKS];
	    end_block   = 
	      (1 + pb->pb_Environment[DE_UPPERCYL]) * dl->d_secpercyl - 1;

	    part = -1;
	    bsd_part = 1;
	    switch (pb->pb_Environment[DE_DOSTYPE])
	      {
	      case DOSTYPE_BSD_ROOT:
	        if (! dl->d_partitions[0].p_size)
	          {
		    extern u_long bootdev;

		    part = 0;

		    /* Since we don't boot BSD with a standalone-kernel, but
		       with a simple boot-loader from AmigaDOS, we fake a
		       bootdev by assigning the just computed values of the
		       root partition. */

		    /* 4: 		sd major
		       0: 		adaptor number
		       ad->amiga_ctlr:	controller number
		       ad->amiga_unit:  scsi-unit
		       part:		partition (A) */

		    if (rootdev == NODEV)
		      bootdev = MAKEBOOTDEV (4, 0, ad->amiga_ctlr, ad->amiga_unit, part);
		    else
		      bootdev = rootdev;
#ifdef DEBUG
		    if (sddebug & SDB_BOOTDEV)
		      printf ("sdinit: setting bootdev to 0x%08x\n", bootdev);
#endif
		  }
		break;

	      case DOSTYPE_BSD_SWAP:		
	        if (! dl->d_partitions[1].p_size)
	          part = 1;
	        break;
	        
	      case DOSTYPE_BSD_D:
	        if (! dl->d_partitions[3].p_size)
	          part = 3;
	        break;
		
	      case DOSTYPE_BSD_E:
	        if (! dl->d_partitions[4].p_size)
	          part = 4;
	        break;
		
	      case DOSTYPE_BSD_F:
	        if (! dl->d_partitions[5].p_size)
	          part = 5;
	        break;
		
	      case DOSTYPE_BSD_G:
	        if (! dl->d_partitions[6].p_size)
	          part = 6;
	        break;
		
	      case DOSTYPE_BSD_H:
	        if (! dl->d_partitions[7].p_size)
	          part = 7;
	        break;

	      default:
		/* Not a BSD partition, let's put it at a minor device number
		   above 0x3f (== 077 == 63).  */
		part = non_bsd_part++;
		bsd_part = 0;
		if (part >= MAXPARTITIONS)
		  {
		    printf ("Ignoring partition at %d due to sd slave table "
			    "overflow.\n", start_block);
		    part = -1;
		  }
		break;
	      }

	    if (part >= 0)
	      {
		num_part++;
		if (bsd_part)
		  {
		    num_bsd_part++;
		    dl->d_partitions[part].p_offset = start_block + reserved;
		  }
		else
		  dl->d_partitions[part].p_offset = start_block;
	        dl->d_partitions[part].p_size
		  = end_block - dl->d_partitions[part].p_offset + 1;

		/* for now, have this static.. XXX */
		dl->d_partitions[part].p_fsize  = 1024;
		dl->d_partitions[part].p_fstype
		  = bsd_part ? part == 1 ? FS_SWAP : FS_BSDFFS : FS_ADOS;
		dl->d_partitions[part].p_frag   = 8;
		dl->d_partitions[part].p_cpg    = 0;	/* XXX */
	    
#ifdef DEBUG
	        if (sddebug)
		  if (bsd_part)
		    printf ("BSD partition %c @%d - %d size %d\n", part + 'A',
			    start_block, end_block,
			    dl->d_partitions[part].p_size);
		  else
		    printf ("Non-BSD partition %d @%d - %d size %d\n",
			    part - 8, start_block, end_block,
			    dl->d_partitions[part].p_size);
#endif
	      }
	  }

	/* This slot is obviously overloaded to mean both the actual count
	   of partitions (that's our view) and the maximum index of the
	   d_partition table incremented by one (this is the view presented
	   by the dkcksum function in ufs_disksubr.c).  If we want to go
	   that way, change num_part to non_bsd_part.  XXX */
#if 0
	dl->d_npartitions = num_part;
#else
	dl->d_npartitions = non_bsd_part;
#endif
#ifdef DEBUG
	if (sddebug)
	  printf ("total %d partitions (%d BSD).\n", num_part, num_bsd_part);
#endif

	/* C gets everything */
	dl->d_partitions[2].p_size = sc->sc_blks;

do_chksum:
	dl->d_checksum = 0;
	dl->d_checksum = dkcksum (dl);

	sc->sc_have_label = 1;
	sc->sc_write_label = 0;

	sc->sc_flags = SDF_ALIVE;
	splx (s);
	return(1);
}

void
sdreset(sc, ad)
	register struct sd_softc *sc;
	register struct amiga_device *ad;
{
	sdstats[ad->amiga_unit].sdresets++;
}

int
sdopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	register int unit = sdunit(dev);
	register struct sd_softc *sc = &sd_softc[unit];

	if (unit >= NSD)
		return(ENXIO);
	if ((sc->sc_flags & SDF_ALIVE) == 0 && suser(p->p_ucred, &p->p_acflag))
		return(ENXIO);

	if (sc->sc_ad->amiga_dk >= 0)
		dk_wpms[sc->sc_ad->amiga_dk] = sc->sc_wpms;
	return(0);
}

/*
 * This routine is called for partial block transfers and non-aligned
 * transfers (the latter only being possible on devices with a block size
 * larger than DEV_BSIZE).  The operation is performed in three steps
 * using a locally allocated buffer:
 *	1. transfer any initial partial block
 *	2. transfer full blocks
 *	3. transfer any final partial block
 */
static void
sdlblkstrat(bp, bsize)
	register struct buf *bp;
	register int bsize;
{
	register struct buf *cbp = (struct buf *)malloc(sizeof(struct buf),
							M_DEVBUF, M_WAITOK);
	caddr_t cbuf = (caddr_t)malloc(bsize, M_DEVBUF, M_WAITOK);
	register int bn, resid;
	register caddr_t addr;

	bzero((caddr_t)cbp, sizeof(*cbp));
	cbp->b_proc = curproc;		/* XXX */
	cbp->b_dev = bp->b_dev;
	bn = bp->b_blkno;
	resid = bp->b_bcount;
	addr = bp->b_un.b_addr;
#ifdef DEBUG
	if (sddebug & SDB_PARTIAL)
		printf("sdlblkstrat: bp %x flags %x bn %x resid %x addr %x\n",
		       bp, bp->b_flags, bn, resid, addr);
#endif

	while (resid > 0) {
		register int boff = dbtob(bn) & (bsize - 1);
		register int count;

		if (boff || resid < bsize) {
			sdstats[sdunit(bp->b_dev)].sdpartials++;
			count = MIN(resid, bsize - boff);
			cbp->b_flags = B_BUSY | B_PHYS | B_READ;
			cbp->b_blkno = bn - btodb(boff);
			cbp->b_un.b_addr = cbuf;
			cbp->b_bcount = bsize;
#ifdef DEBUG
			if (sddebug & SDB_PARTIAL)
				printf(" readahead: bn %x cnt %x off %x addr %x\n",
				       cbp->b_blkno, count, boff, addr);
#endif
			sdstrategy(cbp);
			biowait(cbp);
			if (cbp->b_flags & B_ERROR) {
				bp->b_flags |= B_ERROR;
				bp->b_error = cbp->b_error;
				break;
			}
			if (bp->b_flags & B_READ) {
				bcopy(&cbuf[boff], addr, count);
				goto done;
			}
			bcopy(addr, &cbuf[boff], count);
#ifdef DEBUG
			if (sddebug & SDB_PARTIAL)
				printf(" writeback: bn %x cnt %x off %x addr %x\n",
				       cbp->b_blkno, count, boff, addr);
#endif
		} else {
			count = resid & ~(bsize - 1);
			cbp->b_blkno = bn;
			cbp->b_un.b_addr = addr;
			cbp->b_bcount = count;
#ifdef DEBUG
			if (sddebug & SDB_PARTIAL)
				printf(" fulltrans: bn %x cnt %x addr %x\n",
				       cbp->b_blkno, count, addr);
#endif
		}
		cbp->b_flags = B_BUSY | B_PHYS | (bp->b_flags & B_READ);
		sdstrategy(cbp);
		biowait(cbp);
		if (cbp->b_flags & B_ERROR) {
			bp->b_flags |= B_ERROR;
			bp->b_error = cbp->b_error;
			break;
		}
done:
		bn += btodb(count);
		resid -= count;
		addr += count;
#ifdef DEBUG
		if (sddebug & SDB_PARTIAL)
			printf(" done: bn %x resid %x addr %x\n",
			       bn, resid, addr);
#endif
	}
	free(cbuf, M_DEVBUF);
	free(cbp, M_DEVBUF);
}

void
sdstrategy(bp)
	register struct buf *bp;
{
	register int unit = sdunit(bp->b_dev);
	register struct sd_softc *sc = &sd_softc[unit];
	register struct partition *pinfo = &sc->sc_label.d_partitions[sdpart(bp->b_dev)];
	register struct buf *dp = &sdtab[unit];
	register daddr_t bn;
	register int sz, s;

	QPRINTF (("sdstrat: unit=%d [s=%d,l=%d], [b=%d,l=%d]\n", unit, 
		  pinfo->p_offset, pinfo->p_size, bp->b_blkno, bp->b_bcount));

	if (sc->sc_format_pid) {
		if (sc->sc_format_pid != curproc->p_pid) {	/* XXX */
			bp->b_error = EPERM;
			bp->b_flags |= B_ERROR;
			goto done;
		}
		bp->b_cylin = 0;
	} else {
		bn = bp->b_blkno;
		sz = howmany(bp->b_bcount, DEV_BSIZE);
		if (bn < 0 || bn + sz > pinfo->p_size) {
			sz = pinfo->p_size - bn;
			if (sz == 0) {
				bp->b_resid = bp->b_bcount;
				goto done;
			}
			if (sz < 0) {
				bp->b_error = EINVAL;
				bp->b_flags |= B_ERROR;
				goto done;
			}
			bp->b_bcount = dbtob(sz);
		}
		/*
		 * Non-aligned or partial-block transfers handled specially.
		 */
		s = sc->sc_blksize - 1;
		if ((dbtob(bn) & s) || (bp->b_bcount & s)) {
			sdlblkstrat(bp, sc->sc_blksize);
			goto done;
		}
		bp->b_cylin = (bn + pinfo->p_offset) >> sc->sc_bshift;
	}
	s = splbio();
	disksort(dp, bp);
	if (dp->b_active == 0) {
		dp->b_active = 1;
		sdustart(unit);
	}
	splx(s);
	return;
done:
	biodone(bp);
}

void
sdustart(unit)
	register int unit;
{
	if ((sd_softc[unit].sc_ad->amiga_cdriver->d_req)(&sd_softc[unit].sc_dq))
		sdstart(unit);
}

/*
 * Return:
 *	0	if not really an error
 *	<0	if we should do a retry
 *	>0	if a fatal error
 */
static int
sderror(unit, sc, am, stat)
	int unit, stat;
	register struct sd_softc *sc;
	register struct amiga_device *am;
{
	int cond = 1;

	sdsense[unit].status = stat;
	if (stat & STS_CHECKCOND) {
		struct scsi_xsense *sp;

		(am->amiga_cdriver->d_rqs)(am->amiga_ctlr, am->amiga_slave,
				   sc->sc_punit,
				   sdsense[unit].sense,
				   sizeof(sdsense[unit].sense));
		sp = (struct scsi_xsense *)sdsense[unit].sense;
		printf("sd%d: scsi sense class %d, code %d", unit,
			sp->class, sp->code);
		if (sp->class == 7) {
			printf(", key %d", sp->key);
			if (sp->valid)
				printf(", blk %d", *(int *)&sp->info1);
			switch (sp->key) {
			/* no sense, try again */
			case 0:
				cond = -1;
				break;
			/* recovered error, not a problem */
			case 1:
				cond = 0;
				break;
			}
		}
		printf("\n");
	}
	return(cond);
}

static void
sdfinish(unit, sc, bp)
	int unit;
	register struct sd_softc *sc;
	register struct buf *bp;
{
	sdtab[unit].b_errcnt = 0;
	sdtab[unit].b_actf = bp->b_actf;
	bp->b_resid = 0;
	biodone(bp);
	(sc->sc_ad->amiga_cdriver->d_free)(&sc->sc_dq);
	if (sdtab[unit].b_actf)
		sdustart(unit);
	else
		sdtab[unit].b_active = 0;
}

void
sdstart(unit)
	register int unit;
{
	register struct sd_softc *sc = &sd_softc[unit];
	register struct amiga_device *am = sc->sc_ad;

	/*
	 * we have the SCSI bus -- in format mode, we may or may not need dma
	 * so check now.
	 */
	if (sc->sc_format_pid && legal_cmds[sdcmd[unit].cdb[0]] > 0) {
		register struct buf *bp = sdtab[unit].b_actf;
		register int sts;

		sts = (am->amiga_cdriver->d_immcmd)(am->amiga_ctlr, am->amiga_slave,
					 sc->sc_punit,
					 &sdcmd[unit],
					 bp->b_un.b_addr, bp->b_bcount,
					 bp->b_flags & B_READ);
		sdsense[unit].status = sts;
		if (sts & 0xfe) {
			(void) sderror(unit, sc, am, sts);
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
		}
		sdfinish(unit, sc, bp);

	} else if ((am->amiga_cdriver->d_ustart)(am->amiga_ctlr))
		sdgo(unit);
}

void
sdgo(unit)
	register int unit;
{
	register struct sd_softc *sc = &sd_softc[unit];
	register struct amiga_device *am = sc->sc_ad;
	register struct buf *bp = sdtab[unit].b_actf;
	register int pad;
	register struct scsi_fmt_cdb *cmd;

	/* XXX ? */
	if (! bp)
	  return;

	if (sc->sc_format_pid) {
		cmd = &sdcmd[unit];
		pad = 0;
	} else {
		cmd = bp->b_flags & B_READ? &sd_read_cmd : &sd_write_cmd;
		*(int *)(&cmd->cdb[2]) = bp->b_cylin;
		pad = howmany(bp->b_bcount, sc->sc_blksize);
#if 0
		*(u_short *)(&cmd->cdb[7]) = pad;
#else /* Kludge for GVP 3001 bug */
		cmd->cdb[7] = pad >> 8;
		cmd->cdb[8] = pad;
#endif
		QPRINTF(("sdgo[%d, %d]: %s @%d >%d\n",
			 am->amiga_ctlr, am->amiga_slave,
			 bp->b_flags & B_READ ? "R" : "W",
			 bp->b_cylin, pad));
		pad = (bp->b_bcount & (sc->sc_blksize - 1)) != 0;
#ifdef DEBUG
		if (pad)
			printf("sd%d: partial block xfer -- %x bytes\n",
			       unit, bp->b_bcount);
#endif
		sdstats[unit].sdtransfers++;
	}
	if ((am->amiga_cdriver->d_go)(am->amiga_ctlr, am->amiga_slave, sc->sc_punit, bp, cmd, pad) == 0) {
		if (am->amiga_dk >= 0) {
			dk_busy |= 1 << am->amiga_dk;
			++dk_seek[am->amiga_dk];
			++dk_xfer[am->amiga_dk];
			dk_wds[am->amiga_dk] += bp->b_bcount >> 6;
		}
		return;
	}
#ifdef DEBUG
	if (sddebug & SDB_ERROR)
		printf("sd%d: sdstart: %s adr %d blk %d len %d ecnt %d\n",
		       unit, bp->b_flags & B_READ? "read" : "write",
		       bp->b_un.b_addr, bp->b_cylin, bp->b_bcount,
		       sdtab[unit].b_errcnt);
#endif
	bp->b_flags |= B_ERROR;
	bp->b_error = EIO;
	sdfinish(unit, sc, bp);
}

void
sdintr(unit, stat)
	register int unit;
	int stat;
{
	register struct sd_softc *sc = &sd_softc[unit];
	register struct buf *bp = sdtab[unit].b_actf;
	register struct amiga_device *am = sc->sc_ad;
	int cond;
	
	if (bp == NULL) {
		printf("sd%d: bp == NULL\n", unit);
		return;
	}
	if (am->amiga_dk >= 0)
		dk_busy &=~ (1 << am->amiga_dk);
	if (stat) {
#ifdef DEBUG
		if (sddebug & SDB_ERROR)
			printf("sd%d: sdintr: bad scsi status 0x%x\n",
				unit, stat);
#endif
		cond = sderror(unit, sc, am, stat);
		if (cond) {
			if (cond < 0 && sdtab[unit].b_errcnt++ < SDRETRY) {
#ifdef DEBUG
				if (sddebug & SDB_ERROR)
					printf("sd%d: retry #%d\n",
					       unit, sdtab[unit].b_errcnt);
#endif
				sdstart(unit);
				return;
			}
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
		}
	}
	sdfinish(unit, sc, bp);
}

u_int
sdminphys (bp)
	struct buf *bp;
{
	register int unit = sdunit(bp->b_dev);
	register struct sd_softc *sc = &sd_softc[unit];
	register struct partition *pinfo = &sc->sc_label.d_partitions[sdpart(bp->b_dev)];
	register daddr_t bn;
	register int sz, s;

	/* first restrict by max amount of physio possible */
	bp->b_bcount = min(MAXPHYS, bp->b_bcount);

	/* then restrict by partition size (code replicated from sdstrategy).
	   Should minphys() really fiddle around with b_resid and b_flags ??? XXXX */

	bn = bp->b_blkno;
	sz = howmany(bp->b_bcount, DEV_BSIZE);
	if (bn < 0 || bn + sz > pinfo->p_size) 
	  {
	    sz = pinfo->p_size - bn;
	    if (sz == 0) 
	      {
	        bp->b_resid = bp->b_bcount;
	        goto done;
	      }
	    if (sz < 0) 
	      {
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto done;
	      }
	    bp->b_bcount = dbtob(sz);
	  }

	return bp->b_bcount;
done:
	return 0;
}


int
sdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register int unit = sdunit(dev);
	register int pid;

	if ((pid = sd_softc[unit].sc_format_pid) &&
	    pid != uio->uio_procp->p_pid)
		return (EPERM);
		
	return (physio(sdstrategy, NULL, dev, B_READ, sdminphys, uio));
}

int
sdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	register int unit = sdunit(dev);
	register int pid;

	if ((pid = sd_softc[unit].sc_format_pid) &&
	    pid != uio->uio_procp->p_pid)
		return (EPERM);
		
	return (physio(sdstrategy, NULL, dev, B_WRITE, sdminphys, uio));
}

int
sdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	register int unit = sdunit(dev);
	register struct sd_softc *sc = &sd_softc[unit];
	struct disklabel *dl;
	int error;

	switch (cmd) {
	default:
		return (EINVAL);

	case DIOCGDINFO:
		*(struct disklabel *)data = sc->sc_label;
		return 0;

        case DIOCSDINFO:
                if ((flag & FWRITE) == 0)
		  return EBADF;

		dl = (struct disklabel *)data;
		if (error = setdisklabel(&sc->sc_label, dl, 0, 0));
			return error;
		sc->sc_have_label = 1;
                return 0;

        case DIOCWLABEL:
                if ((flag & FWRITE) == 0)
		  return EBADF;

		sc->sc_write_label = *(int *)data;
                return 0;

        case DIOCWDINFO:
                if ((flag & FWRITE) == 0)
		  return EBADF;

		dl = (struct disklabel *)data;

		if (error = setdisklabel (&sc->sc_label, dl, 0, 0))
		  return error;

		sc->sc_have_label = 1;

#if 0
		old_wlabel = as->wlabel;
		as->wlabel = 1;
		error = writedisklabel(dev, asstrategy, &as->label,
				as->dospart);
		as->wlabel = old_wlabel;
#endif
		return EINVAL;

	case SDIOCSFORMAT:
		/* take this device into or out of "format" mode */
		if (suser(p->p_ucred, &p->p_acflag))
			return(EPERM);

		if (*(int *)data) {
			if (sc->sc_format_pid)
				return (EPERM);
			sc->sc_format_pid = p->p_pid;
		} else
			sc->sc_format_pid = 0;
		return (0);

	case SDIOCGFORMAT:
		/* find out who has the device in format mode */
		*(int *)data = sc->sc_format_pid;
		return (0);

	case SDIOCSCSICOMMAND:
		/*
		 * Save what user gave us as SCSI cdb to use with next
		 * read or write to the char device.
		 */
		if (sc->sc_format_pid != p->p_pid)
			return (EPERM);
		if (legal_cmds[((struct scsi_fmt_cdb *)data)->cdb[0]] == 0)
			return (EINVAL);
		bcopy(data, (caddr_t)&sdcmd[unit], sizeof(sdcmd[0]));
		return (0);

	case SDIOCSENSE:
		/*
		 * return the SCSI sense data saved after the last
		 * operation that completed with "check condition" status.
		 */
		bcopy((caddr_t)&sdsense[unit], data, sizeof(sdsense[0]));
		return (0);
		
	}
	/*NOTREACHED*/
}

int
sdsize(dev)
	dev_t dev;
{
	register int unit = sdunit(dev);
	register struct sd_softc *sc = &sd_softc[unit];

	if (unit >= NSD || (sc->sc_flags & SDF_ALIVE) == 0)
		return(-1);
	return(sc->sc_label.d_partitions[sdpart(dev)].p_size);
}

/*
 * Non-interrupt driven, non-dma dump routine.
 */
int
sddump(dev)
	dev_t dev;
{
	int part = sdpart(dev);
	int unit = sdunit(dev);
	register struct sd_softc *sc = &sd_softc[unit];
	register struct amiga_device *am = sc->sc_ad;
	register daddr_t baddr;
	register int maddr;
	register int pages, i;
	int stat;
	extern int lowram;

	/*
	 * Hmm... all vax drivers dump maxfree pages which is physmem minus
	 * the message buffer.  Is there a reason for not dumping the
	 * message buffer?  Savecore expects to read 'dumpsize' pages of
	 * dump, where dumpsys() sets dumpsize to physmem!
	 */
	pages = physmem;

	/* is drive ok? */
	if (unit >= NSD || (sc->sc_flags & SDF_ALIVE) == 0)
		return (ENXIO);
	/* dump parameters in range? */
	if (dumplo < 0 || dumplo >= sc->sc_label.d_partitions[part].p_size)
		return (EINVAL);
	if (dumplo + ctod(pages) > sc->sc_label.d_partitions[part].p_size)
		pages = dtoc(sc->sc_label.d_partitions[part].p_size - dumplo);
	maddr = ctob(lowram);
	baddr = dumplo + sc->sc_label.d_partitions[part].p_offset;
	/* scsi bus idle? */
	if (!(am->amiga_cdriver->d_req)(&sc->sc_dq)) {
		(am->amiga_cdriver->d_reset)(am->amiga_ctlr);
		sdreset(sc, sc->sc_ad);
		printf("[ drive %d reset ] ", unit);
	}
	for (i = 0; i < pages; i++) {
#define NPGMB	(1024*1024/NBPG)
		/* print out how many Mbs we have dumped */
		if (i && (i % NPGMB) == 0)
			printf("%d ", i / NPGMB);
#undef NPBMG
		pmap_enter(pmap_kernel(), vmmap, maddr, VM_PROT_READ, TRUE);
		stat = (am->amiga_cdriver->d_ttwrite)(am->amiga_ctlr, am->amiga_slave,
				     sc->sc_punit,
				     vmmap, NBPG, baddr, sc->sc_bshift);
		if (stat) {
			printf("sddump: scsi write error 0x%x\n", stat);
			return (EIO);
		}
		maddr += NBPG;
		baddr += ctod(1);
	}
	return (0);
}

/*
  * scsi_unlock_floptical() 
  *
  * Unlocks a floptical drive by reading the vendor
  * mode page 0x2e (floptical page). In addition a rezero 
  * unit command is issued, because the first READ command
  * fails with a "Servo Hardware Fault" error 
  * (ASC=2,ASCQ=0). The rezero command will also fail, but
  * now we can ignore it.
  */
int
scsi_unlock_floptical(ctlr, slave, unit)
	int ctlr;
	int slave;
	int unit;
{
	struct mp_header_s {
		u_char mode_data_length;
		u_char media_type;
		u_char block_descriptor_length;
	};
 
	struct mp_descriptor_s {
		u_char reserved[4];
		u_char block_length[4];
	};
 
	struct floppage_s {
		u_char reserved1:1,
		    pagecode:7;
		u_char len;
		u_char reserved2;
		u_char hamheads;
		u_char secpertrack;
		u_short bytespersector;
		u_short hamcyls;
		u_char reserved3:4,
		    ejm:1,
		    reserved4:2,
		    nhf:1;
		u_char idstr[13];
		u_char reserved5[6];
	};
 
	struct sense_return_s {
		struct mp_header_s mp_header;
		struct mp_descriptor_s mp_descriptor;
		struct floppage_s floppage;
	};
	register struct amiga_device *ad = sd_softc[unit].sc_ad;
	struct sense_return_s sr;
	struct scsi_fmt_cdb cdb;
	struct scsi_cdb6 *cmd;
 	int stat;

	cdb.len = 6;
	cmd = (struct scsi_cdb6 *)&cdb.cdb;
	cmd->cmd = CMD_MODE_SENSE;
        cmd->lun = unit;
        cmd->lbah = 0;
 	cmd->lbam = 0x2e;
        cmd->lbal = 0;
 	cmd->len = sizeof(sr);
        cmd->xtra = 0;

	/* Mode sense. */
	stat = (ad->amiga_cdriver->d_immcmd)(ctlr, slave, unit, &cdb,
	    (u_char *)&sr, sizeof(sr), B_READ);
	if(stat == 0) { 
#if defined(DIAGNOSTIC) && defined(DEBUG)
		printf("Floptical information\n");
		printf("Page code          : %d\n",sr.floppage.pagecode);
		printf("Page length        : %d\n",sr.floppage.len);
		printf("Number of HAM heads: %d\n",sr.floppage.hamheads);
		printf("Sectors per Track  : %d\n",sr.floppage.secpertrack);
		printf("Data Bytes/Sector  : %d\n",sr.floppage.bytespersector);
		printf("Number of HAM cyls : %d\n",sr.floppage.hamcyls);
		printf("Eject Motor        : %d\n",sr.floppage.ejm);
		printf("Non-HAM format     : %d\n",sr.floppage.nhf);
		printf("IdString           : %s\n",sr.floppage.idstr);
#endif
		cmd->cmd = CMD_REWIND;
		stat = (ad->amiga_cdriver->d_immcmd_nd)(ctlr, slave, unit, &cdb);
		if(stat == 2)
			stat = 0; /* explicitly ignore servo fault errors */
	}
	return(stat);
}
#endif
