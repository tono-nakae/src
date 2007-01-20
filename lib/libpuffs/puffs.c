/*	$NetBSD: puffs.c,v 1.26 2007/01/20 13:52:14 pooka Exp $	*/

/*
 * Copyright (c) 2005, 2006  Antti Kantee.  All Rights Reserved.
 *
 * Development of this software was supported by the
 * Google Summer of Code program and the Ulla Tuominen Foundation.
 * The Google SoC project was mentored by Bill Studenmund.
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

#include <sys/cdefs.h>
#if !defined(lint)
__RCSID("$NetBSD: puffs.c,v 1.26 2007/01/20 13:52:14 pooka Exp $");
#endif /* !lint */

#include <sys/param.h>
#include <sys/mount.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <mntopts.h>
#include <puffs.h>
#include <puffsdump.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "puffs_priv.h"

/* Most file systems want this for opts, so just give it to them */
const struct mntopt puffsmopts[] = {
	MOPT_STDOPTS,
	PUFFSMOPT_STD,
	MOPT_NULL,
};

#define FILLOP(lower, upper)						\
do {									\
	if (pops->puffs_node_##lower)					\
		opmask[PUFFS_VN_##upper] = 1;				\
} while (/*CONSTCOND*/0)
static void
fillvnopmask(struct puffs_ops *pops, uint8_t *opmask)
{

	memset(opmask, 0, PUFFS_VN_MAX);

	FILLOP(create,   CREATE);
	FILLOP(mknod,    MKNOD);
	FILLOP(open,     OPEN);
	FILLOP(close,    CLOSE);
	FILLOP(access,   ACCESS);
	FILLOP(getattr,  GETATTR);
	FILLOP(setattr,  SETATTR);
	FILLOP(poll,     POLL); /* XXX: not ready in kernel */
	FILLOP(mmap,     MMAP);
	FILLOP(fsync,    FSYNC);
	FILLOP(seek,     SEEK);
	FILLOP(remove,   REMOVE);
	FILLOP(link,     LINK);
	FILLOP(rename,   RENAME);
	FILLOP(mkdir,    MKDIR);
	FILLOP(rmdir,    RMDIR);
	FILLOP(symlink,  SYMLINK);
	FILLOP(readdir,  READDIR);
	FILLOP(readlink, READLINK);
	FILLOP(reclaim,  RECLAIM);
	FILLOP(inactive, INACTIVE);
	FILLOP(print,    PRINT);
	FILLOP(read,     READ);
	FILLOP(write,    WRITE);

	/* XXX: not implemented in the kernel */
	FILLOP(getextattr, GETEXTATTR);
	FILLOP(setextattr, SETEXTATTR);
	FILLOP(listextattr, LISTEXTATTR);
}
#undef FILLOP

int
puffs_getselectable(struct puffs_usermount *pu)
{

	return pu->pu_fd;
}

int
puffs_setblockingmode(struct puffs_usermount *pu, int mode)
{
	int x;

	x = mode;
	return ioctl(pu->pu_fd, FIONBIO, &x);
}

int
puffs_getstate(struct puffs_usermount *pu)
{

	return pu->pu_state;
}

void
puffs_setstacksize(struct puffs_usermount *pu, size_t ss)
{

	pu->pu_cc_stacksize = ss;
}

struct puffs_pathobj *
puffs_getrootpathobj(struct puffs_usermount *pu)
{
	struct puffs_node *pnr;

	pnr = pu->pu_pn_root;
	if (pnr == NULL) {
		errno = ENOENT;
		return NULL;
	}

	return &pnr->pn_po;
}


void
puffs_set_pathbuild(struct puffs_usermount *pu, pu_pathbuild_fn fn)
{

	pu->pu_pathbuild = fn;
}

void
puffs_set_pathtransform(struct puffs_usermount *pu, pu_pathtransform_fn fn)
{

	pu->pu_pathtransform = fn;
}

void
puffs_set_pathcmp(struct puffs_usermount *pu, pu_pathcmp_fn fn)
{

	pu->pu_pathcmp = fn;
}

void
puffs_set_pathfree(struct puffs_usermount *pu, pu_pathfree_fn fn)
{

	pu->pu_pathfree = fn;
}

void
puffs_set_namemod(struct puffs_usermount *pu, pu_namemod_fn fn)
{

	pu->pu_namemod = fn;
}

enum {PUFFCALL_ANSWER, PUFFCALL_IGNORE, PUFFCALL_AGAIN};

struct puffs_usermount *
_puffs_mount(int develv, struct puffs_ops *pops, const char *dir, int mntflags,
	const char *puffsname, void *priv, uint32_t pflags, size_t maxreqlen)
{
	struct puffs_args pargs;
	struct puffs_usermount *pu;
	int fd = 0;

	if (develv != PUFFS_DEVEL_LIBVERSION) {
		warnx("puffs_mount: mounting with lib version %d, need %d",
		    develv, PUFFS_DEVEL_LIBVERSION);
		errno = EINVAL;
		return NULL;
	}

	fd = open("/dev/puffs", O_RDONLY);
	if (fd == -1)
		return NULL;
	if (fd <= 2)
		warnx("puffs_mount: device fd %d (<= 2), sure this is "
		    "what you want?", fd);

	pargs.pa_vers = PUFFSDEVELVERS | PUFFSVERSION;
	pargs.pa_flags = PUFFS_FLAG_KERN(pflags);
	pargs.pa_fd = fd;
	pargs.pa_maxreqlen = maxreqlen;
	fillvnopmask(pops, pargs.pa_vnopmask);
	(void)strlcpy(pargs.pa_name, puffsname, sizeof(pargs.pa_name));

	pu = malloc(sizeof(struct puffs_usermount));
	if (!pu)
		return NULL;

	pu->pu_flags = pflags;
	pu->pu_ops = *pops;
	free(pops); /* XXX */
	pu->pu_fd = fd;
	pu->pu_privdata = priv;
	pu->pu_cc_stacksize = PUFFS_CC_STACKSIZE_DEFAULT;
	LIST_INIT(&pu->pu_pnodelst);

	/* defaults for some user-settable translation functions */
	pu->pu_cmap = NULL; /* identity translation */

	pu->pu_pathbuild = puffs_path_buildpath;
	pu->pu_pathfree = puffs_path_freepath;
	pu->pu_pathcmp = puffs_path_cmppath;
	pu->pu_pathtransform = NULL;
	pu->pu_namemod = NULL;

	pu->pu_state = PUFFS_STATE_MOUNTING;
	if (mount(MOUNT_PUFFS, dir, mntflags, &pargs) == -1)
		goto failfree;
	pu->pu_maxreqlen = pargs.pa_maxreqlen;

	return pu;

 failfree:
	/* can't unmount() from here for obvious reasons */
	if (fd)
		close(fd);
	free(pu);
	return NULL;
}

int
puffs_start(struct puffs_usermount *pu, void *rootcookie, struct statvfs *sbp)
{
	struct puffs_startreq sreq;

	memset(&sreq, 0, sizeof(struct puffs_startreq));
	sreq.psr_cookie = rootcookie;
	sreq.psr_sb = *sbp;

	/* tell kernel we're flying */
	if (ioctl(pu->pu_fd, PUFFSSTARTOP, &sreq) == -1)
		return -1;

	pu->pu_state = PUFFS_STATE_RUNNING;

	return 0;
}

/*
 * XXX: there's currently no clean way to request unmount from
 * within the user server, so be very brutal about it.
 */
/*ARGSUSED*/
int
puffs_exit(struct puffs_usermount *pu, int force)
{
	struct puffs_node *pn, *pn_next;

	force = 1; /* currently */

	if (pu->pu_fd)
		close(pu->pu_fd);

	pn = LIST_FIRST(&pu->pu_pnodelst);
	while (pn) {
		pn_next = LIST_NEXT(pn, pn_entries);
		puffs_pn_put(pn);
		pn = pn_next;
	}
	free(pu);

	return 0; /* always succesful for now, WILL CHANGE */
}

int
puffs_mainloop(struct puffs_usermount *pu, int flags)
{
	struct puffs_getreq *pgr;
	struct puffs_putreq *ppr;
	int rv;

	rv = -1;
	pgr = puffs_req_makeget(pu, pu->pu_maxreqlen, 0);
	if (pgr == NULL)
		return -1;

	ppr = puffs_req_makeput(pu);
	if (ppr == NULL) {
		puffs_req_destroyget(pgr);
		return -1;
	}

	if ((flags & PUFFSLOOP_NODAEMON) == 0)
		if (daemon(1, 0) == -1)
			goto out;

	/* XXX: should be a bit more robust with errors here */
	while (puffs_getstate(pu) == PUFFS_STATE_RUNNING
	    || puffs_getstate(pu) == PUFFS_STATE_UNMOUNTING) {
		puffs_req_resetput(ppr);

		if (puffs_req_handle(pu, pgr, ppr, 0) == -1) {
			rv = -1;
			break;
		}
		if (puffs_req_putput(ppr) == -1) {
			rv = -1;
			break;
		}
	}

 out:
	puffs_req_destroyput(ppr);
	puffs_req_destroyget(pgr);
	return rv;
}

int
puffs_dopreq(struct puffs_usermount *pu, struct puffs_putreq *ppr,
	struct puffs_req *preq)
{
	struct puffs_cc *pcc;
	int rv;

	if (pu->pu_flags & PUFFS_FLAG_OPDUMP)
		puffsdump_req(preq);

	pcc = puffs_cc_create(pu);

	/* XXX: temporary kludging */
	pcc->pcc_preq = malloc(preq->preq_buflen);
	if (pcc->pcc_preq == NULL)
		return -1;
	(void) memcpy(pcc->pcc_preq, preq, preq->preq_buflen);

	rv = puffs_docc(ppr, pcc);

	if ((pcc->pcc_flags & PCC_DONE) == 0)
		return 0;

	return rv;
}

int
puffs_docc(struct puffs_putreq *ppr, struct puffs_cc *pcc)
{
	int rv;

	assert((pcc->pcc_flags & PCC_DONE) == 0);

	puffs_cc_continue(pcc);
	rv = pcc->pcc_rv;

	if ((pcc->pcc_flags & PCC_DONE) == 0)
		rv = PUFFCALL_AGAIN;

	/* check if we need to store this reply */
	switch (rv) {
	case PUFFCALL_ANSWER:
		puffs_req_putcc(ppr, pcc);
		break;
	case PUFFCALL_IGNORE:
		puffs_cc_destroy(pcc);
		break;
	case PUFFCALL_AGAIN:
		break;
	default:
		assert(/*CONSTCOND*/0);
	}

	return 0;
}

/* library private, but linked from callcontext.c */

void
puffs_calldispatcher(struct puffs_cc *pcc)
{
	struct puffs_usermount *pu = pcc->pcc_pu;
	struct puffs_ops *pops = &pu->pu_ops;
	struct puffs_req *preq = pcc->pcc_preq;
	void *auxbuf = preq; /* help with typecasting */
	int error, rv, buildpath;

	assert(pcc->pcc_flags & (PCC_ONCE | PCC_REALCC));

	if (PUFFSOP_WANTREPLY(preq->preq_opclass))
		rv = PUFFCALL_ANSWER;
	else
		rv = PUFFCALL_IGNORE;

	buildpath = pu->pu_flags & PUFFS_FLAG_BUILDPATH;

	if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VFS) {
		switch (preq->preq_optype) {
		case PUFFS_VFS_UNMOUNT:
		{
			struct puffs_vfsreq_unmount *auxt = auxbuf;

			pu->pu_state = PUFFS_STATE_UNMOUNTING;
			error = pops->puffs_fs_unmount(pcc,
			    auxt->pvfsr_flags, auxt->pvfsr_pid);
			if (!error)
				pu->pu_state = PUFFS_STATE_UNMOUNTED;
			else
				pu->pu_state = PUFFS_STATE_RUNNING;
			break;
		}
		case PUFFS_VFS_STATVFS:
		{
			struct puffs_vfsreq_statvfs *auxt = auxbuf;

			error = pops->puffs_fs_statvfs(pcc,
			    &auxt->pvfsr_sb, auxt->pvfsr_pid);
			break;
		}
		case PUFFS_VFS_SYNC:
		{
			struct puffs_vfsreq_sync *auxt = auxbuf;

			error = pops->puffs_fs_sync(pcc,
			    auxt->pvfsr_waitfor, &auxt->pvfsr_cred,
			    auxt->pvfsr_pid);
			break;
		}
		default:
			/*
			 * I guess the kernel sees this one coming
			 */
			error = EINVAL;
			break;
		}

	/* XXX: audit return values */
	/* XXX: sync with kernel */
	} else if (PUFFSOP_OPCLASS(preq->preq_opclass) == PUFFSOP_VN) {
		switch (preq->preq_optype) {
		case PUFFS_VN_LOOKUP:
		{
			struct puffs_vnreq_lookup *auxt = auxbuf;
			struct puffs_cn pcn;

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			if (buildpath) {
				if (pcn.pcn_flags & PUFFS_ISDOTDOT) {
					buildpath = 0;
				} else {
					error = puffs_path_pcnbuild(pu, &pcn,
					    preq->preq_cookie);
					if (error)
						break;
				}
			}

			/* lookup *must* be present */
			error = pops->puffs_node_lookup(pcc, preq->preq_cookie,
			    &auxt->pvnr_newnode, &auxt->pvnr_vtype,
			    &auxt->pvnr_size, &auxt->pvnr_rdev, &pcn);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					struct puffs_node *pn;

					/*
					 * did we get a new node or a
					 * recycled node?
					 * XXX: mapping assumption
					 */
					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					if (pn->pn_po.po_path == NULL)
						pn->pn_po = pcn.pcn_po_full;
					else
						pu->pu_pathfree(pu,
						    &pcn.pcn_po_full);
				}
			}

			break;
		}

		case PUFFS_VN_CREATE:
		{
			struct puffs_vnreq_create *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_create == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn,
				    preq->preq_cookie);
				if (error)
					break;
			}

			error = pops->puffs_node_create(pcc,
			    preq->preq_cookie, &auxt->pvnr_newnode,
			    &pcn, &auxt->pvnr_va);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					struct puffs_node *pn;

					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			break;
		}

		case PUFFS_VN_MKNOD:
		{
			struct puffs_vnreq_mknod *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_mknod == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn,
				    preq->preq_cookie);
				if (error)
					break;
			}

			error = pops->puffs_node_mknod(pcc,
			    preq->preq_cookie, &auxt->pvnr_newnode,
			    &pcn, &auxt->pvnr_va);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					struct puffs_node *pn;

					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			break;
		}

		case PUFFS_VN_OPEN:
		{
			struct puffs_vnreq_open *auxt = auxbuf;
			if (pops->puffs_node_open == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_open(pcc,
			    preq->preq_cookie, auxt->pvnr_mode,
			    &auxt->pvnr_cred, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_CLOSE:
		{
			struct puffs_vnreq_close *auxt = auxbuf;
			if (pops->puffs_node_close == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_close(pcc,
			    preq->preq_cookie, auxt->pvnr_fflag,
			    &auxt->pvnr_cred, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_ACCESS:
		{
			struct puffs_vnreq_access *auxt = auxbuf;
			if (pops->puffs_node_access == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_access(pcc,
			    preq->preq_cookie, auxt->pvnr_mode,
			    &auxt->pvnr_cred, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_GETATTR:
		{
			struct puffs_vnreq_getattr *auxt = auxbuf;
			if (pops->puffs_node_getattr == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_getattr(pcc,
			    preq->preq_cookie, &auxt->pvnr_va,
			    &auxt->pvnr_cred, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_SETATTR:
		{
			struct puffs_vnreq_setattr *auxt = auxbuf;
			if (pops->puffs_node_setattr == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_setattr(pcc,
			    preq->preq_cookie, &auxt->pvnr_va,
			    &auxt->pvnr_cred, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_MMAP:
		{
			struct puffs_vnreq_mmap *auxt = auxbuf;
			if (pops->puffs_node_mmap == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_mmap(pcc,
			    preq->preq_cookie, auxt->pvnr_fflags,
			    &auxt->pvnr_cred, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_FSYNC:
		{
			struct puffs_vnreq_fsync *auxt = auxbuf;
			if (pops->puffs_node_fsync == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_fsync(pcc,
			    preq->preq_cookie, &auxt->pvnr_cred,
			    auxt->pvnr_flags, auxt->pvnr_offlo,
			    auxt->pvnr_offhi, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_SEEK:
		{
			struct puffs_vnreq_seek *auxt = auxbuf;
			if (pops->puffs_node_seek == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_seek(pcc,
			    preq->preq_cookie, auxt->pvnr_oldoff,
			    auxt->pvnr_newoff, &auxt->pvnr_cred);
			break;
		}

		case PUFFS_VN_REMOVE:
		{
			struct puffs_vnreq_remove *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_remove == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;

			error = pops->puffs_node_remove(pcc,
			    preq->preq_cookie, auxt->pvnr_cookie_targ, &pcn);
			break;
		}

		case PUFFS_VN_LINK:
		{
			struct puffs_vnreq_link *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_link == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn,
				    preq->preq_cookie);
				if (error)
					break;
			}

			error = pops->puffs_node_link(pcc,
			    preq->preq_cookie, auxt->pvnr_cookie_targ, &pcn);
			if (buildpath)
				pu->pu_pathfree(pu, &pcn.pcn_po_full);

			break;
		}

		case PUFFS_VN_RENAME:
		{
			struct puffs_vnreq_rename *auxt = auxbuf;
			struct puffs_cn pcn_src, pcn_targ;
			struct puffs_node *pn_src;

			if (pops->puffs_node_rename == NULL) {
				error = 0;
				break;
			}

			pcn_src.pcn_pkcnp = &auxt->pvnr_cn_src;
			pcn_targ.pcn_pkcnp = &auxt->pvnr_cn_targ;
			if (buildpath) {
				pn_src = auxt->pvnr_cookie_src;
				pcn_src.pcn_po_full = pn_src->pn_po;

				error = puffs_path_pcnbuild(pu, &pcn_targ,
				    auxt->pvnr_cookie_targdir);
				if (error)
					break;
			}

			error = pops->puffs_node_rename(pcc,
			    preq->preq_cookie, auxt->pvnr_cookie_src,
			    &pcn_src, auxt->pvnr_cookie_targdir,
			    auxt->pvnr_cookie_targ, &pcn_targ);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu,
					    &pcn_targ.pcn_po_full);
				} else {
					struct puffs_pathinfo pi;
					struct puffs_pathobj po_old;

					/* handle this node */
					po_old = pn_src->pn_po;
					pn_src->pn_po = pcn_targ.pcn_po_full;

					if (pn_src->pn_va.va_type != VDIR) {
						pu->pu_pathfree(pu, &po_old);
						break;
					}

					/* handle all child nodes for DIRs */
					pi.pi_old = &pcn_src.pcn_po_full;
					pi.pi_new = &pcn_targ.pcn_po_full;

					if (puffs_pn_nodewalk(pu,
					    puffs_path_prefixadj, &pi) != NULL)
						error = ENOMEM;
					pu->pu_pathfree(pu, &po_old);
				}
			}
			break;
		}

		case PUFFS_VN_MKDIR:
		{
			struct puffs_vnreq_mkdir *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_mkdir == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn,
				    preq->preq_cookie);
				if (error)
					break;
			}

			error = pops->puffs_node_mkdir(pcc,
			    preq->preq_cookie, &auxt->pvnr_newnode,
			    &pcn, &auxt->pvnr_va);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					struct puffs_node *pn;

					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			break;
		}

		case PUFFS_VN_RMDIR:
		{
			struct puffs_vnreq_rmdir *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_rmdir == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;

			error = pops->puffs_node_rmdir(pcc,
			    preq->preq_cookie, auxt->pvnr_cookie_targ, &pcn);
			break;
		}

		case PUFFS_VN_SYMLINK:
		{
			struct puffs_vnreq_symlink *auxt = auxbuf;
			struct puffs_cn pcn;
			if (pops->puffs_node_symlink == NULL) {
				error = 0;
				break;
			}

			pcn.pcn_pkcnp = &auxt->pvnr_cn;
			if (buildpath) {
				error = puffs_path_pcnbuild(pu, &pcn,
				    preq->preq_cookie);
				if (error)
					break;
			}

			error = pops->puffs_node_symlink(pcc,
			    preq->preq_cookie, &auxt->pvnr_newnode,
			    &pcn, &auxt->pvnr_va, auxt->pvnr_link);

			if (buildpath) {
				if (error) {
					pu->pu_pathfree(pu, &pcn.pcn_po_full);
				} else {
					struct puffs_node *pn;

					pn = PU_CMAP(pu, auxt->pvnr_newnode);
					pn->pn_po = pcn.pcn_po_full;
				}
			}

			break;
		}

		case PUFFS_VN_READDIR:
		{
			struct puffs_vnreq_readdir *auxt = auxbuf;
			size_t res;

			if (pops->puffs_node_readdir == NULL) {
				error = 0;
				break;
			}

			res = auxt->pvnr_resid;
			error = pops->puffs_node_readdir(pcc,
			    preq->preq_cookie, auxt->pvnr_dent,
			    &auxt->pvnr_cred, &auxt->pvnr_offset,
			    &auxt->pvnr_resid);

			/* need to move a bit more */
			preq->preq_buflen = sizeof(struct puffs_vnreq_readdir) 
			    + (res - auxt->pvnr_resid);
			break;
		}

		case PUFFS_VN_READLINK:
		{
			struct puffs_vnreq_readlink *auxt = auxbuf;
			if (pops->puffs_node_readlink == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_readlink(pcc,
			    preq->preq_cookie, &auxt->pvnr_cred,
			    auxt->pvnr_link, &auxt->pvnr_linklen);
			break;
		}

		case PUFFS_VN_RECLAIM:
		{
			struct puffs_vnreq_reclaim *auxt = auxbuf;
			if (pops->puffs_node_reclaim == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_reclaim(pcc,
			    preq->preq_cookie, auxt->pvnr_pid);
			break;
		}

		case PUFFS_VN_INACTIVE:
		{
			struct puffs_vnreq_inactive *auxt = auxbuf;
			if (pops->puffs_node_inactive == NULL) {
				error = EOPNOTSUPP;
				break;
			}

			error = pops->puffs_node_inactive(pcc,
			    preq->preq_cookie, auxt->pvnr_pid,
			    &auxt->pvnr_backendrefs);
			break;
		}

		case PUFFS_VN_PATHCONF:
		{
			struct puffs_vnreq_pathconf *auxt = auxbuf;
			if (pops->puffs_node_pathconf == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_pathconf(pcc,
			    preq->preq_cookie, auxt->pvnr_name,
			    &auxt->pvnr_retval);
			break;
		}

		case PUFFS_VN_ADVLOCK:
		{
			struct puffs_vnreq_advlock *auxt = auxbuf;
			if (pops->puffs_node_advlock == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_advlock(pcc,
			    preq->preq_cookie, auxt->pvnr_id, auxt->pvnr_op,
			    &auxt->pvnr_fl, auxt->pvnr_flags);
			break;
		}

		case PUFFS_VN_PRINT:
		{
			if (pops->puffs_node_print == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_print(pcc,
			    preq->preq_cookie);
			break;
		}

		case PUFFS_VN_READ:
		{
			struct puffs_vnreq_read *auxt = auxbuf;
			size_t res;

			if (pops->puffs_node_read == NULL) {
				error = EIO;
				break;
			}

			res = auxt->pvnr_resid;
			error = pops->puffs_node_read(pcc,
			    preq->preq_cookie, auxt->pvnr_data,
			    auxt->pvnr_offset, &auxt->pvnr_resid,
			    &auxt->pvnr_cred, auxt->pvnr_ioflag);

			/* need to move a bit more */
			preq->preq_buflen = sizeof(struct puffs_vnreq_read)
			    + (res - auxt->pvnr_resid);
			break;
		}

		case PUFFS_VN_WRITE:
		{
			struct puffs_vnreq_write *auxt = auxbuf;

			if (pops->puffs_node_write == NULL) {
				error = EIO;
				break;
			}

			error = pops->puffs_node_write(pcc,
			    preq->preq_cookie, auxt->pvnr_data,
			    auxt->pvnr_offset, &auxt->pvnr_resid,
			    &auxt->pvnr_cred, auxt->pvnr_ioflag);

			/* don't need to move data back to the kernel */
			preq->preq_buflen = sizeof(struct puffs_vnreq_write);
			break;
		}

/* holy bitrot, ryydman! */
#if 0
		case PUFFS_VN_IOCTL:
			error = pops->puffs_node_ioctl1(pcc, preq->preq_cookie,
			     (struct puffs_vnreq_ioctl *)auxbuf, &pop);
			if (error != 0)
				break;
			pop.pso_reqid = preq->preq_id;

			/* let the kernel do it's intermediate duty */
			error = ioctl(pu->pu_fd, PUFFSSIZEOP, &pop);
			/*
			 * XXX: I don't actually know what the correct
			 * thing to do in case of an error is, so I'll
			 * just ignore it for the time being.
			 */
			error = pops->puffs_node_ioctl2(pcc, preq->preq_cookie,
			    (struct puffs_vnreq_ioctl *)auxbuf, &pop);
			break;

		case PUFFS_VN_FCNTL:
			error = pops->puffs_node_fcntl1(pcc, preq->preq_cookie,
			     (struct puffs_vnreq_fcntl *)auxbuf, &pop);
			if (error != 0)
				break;
			pop.pso_reqid = preq->preq_id;

			/* let the kernel do it's intermediate duty */
			error = ioctl(pu->pu_fd, PUFFSSIZEOP, &pop);
			/*
			 * XXX: I don't actually know what the correct
			 * thing to do in case of an error is, so I'll
			 * just ignore it for the time being.
			 */
			error = pops->puffs_node_fcntl2(pcc, preq->preq_cookie,
			    (struct puffs_vnreq_fcntl *)auxbuf, &pop);
			break;
#endif

		default:
			printf("inval op %d\n", preq->preq_optype);
			error = EINVAL;
			break;
		}
	} else {
		/*
		 * this one also
		 */
		error = EINVAL;
	}

	preq->preq_rv = error;

	pcc->pcc_rv = rv;
	pcc->pcc_flags |= PCC_DONE;
}


#if 0
		case PUFFS_VN_POLL:
		{
			struct puffs_vnreq_poll *auxt = auxbuf;
			if (pops->puffs_node_poll == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_poll(pcc,
			    preq->preq_cookie, preq-);
			break;
		}

		case PUFFS_VN_KQFILTER:
		{
			struct puffs_vnreq_kqfilter *auxt = auxbuf;
			if (pops->puffs_node_kqfilter == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_node_kqfilter(pcc,
			    preq->preq_cookie, );
			break;
		}

		case PUFFS_VN_CLOSEEXTATTR:
		{
			struct puffs_vnreq_closeextattr *auxt = auxbuf;
			if (pops->puffs_closeextattr == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_closeextattr(pcc,
			    preq->preq_cookie, );
			break;
		}

		case PUFFS_VN_GETEXTATTR:
		{
			struct puffs_vnreq_getextattr *auxt = auxbuf;
			if (pops->puffs_getextattr == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_getextattr(pcc,
			    preq->preq_cookie, );
			break;
		}

		case PUFFS_VN_LISTEXTATTR:
		{
			struct puffs_vnreq_listextattr *auxt = auxbuf;
			if (pops->puffs_listextattr == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_listextattr(pcc,
			    preq->preq_cookie, );
			break;
		}

		case PUFFS_VN_OPENEXTATTR:
		{
			struct puffs_vnreq_openextattr *auxt = auxbuf;
			if (pops->puffs_openextattr == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_openextattr(pcc,
			    preq->preq_cookie, );
			break;
		}

		case PUFFS_VN_DELETEEXTATTR:
		{
			struct puffs_vnreq_deleteextattr *auxt = auxbuf;
			if (pops->puffs_deleteextattr == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_deleteextattr(pcc,
			    preq->preq_cookie, );
			break;
		}

		case PUFFS_VN_SETEXTATTR:
		{
			struct puffs_vnreq_setextattr *auxt = auxbuf;
			if (pops->puffs_setextattr == NULL) {
				error = 0;
				break;
			}

			error = pops->puffs_setextattr(pcc,
			    preq->preq_cookie, );
			break;
		}

#endif
