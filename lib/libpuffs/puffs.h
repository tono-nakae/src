/*	$NetBSD: puffs.h,v 1.22 2007/01/10 20:11:04 pooka Exp $	*/

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

#ifndef _PUFFS_H_
#define _PUFFS_H_

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sys/vnode.h>

#include <fs/puffs/puffs_msgif.h>

#include <mntopts.h>
#include <string.h>

/* forwards */
struct puffs_cc;

struct puffs_getreq;
struct puffs_putreq;

/* XXX: might disappear from here */
struct puffs_node {
	off_t		pn_size;
	int		pn_flag;
	struct vattr	pn_va;

	void		*pn_data;		/* private data		*/

	char 		*pn_path;		/* PUFFS_FLAG_BUILDPATH */
	size_t		pn_plen;

	struct puffs_usermount *pn_mnt;

	LIST_ENTRY(puffs_node) pn_entries;
};


struct puffs_usermount;

/*
 * megaXXX: these are values from inside _KERNEL
 * need to work on the translation for ALL the necessary values.
 */
#define PUFFS_VNOVAL (-1)
#define PUFFS_ISDOTDOT 0x2000
#define PUFFS_IO_APPEND 0x20

/*
 * Magic constants
 */
#define PUFFS_CC_STACKSIZE_DEFAULT (1024*1024)

struct puffs_cn {
	struct puffs_kcn	*pcn_pkcnp;	/* kernel input */

	/* XXX: this will be a more complex object some day */
	char			*pcn_fullpath;	/* if PUFFS_FLAG_BUILDPATH */
	size_t			pcn_fullplen;
};
#define pcn_nameiop	pcn_pkcnp->pkcn_nameiop
#define pcn_flags	pcn_pkcnp->pkcn_flags
#define pcn_pid		pcn_pkcnp->pkcn_pid
#define pcn_cred	pcn_pkcnp->pkcn_cred
#define pcn_name	pcn_pkcnp->pkcn_name
#define pcn_namelen	pcn_pkcnp->pkcn_namelen


/*
 * Puffs options to mount
 */
/* kernel */
#define	PUFFSMOPT_CACHE		{ "cache", 1, PUFFS_KFLAG_NOCACHE, 1 }
#define PUFFSMOPT_ALLOPS	{ "allops", 0, PUFFS_KFLAG_ALLOPS, 1 }

/* libpuffs */
#define PUFFSMOPT_DUMP		{ "dump", 0, PUFFS_FLAG_OPDUMP, 1 }

#define PUFFSMOPT_STD							\
	PUFFSMOPT_CACHE,						\
	PUFFSMOPT_ALLOPS,						\
	PUFFSMOPT_DUMP

extern const struct mntopt puffsmopts[]; /* puffs.c */

/* callbacks for operations */
struct puffs_ops {
	int (*puffs_fs_unmount)(struct puffs_cc *, int, pid_t);
	int (*puffs_fs_statvfs)(struct puffs_cc *,
	    struct statvfs *, pid_t);
	int (*puffs_fs_sync)(struct puffs_cc *, int,
	    const struct puffs_cred *, pid_t);

	int (*puffs_node_lookup)(struct puffs_cc *,
	    void *, void **, enum vtype *, voff_t *, dev_t *,
	    const struct puffs_cn *);
	int (*puffs_node_create)(struct puffs_cc *,
	    void *, void **, const struct puffs_cn *, const struct vattr *);
	int (*puffs_node_mknod)(struct puffs_cc *,
	    void *, void **, const struct puffs_cn *, const struct vattr *);
	int (*puffs_node_open)(struct puffs_cc *,
	    void *, int, const struct puffs_cred *, pid_t);
	int (*puffs_node_close)(struct puffs_cc *,
	    void *, int, const struct puffs_cred *, pid_t);
	int (*puffs_node_access)(struct puffs_cc *,
	    void *, int, const struct puffs_cred *, pid_t);
	int (*puffs_node_getattr)(struct puffs_cc *,
	    void *, struct vattr *, const struct puffs_cred *, pid_t);
	int (*puffs_node_setattr)(struct puffs_cc *,
	    void *, const struct vattr *, const struct puffs_cred *, pid_t);
	int (*puffs_node_poll)(struct puffs_cc *,
	    void *, struct puffs_vnreq_poll *);
	int (*puffs_node_revoke)(struct puffs_cc *, void *, int);
	int (*puffs_node_mmap)(struct puffs_cc *,
	    void *, int, const struct puffs_cred *, pid_t);
	int (*puffs_node_fsync)(struct puffs_cc *,
	    void *, const struct puffs_cred *, int, off_t, off_t, pid_t);
	int (*puffs_node_seek)(struct puffs_cc *,
	    void *, off_t, off_t, const struct puffs_cred *);
	int (*puffs_node_remove)(struct puffs_cc *,
	    void *, void *, const struct puffs_cn *);
	int (*puffs_node_link)(struct puffs_cc *,
	    void *, void *, const struct puffs_cn *);
	int (*puffs_node_rename)(struct puffs_cc *,
	    void *, void *, const struct puffs_cn *, void *, void *,
	    const struct puffs_cn *);
	int (*puffs_node_mkdir)(struct puffs_cc *,
	    void *, void **, const struct puffs_cn *, const struct vattr *);
	int (*puffs_node_rmdir)(struct puffs_cc *,
	    void *, void *, const struct puffs_cn *);
	int (*puffs_node_symlink)(struct puffs_cc *,
	    void *, void **, const struct puffs_cn *, const struct vattr *,
	    const char *);
	int (*puffs_node_readdir)(struct puffs_cc *,
	    void *, struct dirent *, const struct puffs_cred *,
	    off_t *, size_t *);
	int (*puffs_node_readlink)(struct puffs_cc *,
	    void *, const struct puffs_cred *, char *, size_t *);
	int (*puffs_node_reclaim)(struct puffs_cc *,
	    void *, pid_t);
	int (*puffs_node_inactive)(struct puffs_cc *,
	    void *, pid_t, int *);
	int (*puffs_node_print)(struct puffs_cc *,
	    void *);
	int (*puffs_node_pathconf)(struct puffs_cc *,
	    void *, int, int *);
	int (*puffs_node_advlock)(struct puffs_cc *,
	    void *, void *, int, struct flock *, int);
	int (*puffs_node_getextattr)(struct puffs_cc *,
	    void *, struct puffs_vnreq_getextattr *);
	int (*puffs_node_setextattr)(struct puffs_cc *,
	    void *, struct puffs_vnreq_setextattr *);
	int (*puffs_node_listextattr)(struct puffs_cc *,
	    void *, struct puffs_vnreq_listextattr *);
	int (*puffs_node_read)(struct puffs_cc *, void *,
	    uint8_t *, off_t, size_t *, const struct puffs_cred *, int);
	int (*puffs_node_write)(struct puffs_cc *, void *,
	    uint8_t *, off_t, size_t *, const struct puffs_cred *, int);
};

struct puffs_usermount {
	struct puffs_ops	pu_ops;

	int			pu_fd;
	uint32_t		pu_flags;
	size_t			pu_maxreqlen;
	size_t			pu_cc_stacksize;

	int			pu_state;

	struct puffs_node	*pu_pn_root;

	LIST_HEAD(, puffs_node)	pu_pnodelst;

	int	pu_wcnt;
	void	*pu_privdata;
};

enum {
	PUFFS_STATE_MOUNTING,	PUFFS_STATE_RUNNING,
	PUFFS_STATE_UNMOUNTING,	PUFFS_STATE_UNMOUNTED
};

#define PUFFS_FLAG_KERN(a)	((a) & 0x0000ffff)
#define PUFFS_FLAG_LIB(a)	((a) & 0xffff0000)

#define PUFFS_FLAG_BUILDPATH	0x80000000	/* node paths in pnode */
#define PUFFS_FLAG_OPDUMP	0x40000000	/* dump all operations */

/* blocking mode argument */
#define PUFFSDEV_BLOCK 0
#define PUFFSDEV_NONBLOCK 1

/* mainloop flags */
#define PUFFSLOOP_NODAEMON 0x01

int  puffs_fsnop_unmount(struct puffs_cc *, int, pid_t);
int  puffs_fsnop_statvfs(struct puffs_cc *, struct statvfs *, pid_t);
void puffs_zerostatvfs(struct statvfs *);
int  puffs_fsnop_sync(struct puffs_cc *, int waitfor,
		      const struct puffs_cred *, pid_t);

#define		DENT_DOT	0
#define		DENT_DOTDOT	1
#define		DENT_ADJ(a)	((a)-2)	/* nth request means dir's n-2th */
int		puffs_gendotdent(struct dirent **, ino_t, int, size_t *);
int		puffs_nextdent(struct dirent **, const char *, ino_t,
			       uint8_t, size_t *);
int		puffs_vtype2dt(enum vtype);
enum vtype	puffs_mode2vt(mode_t);


/*
 * Operation credentials
 */

/* Credential fetch */
int	puffs_cred_getuid(const struct puffs_cred *pcr, uid_t *);
int	puffs_cred_getgid(const struct puffs_cred *pcr, gid_t *);
int	puffs_cred_getgroups(const struct puffs_cred *pcr, gid_t *, short *);

/* Credential check */
int	puffs_cred_isuid(const struct puffs_cred *pcr, uid_t);
int	puffs_cred_hasgroup(const struct puffs_cred *pcr, gid_t);
/* kernel internal NOCRED */
int	puffs_cred_iskernel(const struct puffs_cred *pcr);
/* kernel internal FSCRED */
int	puffs_cred_isfs(const struct puffs_cred *pcr);
/* root || NOCRED || FSCRED */
int	puffs_cred_isjuggernaut(const struct puffs_cred *pcr);


/*
 * protos
 */

#define PUFFSOP_PROTOS(fsname)						\
	int fsname##_fs_mount(struct puffs_usermount *, void **,	\
	    struct statvfs *);						\
	int fsname##_fs_unmount(struct puffs_cc *, int, pid_t);		\
	int fsname##_fs_statvfs(struct puffs_cc *,			\
	    struct statvfs *, pid_t);					\
	int fsname##_fs_sync(struct puffs_cc *, int,			\
	    const struct puffs_cred *cred, pid_t);			\
									\
	int fsname##_node_lookup(struct puffs_cc *,			\
	    void *, void **, enum vtype *, voff_t *, dev_t *,		\
	    const struct puffs_cn *);					\
	int fsname##_node_create(struct puffs_cc *,			\
	    void *, void **, const struct puffs_cn *,			\
	    const struct vattr *);					\
	int fsname##_node_mknod(struct puffs_cc *,			\
	    void *, void **, const struct puffs_cn *,			\
	    const struct vattr *);					\
	int fsname##_node_open(struct puffs_cc *,			\
	    void *, int, const struct puffs_cred *, pid_t);		\
	int fsname##_node_close(struct puffs_cc *,			\
	    void *, int, const struct puffs_cred *, pid_t);		\
	int fsname##_node_access(struct puffs_cc *,			\
	    void *, int, const struct puffs_cred *, pid_t);		\
	int fsname##_node_getattr(struct puffs_cc *,			\
	    void *, struct vattr *, const struct puffs_cred *, pid_t);	\
	int fsname##_node_setattr(struct puffs_cc *,			\
	    void *, const struct vattr *, const struct puffs_cred *,	\
	    pid_t);							\
	int fsname##_node_poll(struct puffs_cc *,			\
	    void *, struct puffs_vnreq_poll *);				\
	int fsname##_node_revoke(struct puffs_cc *, void *, int);	\
	int fsname##_node_mmap(struct puffs_cc *,			\
	    void *, int, const struct puffs_cred *, pid_t);		\
	int fsname##_node_fsync(struct puffs_cc *,			\
	    void *, const struct puffs_cred *, int, off_t, off_t,	\
	    pid_t);							\
	int fsname##_node_seek(struct puffs_cc *,			\
	    void *, off_t, off_t, const struct puffs_cred *);		\
	int fsname##_node_remove(struct puffs_cc *,			\
	    void *, void *, const struct puffs_cn *);			\
	int fsname##_node_link(struct puffs_cc *,			\
	    void *, void *, const struct puffs_cn *);			\
	int fsname##_node_rename(struct puffs_cc *,			\
	    void *, void *, const struct puffs_cn *, void *, void *,	\
	    const struct puffs_cn *);					\
	int fsname##_node_mkdir(struct puffs_cc *,			\
	    void *, void **, const struct puffs_cn *,			\
	    const struct vattr *);					\
	int fsname##_node_rmdir(struct puffs_cc *,			\
	    void *, void *, const struct puffs_cn *);			\
	int fsname##_node_symlink(struct puffs_cc *,			\
	    void *, void **, const struct puffs_cn *,			\
	    const struct vattr *, const char *);			\
	int fsname##_node_readdir(struct puffs_cc *,			\
	    void *, struct dirent *, const struct puffs_cred *,		\
	    off_t *, size_t *);						\
	int fsname##_node_readlink(struct puffs_cc *,			\
	    void *, const struct puffs_cred *, char *, size_t *);	\
	int fsname##_node_reclaim(struct puffs_cc *,			\
	    void *, pid_t);						\
	int fsname##_node_inactive(struct puffs_cc *,			\
	    void *, pid_t, int *);					\
	int fsname##_node_print(struct puffs_cc *,			\
	    void *);							\
	int fsname##_node_pathconf(struct puffs_cc *,			\
	    void *, int, int *);					\
	int fsname##_node_advlock(struct puffs_cc *,			\
	    void *, void *, int, struct flock *, int);			\
	int fsname##_node_getextattr(struct puffs_cc *,			\
	    void *, struct puffs_vnreq_getextattr *);			\
	int fsname##_node_setextattr(struct puffs_cc *,			\
	    void *, struct puffs_vnreq_setextattr *);			\
	int fsname##_node_listextattr(struct puffs_cc *,		\
	    void *, struct puffs_vnreq_listextattr *);			\
	int fsname##_node_read(struct puffs_cc *, void *,		\
	    uint8_t *, off_t, size_t *, const struct puffs_cred *, int);\
	int fsname##_node_write(struct puffs_cc *, void *,		\
	    uint8_t *, off_t, size_t *, const struct puffs_cred *, int);

#define PUFFSOP_INIT(ops)						\
    ops = malloc(sizeof(struct puffs_ops));				\
    memset(ops, 0, sizeof(struct puffs_ops))
#define PUFFSOP_SET(ops, fsname, fsornode, opname)			\
    (ops)->puffs_##fsornode##_##opname = fsname##_##fsornode##_##opname
#define PUFFSOP_SETFSNOP(ops, opname)					\
    (ops)->puffs_fs_##opname = puffs_fsnop_##opname

#define PUFFS_DEVEL_LIBVERSION 2
#define puffs_mount(a,b,c,d,e,f,g) \
    _puffs_mount(PUFFS_DEVEL_LIBVERSION,a,b,c,d,e,f,g)

__BEGIN_DECLS

struct puffs_usermount *_puffs_mount(int, struct puffs_ops *, const char *, int,
				    const char *, void *, uint32_t, size_t);
int		puffs_start(struct puffs_usermount *, void *, struct statvfs *);
int		puffs_exit(struct puffs_usermount *, int);
int		puffs_mainloop(struct puffs_usermount *, int);


int	puffs_getselectable(struct puffs_usermount *);
int	puffs_setblockingmode(struct puffs_usermount *, int);
int	puffs_getstate(struct puffs_usermount *);
int	puffs_setrootpath(struct puffs_usermount *, const char *);
void	puffs_setstacksize(struct puffs_usermount *, size_t);

struct puffs_node *	puffs_pn_new(struct puffs_usermount *, void *);
void			puffs_pn_put(struct puffs_node *);
struct vattr 		*puffs_pn_getvattrp(struct puffs_node *);
void			puffs_setvattr(struct vattr *, const struct vattr *);
void			puffs_vattr_null(struct vattr *);

/*
 * Requests
 */

struct puffs_getreq	*puffs_makegetreq(struct puffs_usermount *,
					  size_t, int);
int			puffs_loadgetreq(struct puffs_getreq *);
struct puffs_req	*puffs_getreq(struct puffs_getreq *);
int			puffs_remaininggetreq(struct puffs_getreq *);
void			puffs_setmaxgetreq(struct puffs_getreq *, int);
void			puffs_destroygetreq(struct puffs_getreq *);

struct puffs_putreq	*puffs_makeputreq(struct puffs_usermount *);
void			puffs_putreq(struct puffs_putreq *, struct puffs_req *);
void			puffs_putreq_cc(struct puffs_putreq *,struct puffs_cc*);
int			puffs_putputreq(struct puffs_putreq *);
void			puffs_resetputreq(struct puffs_putreq *);
void			puffs_destroyputreq(struct puffs_putreq *);

/*
 * Call Context interfaces relevant for user.
 */

void			puffs_cc_yield(struct puffs_cc *);
void			puffs_cc_continue(struct puffs_cc *);
struct puffs_usermount	*puffs_cc_getusermount(struct puffs_cc *);
struct puffs_cc 	*puffs_cc_create(struct puffs_usermount *);
void			puffs_cc_destroy(struct puffs_cc *);

/*
 * Execute or continue a request
 */

int	puffs_handlereqs(struct puffs_usermount *, struct puffs_getreq *,
			 struct puffs_putreq *, int);

int	puffs_dopreq(struct puffs_usermount *, struct puffs_putreq *,
		     struct puffs_req *);
int	puffs_docc(struct puffs_putreq *, struct puffs_cc *);

/*
 * Flushing / invalidation routines
 */

int	puffs_inval_namecache_dir(struct puffs_usermount *, void *);
int	puffs_inval_namecache_all(struct puffs_usermount *);

__END_DECLS

#endif /* _PUFFS_H_ */
