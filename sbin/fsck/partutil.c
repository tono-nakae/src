/*	$NetBSD: partutil.c,v 1.6 2009/06/06 17:47:50 haad Exp $	*/

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
__RCSID("$NetBSD: partutil.c,v 1.6 2009/06/06 17:47:50 haad Exp $");

#include <sys/types.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>


#include <disktab.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <util.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <prop/proplib.h>

#include "partutil.h"


/*
 * Set what we need to know about disk geometry.
 */
static void
dict2geom(struct disk_geom *geo, prop_dictionary_t dict)
{
	memset(geo, 0, sizeof(struct disk_geom));
	prop_dictionary_get_int64(dict, "sectors-per-unit", &geo->dg_secperunit);
	prop_dictionary_get_uint32(dict, "sector-size", &geo->dg_secsize);
	prop_dictionary_get_uint32(dict, "sectors-per-track", &geo->dg_nsectors);
	prop_dictionary_get_uint32(dict, "tracks-per-cylinder", &geo->dg_ntracks);
	prop_dictionary_get_uint32(dict, "cylinders-per-unit", &geo->dg_ncylinders);
}


static void
part2wedge(struct dkwedge_info *dkw, const struct disklabel *lp, const char *s)
{
	struct stat sb;
	const struct partition *pp;
	int ptn;

	(void)memset(dkw, 0, sizeof(*dkw));
	if (stat(s, &sb) == -1)
		return;

	ptn = strchr(s, '\0')[-1] - 'a';
	if ((unsigned)ptn >= lp->d_npartitions ||
	    (devminor_t)ptn != DISKPART(sb.st_rdev))
		return;

	pp = &lp->d_partitions[ptn];
	dkw->dkw_offset = pp->p_offset;
	dkw->dkw_size = pp->p_size;
	dkw->dkw_parent[0] = '*';
	switch (pp->p_fstype) {
	default:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_UNKNOWN);
		break;
	case FS_UNUSED:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_UNUSED);
		break;
	case FS_SWAP:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_SWAP);
		break;
	case FS_BSDFFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_FFS);
		break;
	case FS_BSDLFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_LFS);
		break;
	case FS_EX2FS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_EXT2FS);
		break;
	case FS_ISO9660:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_ISO9660);
		break;
	case FS_ADOS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_AMIGADOS);
		break;
	case FS_HFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_APPLEHFS);
		break;
	case FS_MSDOS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_FAT);
		break;
	case FS_FILECORE:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_FILECORE);
		break;
	case FS_APPLEUFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_APPLEUFS);
		break;
	case FS_NTFS:
		(void)strcpy(dkw->dkw_ptype, DKW_PTYPE_NTFS);
		break;
	}
}

int
getdiskinfo(const char *s, int fd, const char *dt, struct disk_geom *geo,
    struct dkwedge_info *dkw)
{
	struct disklabel lab;
	struct disklabel *lp = &lab;
	prop_dictionary_t disk_dict, geom_dict;

	if (dt) {
		lp = getdiskbyname(dt);
		if (lp == NULL)
			errx(1, "%s: unknown disk type", dt);
	}

	/* Get disk description dictionary */
	if (prop_dictionary_recv_ioctl(fd, DIOCGDISKINFO, &disk_dict) != 0) {
		warn("Please implement DIOCGDISKINFO for %s\n disk driver\n", s);
		return (errno);
	}
	
	geom_dict = prop_dictionary_get(disk_dict, "geometry");
	dict2geom(geo, geom_dict);

	/* Get info about partition/wedge */
	if (ioctl(fd, DIOCGWEDGEINFO, dkw) == -1) {
		if (ioctl(fd, DIOCGDINFO, lp) == -1)
			errx(errno, "Please implement DIOCGWEDGEINFO or DIOCGDINFO for disk device %s\n", s);

		part2wedge(dkw, lp, s);
	}

	return 0;
}
