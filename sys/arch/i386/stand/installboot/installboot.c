/*	$NetBSD: installboot.c,v 1.1.1.1 1997/03/14 02:40:32 perry Exp $	*/

/*
 * Copyright (c) 1994 Paul Kranenburg
 * All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Perry E. Metzger.  All rights reserved.
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
 *      This product includes software developed by Paul Kranenburg.
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
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

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <sys/errno.h>
#include <err.h>
#include <a.out.h>
#include <fcntl.h>
#include <nlist.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "bbinfo.h"

#define DEFBBLKNAME "boot"

struct fraglist *fraglist;

struct nlist nl[] = {
#define X_fraglist	0
	{{"_fraglist"}},
	{{NULL}}
};

int verbose = 0;
int nowrite = 0;

char bootblkpath[MAXPATHLEN];
int bootblkscreated = 0; /* flag for error handling */

char *
loadprotoblocks(fname, size)
	char *fname;
	long *size;
{
	int	fd;
	size_t	tdsize;		/* text+data size */
	size_t	bbsize;		/* boot block size (block aligned) */
	char	*bp;
	struct	nlist *nlp;
	struct	exec eh;

	fd = -1;
	bp = NULL;

	/* Locate block number array in proto file */
	if (nlist(fname, nl) != 0) {
		warnx("nlist: %s: symbols not found", fname);
		return NULL;
	}
	/* Validate symbol types (global text!). */
	for (nlp = nl; nlp->n_un.n_name; nlp++) {
		if (nlp->n_type != (N_TEXT | N_EXT)) {
			warnx("nlist: %s: wrong type", nlp->n_un.n_name);
			return NULL;
		}
	}

	if ((fd = open(fname, O_RDONLY)) < 0) {
		warn("open: %s", fname);
		return NULL;
	}
	if (read(fd, &eh, sizeof(eh)) != sizeof(eh)) {
		warn("read: %s", fname);
		goto bad;
	}
	if (N_GETMAGIC(eh) != OMAGIC) {
		warn("bad magic: 0x%lx", eh.a_midmag);
		goto bad;
	}
	/*
	 * We have to include the exec header in the beginning of
	 * the buffer, and leave extra space at the end in case
	 * the actual write to disk wants to skip the header.
	 */
	tdsize = eh.a_text + eh.a_data;
	bbsize = roundup(tdsize, DEV_BSIZE);

	/*
	 * Allocate extra space here because the caller may copy
	 * the boot block starting at the end of the exec header.
	 * This prevents reading beyond the end of the buffer.
	 */
	if ((bp = calloc(bbsize, 1)) == NULL) {
		warnx("malloc: %s: no memory", fname);
		goto bad;
	}
	/* read the rest of the file. */
	if (read(fd, bp, tdsize) != tdsize) {
		warn("read: %s", fname);
		goto bad;
	}

	*size = bbsize;	/* aligned to DEV_BSIZE */

	fraglist = (struct fraglist*)(bp + nl[X_fraglist].n_value);

	if (fraglist->magic != FRAGLISTMAGIC) {
	    printf("invalid bootblock version\n");
	    goto bad;
	}

	if (verbose) {
		printf("%s: entry point %#lx\n", fname, eh.a_entry);
		printf("proto bootblock size %ld\n", *size);
		printf("room for %d filesystem blocks at %#lx\n",
			fraglist->maxentries, nl[X_fraglist].n_value);
	}

	close(fd);
	return bp;

 bad:
	if (bp)
		free(bp);
	if (fd >= 0)
		close(fd);
	return NULL;
}

static int
devread(fd, buf, blk, size, msg)
	int	fd;
	void	*buf;
	daddr_t	blk;
	size_t	size;
	char	*msg;
{
	if (lseek(fd, dbtob(blk), SEEK_SET) != dbtob(blk)) {
	    warn("%s: devread: lseek", msg);
	    return(1);
	}

	if (read(fd, buf, size) != size) {
	    warn("%s: devread: read", msg);
	    return(1);
	}

	return(0);
}

/* add file system blocks to fraglist */
static int add_fsblk(fs, blk, blcnt)
struct fs *fs;
daddr_t blk;
int blcnt;
{
  int nblk;

  /* convert to disk blocks */
  blk = fsbtodb(fs, blk);
  nblk = fs->fs_bsize / DEV_BSIZE;
  if(nblk > blcnt)
      nblk = blcnt;

  if (verbose)
    printf("dblk: %d, num: %d\n", blk, nblk);

  /* start new entry or append to previous? */
  if(!fraglist->numentries ||
     (fraglist->entries[fraglist->numentries - 1].offset
      + fraglist->entries[fraglist->numentries - 1].num != blk)) {

    /* need new entry */
    if(fraglist->numentries > fraglist->maxentries - 1)
      errx(1, "not enough fragment space in bootcode\n");

    fraglist->entries[fraglist->numentries].offset = blk;
    fraglist->entries[fraglist->numentries++].num = 0;
  }

  fraglist->entries[fraglist->numentries - 1].num += nblk;

  return(blcnt - nblk);
}

static char sblock[SBSIZE];

int
loadblocknums(diskdev, bootblkname, bp, size)
char *diskdev, *bootblkname;
char *bp;
int size;
{
	int		devfd = -1, fd = -1;
	struct	stat	statbuf;
	struct	statfs	statfsbuf;
	struct fs	*fs;
	char		*buf = 0;
	daddr_t		blk, *ap;
	struct dinode	*ip;
	int		i, ndb;
	char *p;
	int allok = 0;

	devfd = open(diskdev, O_RDONLY, 0);
	if(devfd < 0) {
	    warn("open raw partition");
	    return(1);
	}

	/* Read superblock */
	if(devread(devfd, sblock, SBLOCK, SBSIZE, "superblock"))
	    goto out;
	fs = (struct fs *)sblock;

	if(fs->fs_magic != FS_MAGIC) {
	    warnx("invalid super block");
	    goto out;
	}

	if(verbose)
	    printf("last mountpoint: %s\n", fs->fs_fsmnt);

	/*
	 * create file in (assumed) fs root for bootloader data
	 */
	sprintf(bootblkpath, "%s/%s", fs->fs_fsmnt, bootblkname);
	fd = open(bootblkpath, O_RDWR | O_CREAT | O_EXCL, 0444);
	if(fd < 0) {
	    /* should overwrite if (!nowrite)???
	     for now, be cautious */
	    warn("open %s", bootblkpath);
	    goto out;
	}

	bootblkscreated = 1;

	/*
	 * some checks to make sure the disk is mounted
	 */

	/* get info where we are really putting the file */
	if (fstatfs(fd, &statfsbuf) != 0) {
	    warn("statfs: %s", bootblkpath);
	    goto out;
	}

	/* f_mntfromname should be the block device
	 which corresponds to our raw device */
	if((p = rindex(statfsbuf.f_mntfromname, '/'))) p++;
	else p = statfsbuf.f_mntfromname;
	if(strcmp(p, diskdev + strlen(diskdev) - strlen(p))) {
	    warnx("%s is not mounted", diskdev);
	    goto out;
	}

	/* perhaps redundant: (last) mountpoint of our device
	 should be mountpoint belonging to the file */
	if(strncmp(fs->fs_fsmnt, statfsbuf.f_mntonname, MFSNAMELEN)) {
	    warnx("inconsistent mount info");
	    goto out;
	}

	/* this code is FFS only */
	if (strncmp(statfsbuf.f_fstypename, MOUNT_FFS, MFSNAMELEN)) {
	    warnx("%s: must be on a FFS filesystem", bootblkpath);
	    goto out;
	}

	/*
	 * do the real write, flush, get inode number
	 */
	if(write(fd, bp, size) < 0) {
	    warn("write %s", bootblkpath);
	    goto out;
	}
	if (fsync(fd) != 0) {
	    warn("fsync: %s", bootblkpath);
	    goto out;
	}
	if (fstat(fd, &statbuf) != 0) {
	    warn("fstat: %s", bootblkpath);
	    goto out;
	}

	close(fd);
	fd = -1;

	/* paranoia */
	sync();
	sleep(3);

	/* Read inode */
	if ((buf = malloc(fs->fs_bsize)) == NULL) {
	    warnx("No memory for filesystem block");
	    goto out;
	}

	blk = fsbtodb(fs, ino_to_fsba(fs, statbuf.st_ino));
	if(devread(devfd, buf, blk, fs->fs_bsize, "inode"))
	    goto out;
	ip = (struct dinode *)(buf) + ino_to_fsbo(fs, statbuf.st_ino);

	/*
	 * Have the inode.  Figure out how many blocks we need.
	 */
	if(size != ip->di_size) {
	    warnx("size inconsistency");
	    goto out;
	}
	ndb = size / DEV_BSIZE; /* size is rounded! */

	if (verbose)
		printf("Will load %d blocks.\n", ndb);

	/*
	 * Get the block numbers, first direct blocks
	 */
	ap = ip->di_db;
	for (i = 0; i < NDADDR && *ap && ndb; i++, ap++)
	    ndb = add_fsblk(fs, *ap, ndb);

	if (ndb) {
	    /*
	     * Just one level of indirections; there isn't much room
	     * for more in the 1st-level bootblocks anyway.
	     */
	    blk = fsbtodb(fs, ip->di_ib[0]);
	    if(devread(devfd, buf, blk, fs->fs_bsize, "indirect block"))
		goto out;
	    ap = (daddr_t *)buf;
	    for (; i < NINDIR(fs) && *ap && ndb; i++, ap++) {
		ndb = add_fsblk(fs, *ap, ndb);
	    }

	    if(ndb) {
		warnx("too many fs blocks");
		goto out;
	    }
	}

	allok = 1;

out:
        if(buf)
	    free(buf);
        if(fd >= 0)
	    close(fd);
	if(devfd >= 0)
	    close(devfd);
	return(!allok);
}

static void
usage()
{
	fprintf(stderr,
		"usage: installboot [-n] [-v] [-f] <boot> <device>\n");
	exit(1);
}

int main(argc, argv)
int argc;
char *argv[];
{
  char c, *bp = 0;
  long size;
  int devfd = -1;
  struct disklabel dl;
  int bsdoffs;
  int i;
  int res;
  int forceifnolabel = 0;
  char *bootblkname = DEFBBLKNAME;
  int allok = 0;

  while ((c = getopt(argc, argv, "vnf")) != EOF) {
    switch (c) {
      case 'n':
	/* Do not actually write the bootblock to disk */
	nowrite = 1;
	break;
      case 'v':
	/* Chat */
	verbose = 1;
	break;
      case 'f':
	/* assume zero offset if no disklabel */
	forceifnolabel = 1;
	break;
      default:
	usage();
    }
  }

  if (argc - optind != 2) {
    usage();
  }

  if(argv[optind + 1][strlen(argv[optind + 1]) - 1] != 'a')
      errx(1, "use partition 'a'!");

  bp = loadprotoblocks(argv[optind], &size);
  if(!bp)
      errx(1, "error reading bootblocks");

  fraglist->numentries = 0;

  /* do we need the fraglist? */
  if(size > fraglist->loadsz * DEV_BSIZE) {

      if(loadblocknums(argv[optind + 1], bootblkname,
		       bp + fraglist->loadsz * DEV_BSIZE,
		       size - fraglist->loadsz * DEV_BSIZE))
	  goto out;

      size = fraglist->loadsz * DEV_BSIZE;
      /* size to be written to bootsect */
  }

  devfd = open(argv[optind + 1], O_RDWR, 0);
  if(devfd < 0) {
      warn("open raw partition RW");
      goto out;
  }

  if(ioctl(devfd, DIOCGDINFO, &dl) < 0) {
      if((errno == EINVAL) || (errno == ENOTTY)){
	  if(forceifnolabel)
	      bsdoffs = 0;
	  else {
	      warnx("no disklabel, use -f to install anyway");
	      goto out;
	  }
      }	else {
	  warn("get disklabel");
	  goto out;
      }
  } else
      bsdoffs = dl.d_partitions[0].p_offset;

  if(verbose)
      printf("BSD partition starts at sector %d\n", bsdoffs);

  /*
   * add offset of BSD partition to fraglist entries
   */
  for(i=0; i < fraglist->numentries; i++)
      fraglist->entries[i].offset += bsdoffs;

  if(!nowrite) {
      /*
       * write first blocks (max loadsz) to start of BSD partition,
       * skip disklabel (in second disk block)
       */
      lseek(devfd, 0, SEEK_SET);
      res = write(devfd, bp, DEV_BSIZE);
      if(res < 0) {
	  warn("final write1");
	  goto out;
      }
      lseek(devfd, 2 * DEV_BSIZE, SEEK_SET);
      res = write(devfd, bp + 2 * DEV_BSIZE, size - 2 * DEV_BSIZE);
      if(res < 0) {
	  warn("final write2");
	  goto out;
      }
  }

  allok = 1;

out:
  if(devfd >= 0)
      close(devfd);
  if(bp)
      free(bp);
  if(bootblkscreated && (!allok || nowrite))
      unlink(bootblkpath);
  return(!allok);
}
