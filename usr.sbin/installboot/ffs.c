/*	$NetBSD: ffs.c,v 1.1 2002/04/19 07:08:51 lukem Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Fredette.
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

#include <sys/cdefs.h>
#if defined(__RCSID) && !defined(__lint)
__RCSID("$NetBSD: ffs.c,v 1.1 2002/04/19 07:08:51 lukem Exp $");
#endif	/* !__lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "installboot.h"

#undef DIRBLKSIZ

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_bswap.h>

/* This reads a disk block from the filesystem. */
static int
ffs_read_disk_block(ib_params *params, uint32_t blkno, int size, char *blk)
{
	int	rv;

	assert(params->filesystem != NULL);
	assert(params->fsfd != -1);
	assert(blkno > 0);
	assert(size > 0);
	assert(blk != NULL);

	rv = pread(params->fsfd, blk, size, blkno * DEV_BSIZE);
	if (rv == -1) {
		warn("Reading block %d in `%s'", blkno, params->filesystem);
		return (0);
	} else if (rv != size) {
		warnx("Reading block %d in `%s': short read", blkno,
		    params->filesystem);
		return (0);
	}

	return (1);
}

/*
 * This iterates over the data blocks belonging to an inode,
 * making a callback each iteration with the disk block number
 * and the size.
 */
static int
ffs_find_disk_blocks(ib_params *params, uint32_t ino, 
	int (*callback)(ib_params *, void *, uint32_t, uint32_t, int),
	void *state)
{
	char		sbbuf[SBSIZE];
	struct fs	*fs;
	int		needswap;
	char		inodebuf[MAXBSIZE];
	struct dinode	*inode;
	int		level_i;
	ufs_daddr_t	blk, lblk, nblk;
	int		rv;
#define LEVELS 4
	struct {
		ufs_daddr_t	*blknums;
		unsigned long	blkcount;
		char		diskbuf[MAXBSIZE];
	} level[LEVELS];

	/* Read the superblock. */
	if (! ffs_read_disk_block(params, SBLOCK, SBSIZE, sbbuf))
		return (0);
	fs = (struct fs *)sbbuf;
	needswap = 0;
	if (fs->fs_magic == htole32(FS_MAGIC)) {
#if BYTE_ORDER == BIG_ENDIAN
		needswap = 1;
#endif
	} else if (fs->fs_magic == htobe32(FS_MAGIC)) {
#if BYTE_ORDER == LITTLE_ENDIAN
		needswap = 1;
#endif
	} else
		return (0);
	if (needswap)
		ffs_sb_swap(fs, fs);

	/* Sanity check the superblock. */
	if (fs->fs_magic != FS_MAGIC) {
		warnx("Bad superblock magic number in `%s'",
		    params->filesystem);
		return (0);
	}
	if (fs->fs_inopb <= 0) {
		warnx("Bad inopb %d in superblock in `%s'",
		    fs->fs_inopb, params->filesystem);
		return (0);
	}

	/* Read the inode. */
	if (! ffs_read_disk_block(params, fsbtodb(fs, ino_to_fsba(fs, ino)),
		fs->fs_bsize, inodebuf))
		return (0);
	inode = (struct dinode *)inodebuf;
	inode += ino_to_fsbo(fs, ino);
	if (needswap)
		ffs_dinode_swap(inode, inode);

	/* Get the block count and initialize for our block walk. */
	nblk = howmany(inode->di_size, fs->fs_bsize);
	lblk = 0;
	level_i = 0;
	level[0].blknums = &inode->di_db[0];
	level[0].blkcount = NDADDR;
	level[1].blknums = &inode->di_ib[0];
	level[1].blkcount = 1;
	level[2].blknums = &inode->di_ib[1];
	level[2].blkcount = 1;
	level[3].blknums = &inode->di_ib[2];
	level[3].blkcount = 1;

	/* Walk the data blocks. */
	while (nblk > 0) {

		/*
		 * If there are no more blocks at this indirection 
		 * level, move up one indirection level and loop.
		 */
		if (level[level_i].blkcount == 0) {
			if (++level_i == LEVELS)
				break;
			continue;
		}

		/* Get the next block at this level. */
		blk = *(level[level_i].blknums++);
		level[level_i].blkcount--;
		if (needswap)
			blk = bswap32(blk);

#if 0
		fprintf(stderr, "ino %lu blk %lu level %d\n", ino, blk, 
		    level_i);
#endif

		/*
		 * If we're not at the direct level, descend one
		 * level, read in that level's new block list, 
		 * and loop.
		 */
		if (level_i > 0) {
			level_i--;
			if (blk == 0)
				memset(level[level_i].diskbuf, 0, MAXBSIZE);
			else if (! ffs_read_disk_block(params, 
				fsbtodb(fs, blk),
				fs->fs_bsize, level[level_i].diskbuf))
				return (0);
			level[level_i].blknums = 
				(ufs_daddr_t *)level[level_i].diskbuf;
			level[level_i].blkcount = NINDIR(fs);
			continue;
		}

		/* blk is the next direct level block. */
#if 0
		fprintf(stderr, "ino %lu db %lu blksize %lu\n", ino, 
		    fsbtodb(fs, blk), dblksize(fs, inode, lblk));
#endif
		rv = (*callback)(params, state, 
		    fsbtodb(fs, blk), dblksize(fs, inode, lblk), needswap);
		lblk++;
		nblk--;
		if (rv != 1)
			return (rv);
	}

	if (nblk != 0) {
		warnx("Inode %d in `%s' ran out of blocks?", ino,
		    params->filesystem);
		return (0);
	}

	return (1);
}

/*
 * This callback reads a block of the root directory, 
 * searches for an entry for the secondary bootstrap,
 * and saves the inode number if one is found.
 */
static int
ffs_findstage2_ino(ib_params *params, void *_ino, 
	uint32_t blk, uint32_t blksize, int needswap)
{
	char		dirbuf[MAXBSIZE];
	struct direct	*de, *ede;
	uint32_t	ino;

	/* Skip directory holes. */
	if (blk == 0)
		return (1);

	/* Read the directory block. */
	if (! ffs_read_disk_block(params, blk, blksize, dirbuf))
		return (0);

	/* Loop over the directory entries. */
	de = (struct direct *)&dirbuf[0];
	ede = (struct direct *)&dirbuf[blksize];
	while (de < ede) {
		ino = de->d_ino;
		if (needswap) {
			ino = bswap32(ino);
			de->d_reclen = bswap16(de->d_reclen);
		}
		if (ino != 0 && strcmp(de->d_name, params->stage2) == 0) {
			*((uint32_t *)_ino) = ino;
			return (2);
		}
		de = (struct direct *)((char *)de + de->d_reclen);
	}

	return (1);
}

struct findblks_state {
	uint32_t	maxblk;
	uint32_t	nblk;
	ib_block	*blocks;
};

/* This callback records the blocks of the secondary bootstrap. */
static int
ffs_findstage2_blocks(ib_params *params, void *_state,
	uint32_t blk, uint32_t blksize, int needswap)
{
	struct findblks_state *state = _state;

	if (state->nblk == state->maxblk) {
		warnx("Secondary bootstrap `%s' has too many blocks " \
		    "(max %d)\n", params->stage2, state->maxblk);
		return (0);
	}
	state->blocks[state->nblk].block = blk;
	state->blocks[state->nblk].blocksize = blksize;
	state->nblk++;
	return (1);
}

	/* publically visible functions */

int
ffs_match(ib_params *params)
{
	char		sbbuf[SBSIZE];
	struct fs	*fs;

	/* Read and check the superblock. */
	if (! ffs_read_disk_block(params, SBLOCK, SBSIZE, sbbuf))
		return (0);
	fs = (struct fs *)sbbuf;
	if (fs->fs_magic == htole32(FS_MAGIC) ||
	    fs->fs_magic == htobe32(FS_MAGIC))
		return (1);

	return (0);
}

int
ffs_findstage2(ib_params *params, uint32_t *maxblk, ib_block *blocks)
{
	int			rv;
	uint32_t		ino;
	struct findblks_state	state;

	assert (params->stage2 != NULL);

	/* The secondary bootstrap must be clearly in /. */
	if (params->stage2[0] == '/')
		params->stage2++;
	if (strchr(params->stage2, '/') != NULL) {
		warnx("The secondary bootstrap `%s' must be in /",
		    params->stage2);
		return (0);
	}

	/* Get the inode number of the secondary bootstrap. */
	rv = ffs_find_disk_blocks(params, ROOTINO, ffs_findstage2_ino, &ino);
	if (rv != 2)
		return (0);

	/* Record the disk blocks of the secondary bootstrap. */
	state.maxblk = *maxblk;
	state.nblk = 0;
	state.blocks = blocks;
	rv = ffs_find_disk_blocks(params, ino, ffs_findstage2_blocks, &state);
	if (! rv)
		return (0);

	*maxblk = state.nblk;
	return (1);
}
