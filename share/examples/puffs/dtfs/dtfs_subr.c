/*	$NetBSD: dtfs_subr.c,v 1.5 2006/10/27 14:03:52 pooka Exp $	*/

/*
 * Copyright (c) 2006  Antti Kantee.  All Rights Reserved.
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

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <puffs.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "dtfs.h"

void
dtfs_baseattrs(struct vattr *vap, enum vtype type, int32_t fsid, ino_t id)
{
	struct timeval tv;
	struct timespec ts;

	gettimeofday(&tv, NULL);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);

	vap->va_type = type;
	if (type == VDIR) {
		vap->va_mode = 0777;
		vap->va_nlink = 1;	/* n + 1 after adding dent */
	} else {
		vap->va_mode = 0666;
		vap->va_nlink = 0;	/* n + 1 */
	}
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = fsid;
	vap->va_fileid = id;
	vap->va_size = 0;
	vap->va_blocksize = getpagesize();
	vap->va_gen = 1;
	vap->va_flags = 0;
	vap->va_rdev = PUFFS_VNOVAL;
	vap->va_bytes = 0;
	vap->va_filerev = 1;
	vap->va_vaflags = 0;

	vap->va_atime = vap->va_mtime = vap->va_ctime = vap->va_birthtime = ts;
}

/*
 * Well, as you can probably see, this interface has the slight problem
 * of assuming file creation will always be succesful, or at least not
 * giving a reason for the failure.  Be sure to do better when you
 * implement your own fs.
 */
struct puffs_node *
dtfs_genfile(struct puffs_node *dir, const char *name, enum vtype type)
{
	struct dtfs_file *df_dir, *dff;
	struct dtfs_dirent *dfd;
	struct dtfs_mount *dtm;
	struct puffs_node *newpn;

	assert(dir->pn_type == VDIR);
	assert(dir->pn_mnt != NULL);

	if (type == VDIR) {
		dff = dtfs_newdir();
		dff->df_dotdot = dir;
	} else
		dff = dtfs_newfile();

	newpn = puffs_newpnode(dir->pn_mnt, dff, type);
	if (newpn == NULL)
		errx(1, "getnewpnode");
	dtm = dir->pn_mnt->pu_privdata;
	dtfs_baseattrs(&newpn->pn_va, type, dtm->dtm_fsidx.__fsid_val[0],
	    dtm->dtm_nextfileid++);

	df_dir = dir->pn_data;
	dfd = emalloc(sizeof(struct dtfs_dirent));
	dfd->dfd_node = newpn;
	dfd->dfd_name = estrdup(name);
	dfd->dfd_parent = dir;
	dtfs_adddent(dir, dfd);

	return newpn;
}

struct dtfs_file *
dtfs_newdir()
{
	struct dtfs_file *dff;

	dff = emalloc(sizeof(struct dtfs_file));
	memset(dff, 0, sizeof(struct dtfs_file));
	LIST_INIT(&dff->df_dirents);

	return dff;
}

struct dtfs_file *
dtfs_newfile()
{
	struct dtfs_file *dff;

	dff = emalloc(sizeof(struct dtfs_file));
	memset(dff, 0, sizeof(struct dtfs_file));

	return dff;
}

struct dtfs_dirent *
dtfs_dirgetnth(struct dtfs_file *searchdir, int n)
{
	struct dtfs_dirent *dirent;
	int i;

	i = 0;
	LIST_FOREACH(dirent, &searchdir->df_dirents, dfd_entries) {
		if (i == n)
			return dirent;
		i++;
	}

	return NULL;
}

struct dtfs_dirent *
dtfs_dirgetbyname(struct dtfs_file *searchdir, const char *fname)
{
	struct dtfs_dirent *dirent;

	LIST_FOREACH(dirent, &searchdir->df_dirents, dfd_entries)
		if (strcmp(dirent->dfd_name, fname) == 0)
			return dirent;

	return NULL;
}

/*
 * common nuke, kill dirent from parent node
 */
void
dtfs_nukenode(struct puffs_node *nukeme, struct puffs_node *pn_parent,
	const char *fname)
{
	struct dtfs_dirent *dfd;
	struct dtfs_mount *dtm;

	assert(pn_parent->pn_va.va_type == VDIR);

	dfd = dtfs_dirgetbyname(DTFS_PTOF(pn_parent), fname);
	assert(dfd);

	dtm = nukeme->pn_mnt->pu_privdata;
	dtm->dtm_nfiles--;
	assert(dtm->dtm_nfiles >= 1);

	dtfs_removedent(pn_parent, dfd);
	free(dfd);
}

/* free lingering information */
void
dtfs_freenode(struct puffs_node *pn)
{
	struct dtfs_file *df = DTFS_PTOF(pn);
	struct dtfs_mount *dtm;

	assert(pn->pn_va.va_nlink == 0);
	dtm = pn->pn_mnt->pu_privdata;

	switch (pn->pn_type) {
	case VREG:
		assert(dtm->dtm_fsizes >= pn->pn_va.va_size);
		dtm->dtm_fsizes -= pn->pn_va.va_size;
		free(df->df_data);
		break;
	case VLNK:
		free(df->df_linktarget);
		break;
	case VCHR:
	case VBLK:
	case VDIR:
	case VSOCK:
	case VFIFO:
		break;
	default:
		assert(0);
		break;
	}

	free(df);
	puffs_putpnode(pn);
}

void
dtfs_setsize(struct puffs_node *pn, off_t newsize, int extend)
{
	struct dtfs_file *df = DTFS_PTOF(pn);
	struct dtfs_mount *dtm;
	int more; /* too tired to think about signed/unsigned promotions */

	more = newsize > pn->pn_va.va_size;
	if (extend && !more)
		return;

	df->df_data = erealloc(df->df_data, newsize);
	/* if extended, set storage to zero to match correct behaviour */ 
	if (more)
		memset(df->df_data+df->df_datalen, 0, newsize-df->df_datalen);
	df->df_datalen = newsize;

	dtm = pn->pn_mnt->pu_privdata;
	if (more) {
		dtm->dtm_fsizes += newsize - pn->pn_va.va_size;
	} else {
		assert(dtm->dtm_fsizes >= pn->pn_va.va_size - newsize);
		dtm->dtm_fsizes -= pn->pn_va.va_size - newsize;
	}

	pn->pn_va.va_size = newsize;
	pn->pn_va.va_bytes = newsize;
}

/* add & bump link count */
void
dtfs_adddent(struct puffs_node *pn_dir, struct dtfs_dirent *dent)
{
	struct dtfs_file *dir = DTFS_PTOF(pn_dir);
	struct puffs_node *pn_file = dent->dfd_node;
	struct dtfs_file *file = DTFS_PTOF(pn_file);
	struct dtfs_mount *dtm;

	assert(pn_dir->pn_type == VDIR);
	LIST_INSERT_HEAD(&dir->df_dirents, dent, dfd_entries);
	pn_dir->pn_va.va_nlink++;
	pn_file->pn_va.va_nlink++;

	dtm = pn_file->pn_mnt->pu_privdata;
	dtm->dtm_nfiles++;

	dent->dfd_parent = pn_dir;
	if (dent->dfd_node->pn_type == VDIR)
		file->df_dotdot = pn_dir;

	dtfs_updatetimes(pn_dir, 0, 1, 1);
}

/* remove & lower link count */
void
dtfs_removedent(struct puffs_node *pn_dir, struct dtfs_dirent *dent)
{
	struct puffs_node *pn_file = dent->dfd_node;

	assert(pn_dir->pn_type == VDIR);
	LIST_REMOVE(dent, dfd_entries);
	pn_dir->pn_va.va_nlink--;
	pn_file->pn_va.va_nlink--;
	assert(pn_dir->pn_va.va_nlink >= 2);

	dtfs_updatetimes(pn_dir, 0, 1, 1);
}

void
dtfs_updatetimes(struct puffs_node *pn, int doatime, int doctime, int domtime)
{
	struct timeval tv;
	struct timespec ts;

	gettimeofday(&tv, NULL);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);

	if (doatime)
		pn->pn_va.va_atime = ts;
	if (doctime)
		pn->pn_va.va_ctime = ts;
	if (domtime)
		pn->pn_va.va_mtime = ts;
}
