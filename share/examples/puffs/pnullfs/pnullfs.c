/*	$NetBSD: pnullfs.c,v 1.5 2007/02/15 12:54:52 pooka Exp $	*/

/*
 * Copyright (c) 2007  Antti Kantee.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * pnullfs: puffs nullfs example
 */

#include <err.h>
#include <puffs.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

PUFFSOP_PROTOS(puffs_null) /* XXX */

static void usage(void);

static void
usage()
{

	errx(1, "usage: %s [-s]�[-o mntopts]�nullpath mountpath",
	    getprogname());
}

int
main(int argc, char *argv[])
{
	struct puffs_usermount *pu;
	struct puffs_ops *pops;
	struct puffs_pathobj *po_root;
	struct statvfs svfsb;
	struct stat sb;
	mntoptparse_t mp;
	int mntflags, pflags, lflags;
	int ch;

	setprogname(argv[0]);

	if (argc < 3)
		usage();

	pflags = lflags = mntflags = 0;
	while ((ch = getopt(argc, argv, "o:s")) != -1) {
		switch (ch) {
		case 'o':
			mp = getmntopts(optarg, puffsmopts, &mntflags, &pflags);
			if (mp == NULL)
				err(1, "getmntopts");
			freemntopts(mp);
			break;
		case 's':
			lflags |= PUFFSLOOP_NODAEMON;
			break;
		}
	}
	pflags |= PUFFS_FLAG_BUILDPATH;
	argv += optind;
	argc -= optind;

	if (pflags & PUFFS_FLAG_OPDUMP)
		lflags = PUFFSLOOP_NODAEMON;

	if (argc != 2)
		usage();

	PUFFSOP_INIT(pops);

	PUFFSOP_SET(pops, puffs_null, fs, statvfs);
	PUFFSOP_SETFSNOP(pops, unmount);
	PUFFSOP_SETFSNOP(pops, sync);

	PUFFSOP_SET(pops, puffs_null, node, lookup);
	PUFFSOP_SET(pops, puffs_null, node, create);
	PUFFSOP_SET(pops, puffs_null, node, mknod);
	PUFFSOP_SET(pops, puffs_null, node, getattr);
	PUFFSOP_SET(pops, puffs_null, node, setattr);
	PUFFSOP_SET(pops, puffs_null, node, fsync);
	PUFFSOP_SET(pops, puffs_null, node, remove);
	PUFFSOP_SET(pops, puffs_null, node, link);
	PUFFSOP_SET(pops, puffs_null, node, rename);
	PUFFSOP_SET(pops, puffs_null, node, mkdir);
	PUFFSOP_SET(pops, puffs_null, node, rmdir);
	PUFFSOP_SET(pops, puffs_null, node, symlink);
	PUFFSOP_SET(pops, puffs_null, node, readlink);
	PUFFSOP_SET(pops, puffs_null, node, readdir);
	PUFFSOP_SET(pops, puffs_null, node, read);
	PUFFSOP_SET(pops, puffs_null, node, write);
	PUFFSOP_SET(pops, puffs_null, node, reclaim);

	if ((pu = puffs_mount(pops, argv[1], mntflags, "pnullfs", NULL,
	    pflags, 0)) == NULL)
		err(1, "mount");

	if (statvfs(argv[0], &svfsb) == -1)
		err(1, "statvfs %s", argv[0]);

	pu->pu_pn_root = puffs_pn_new(pu, NULL);
	if (pu->pu_pn_root == NULL)
		err(1, "puffs_pn_new");

	po_root = puffs_getrootpathobj(pu);
	if (po_root == NULL)
		err(1, "getrootpathobj");
	po_root->po_path = argv[0];
	po_root->po_len = strlen(argv[0]);
	if (stat(argv[0], &sb) == -1)
		err(1, "stat %s", argv[0]);
	puffs_stat2vattr(&pu->pu_pn_root->pn_va, &sb);

	if (puffs_start(pu, pu->pu_pn_root, &svfsb) == -1)
		err(1, "puffs_start");

	if (puffs_mainloop(pu, lflags) == -1)
		err(1, "mainloop");

	return 0;
}
