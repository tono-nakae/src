/*	$NetBSD: sunos_misc.c,v 1.36 1994/11/23 07:05:20 deraadt Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)sunos_misc.c	8.1 (Berkeley) 6/18/93
 *
 *	Header: sunos_misc.c,v 1.16 93/04/07 02:46:27 torek Exp 
 */

/*
 * SunOS compatibility module.
 *
 * SunOS system calls that are implemented differently in BSD are
 * handled here.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/dir.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>
#include <sys/syscallargs.h>
#include <compat/sunos/sunos.h>
#include <compat/sunos/sunos_syscallargs.h>

#include <netinet/in.h>

#include <miscfs/specfs/specdev.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>

#include <vm/vm.h>

int
sunos_wait4(p, uap, retval)
	struct proc *p;
	struct sunos_wait4_args *uap;
	register_t *retval;
{
	if (SCARG(uap, pid) == 0)
		SCARG(uap, pid) = WAIT_ANY;
	return (wait4(p, uap, retval, 1));
}

int
sunos_creat(p, uap, retval)
	struct proc *p;
	struct sunos_creat_args *uap;
	register_t *retval;
{
	struct sunos_open_args ouap;

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, flags) = O_WRONLY | O_CREAT | O_TRUNC;
	SCARG(&ouap, mode) = SCARG(uap, mode);
	return (open(p, &ouap, retval));
}

int
sunos_execv(p, uap, retval)
	struct proc *p;
	struct sunos_execv_args *uap;
	register_t *retval;
{
	struct execve_args ouap;

	SCARG(&ouap, path) = SCARG(uap, path);
	SCARG(&ouap, argp) = SCARG(uap, argp);
	SCARG(&ouap, envp) = NULL;
	return (execve(p, &ouap, retval));
}

int
sunos_omsync(p, uap, retval)
	struct proc *p;
	struct sunos_omsync_args *uap;
	register_t *retval;
{
	struct msync_args ouap;

	if (SCARG(uap, flags))
		return (EINVAL);
	SCARG(&ouap, addr) = SCARG(uap, addr);
	SCARG(&ouap, len) = SCARG(uap, len);
	return (msync(p, &ouap, retval));
}

int
sunos_unmount(p, uap, retval)
	struct proc *p;
	struct sunos_unmount_args *uap;
	register_t *retval;
{
	SCARG(uap, flags) = 0;
	return (unmount(p, (struct unmount_args *)uap, retval));
}

int
sunos_mount(p, uap, retval)
	struct proc *p;
	struct sunos_mount_args *uap;
	register_t *retval;
{
	int oflags = SCARG(uap, flags), nflags, error;
	extern char sigcode[], esigcode[];
	char fsname[MFSNAMELEN];

#define	szsigcode	(esigcode - sigcode)

	if (oflags & (SUNM_NOSUB | SUNM_SYS5))
		return (EINVAL);
	if ((oflags & SUNM_NEWTYPE) == 0)
		return (EINVAL);
	nflags = 0;
	if (oflags & SUNM_RDONLY)
		nflags |= MNT_RDONLY;
	if (oflags & SUNM_NOSUID)
		nflags |= MNT_NOSUID;
	if (oflags & SUNM_REMOUNT)
		nflags |= MNT_UPDATE;
	SCARG(uap, flags) = nflags;

	if (error = copyinstr((caddr_t)SCARG(uap, type), fsname,
	    sizeof fsname, (u_int *)0))
		return (error);

	if (strcmp(fsname, "4.2") == 0) {
		SCARG(uap, type) = STACKGAPBASE;
		if (error = copyout("ufs", SCARG(uap, type), sizeof("ufs")))
			return (error);
	} else if (strcmp(fsname, "nfs") == 0) {
		struct sunos_nfs_args sna;
		struct sockaddr_in sain;
		struct nfs_args na;
		struct sockaddr sa;

		if (error = copyin(SCARG(uap, data), &sna, sizeof sna))
			return (error);
		if (error = copyin(sna.addr, &sain, sizeof sain))
			return (error);
		bcopy(&sain, &sa, sizeof sa);
		sa.sa_len = sizeof(sain);
		SCARG(uap, data) = STACKGAPBASE;
		na.addr = (struct sockaddr *)((int)SCARG(uap, data) + sizeof na);
		na.addrlen = sizeof(struct sockaddr);
		na.sotype = SOCK_DGRAM;
		na.proto = IPPROTO_UDP;
		na.fh = (nfsv2fh_t *)sna.fh;
		na.flags = sna.flags;
		na.wsize = sna.wsize;
		na.rsize = sna.rsize;
		na.timeo = sna.timeo;
		na.retrans = sna.retrans;
		na.hostname = sna.hostname;

		if (error = copyout(&sa, na.addr, sizeof sa))
			return (error);
		if (error = copyout(&na, SCARG(uap, data), sizeof na))
			return (error);
	}
	return (mount(p, (struct mount_args *)uap, retval));
}

#if defined(NFSCLIENT)
int
async_daemon(p, uap, retval)
	struct proc *p;
	void *uap;
	register_t *retval;
{
	struct nfssvc_args ouap;

	SCARG(&ouap, flag) = NFSSVC_BIOD;
	SCARG(&ouap, argp) = NULL;
	return nfssvc(p, &ouap, retval);
}
#endif /* NFSCLIENT */

int
sunos_sigpending(p, uap, retval)
	struct proc *p;
	struct sunos_sigpending_args *uap;
	register_t *retval;
{
	int mask = p->p_siglist & p->p_sigmask;

	return (copyout((caddr_t)&mask, (caddr_t)SCARG(uap, mask), sizeof(int)));
}

/*
 * Read Sun-style directory entries.  We suck them into kernel space so
 * that they can be massaged before being copied out to user code.  Like
 * SunOS, we squish out `empty' entries.
 *
 * This is quite ugly, but what do you expect from compatibility code?
 */
int
sunos_getdents(p, uap, retval)
	struct proc *p;
	register struct sunos_getdents_args *uap;
	register_t *retval;
{
	register struct vnode *vp;
	register caddr_t inp, buf;	/* BSD-format */
	register int len, reclen;	/* BSD-format */
	register caddr_t outp;		/* Sun-format */
	register int resid;		/* Sun-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	off_t off;			/* true file offset */
	long soff;			/* Sun file offset */
	int buflen, error, eofflag;
#define	BSD_DIRENT(cp) ((struct dirent *)(cp))
#define	SUNOS_RECLEN(reclen) (reclen + sizeof(long))

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR)	/* XXX  vnode readdir op should do this */
		return (EINVAL);
	buflen = min(MAXBSIZE, SCARG(uap, nbytes));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	VOP_LOCK(vp);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_resid = buflen;
	auio.uio_offset = off;
	/*
	 * First we read into the malloc'ed buffer, then
	 * we massage it into user space, one record at a time.
	 */
	if (error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag,
	    (u_long *)0, 0))
		goto out;
	inp = buf;
	outp = SCARG(uap, buf);
	resid = SCARG(uap, nbytes);
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;
	for (; len > 0; len -= reclen) {
		reclen = ((struct dirent *)inp)->d_reclen;
		if (reclen & 3)
			panic("sunos_getdents");
		off += reclen;		/* each entry points to next */
		if (BSD_DIRENT(inp)->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			continue;
		}
		if (reclen > len || resid < SUNOS_RECLEN(reclen)) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make a Sun-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		BSD_DIRENT(inp)->d_reclen = SUNOS_RECLEN(reclen);
#if notdef
		BSD_DIRENT(inp)->d_type = 0; 	/* 4.4 specific */
#endif
		soff = off;
		if ((error = copyout((caddr_t)&soff, outp, sizeof soff)) != 0 ||
		    (error = copyout(inp, outp + sizeof soff, reclen)) != 0)
			goto out;
		/* advance past this real entry */
		inp += reclen;
		/* advance output past Sun-shaped entry */
		outp += SUNOS_RECLEN(reclen);
		resid -= SUNOS_RECLEN(reclen);
	}
	/* if we squished out the whole block, try again */
	if (outp == SCARG(uap, buf))
		goto again;
	fp->f_offset = off;		/* update the vnode offset */
eof:
	*retval = SCARG(uap, nbytes) - resid;
out:
	VOP_UNLOCK(vp);
	free(buf, M_TEMP);
	return (error);
}

#define	SUNOS__MAP_NEW	0x80000000	/* if not, old mmap & cannot handle */

int
sunos_mmap(p, uap, retval)
	register struct proc *p;
	register struct sunos_mmap_args *uap;
	register_t *retval;
{
	struct mmap_args ouap;
	register struct filedesc *fdp;
	register struct file *fp;
	register struct vnode *vp;

	/*
	 * Verify the arguments.
	 */
	if (SCARG(uap, prot) & ~(PROT_READ|PROT_WRITE|PROT_EXEC))
		return (EINVAL);			/* XXX still needed? */

	if ((SCARG(uap, flags) & SUNOS__MAP_NEW) == 0)
		return (EINVAL);

	SCARG(&ouap, flags) = SCARG(uap, flags) & ~SUNOS__MAP_NEW;
	SCARG(&ouap, addr) = SCARG(uap, addr);

	if ((SCARG(&ouap, flags) & MAP_FIXED) == 0 &&
	    SCARG(&ouap, addr) != 0 &&
	    SCARG(&ouap, addr) < (caddr_t)round_page(p->p_vmspace->vm_daddr+MAXDSIZ))
		SCARG(&ouap, addr) = (caddr_t)round_page(p->p_vmspace->vm_daddr+MAXDSIZ);

	SCARG(&ouap, len) = SCARG(uap, len);
	SCARG(&ouap, prot) = SCARG(uap, prot);
	SCARG(&ouap, fd) = SCARG(uap, fd);
	SCARG(&ouap, pos) = SCARG(uap, pos);

	/*
	 * Special case: if fd refers to /dev/zero, map as MAP_ANON.  (XXX)
	 */
	fdp = p->p_fd;
	if ((unsigned)SCARG(&ouap, fd) < fdp->fd_nfiles &&		/*XXX*/
	    (fp = fdp->fd_ofiles[SCARG(&ouap, fd)]) != NULL &&		/*XXX*/
	    fp->f_type == DTYPE_VNODE &&				/*XXX*/
	    (vp = (struct vnode *)fp->f_data)->v_type == VCHR &&	/*XXX*/
	    iszerodev(vp->v_rdev)) {					/*XXX*/
		SCARG(&ouap, flags) |= MAP_ANON;
		SCARG(&ouap, fd) = -1;
	}

	return (mmap(p, &ouap, retval));
}

#define	MC_SYNC		1
#define	MC_LOCK		2
#define	MC_UNLOCK	3
#define	MC_ADVISE	4
#define	MC_LOCKAS	5
#define	MC_UNLOCKAS	6

int
sunos_mctl(p, uap, retval)
	register struct proc *p;
	register struct sunos_mctl_args *uap;
	register_t *retval;
{

	switch (SCARG(uap, func)) {
	case MC_ADVISE:		/* ignore for now */
		return (0);
	case MC_SYNC:		/* translate to msync */
		return (msync(p, uap, retval));
	default:
		return (EINVAL);
	}
}

int
sunos_setsockopt(p, uap, retval)
	struct proc *p;
	register struct sunos_setsockopt_args *uap;
	register_t *retval;
{
	struct file *fp;
	struct mbuf *m = NULL;
	int error;

	if (error = getsock(p->p_fd, SCARG(uap, s), &fp))
		return (error);
#define	SO_DONTLINGER (~SO_LINGER)
	if (SCARG(uap, name) == SO_DONTLINGER) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return (ENOBUFS);
		mtod(m, struct linger *)->l_onoff = 0;
		m->m_len = sizeof(struct linger);
		return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
		    SO_LINGER, m));
	}
	if (SCARG(uap, valsize) > MLEN)
		return (EINVAL);
	if (SCARG(uap, val)) {
		m = m_get(M_WAIT, MT_SOOPTS);
		if (m == NULL)
			return (ENOBUFS);
		if (error = copyin(SCARG(uap, val), mtod(m, caddr_t),
		    (u_int)SCARG(uap, valsize))) {
			(void) m_free(m);
			return (error);
		}
		m->m_len = SCARG(uap, valsize);
	}
	return (sosetopt((struct socket *)fp->f_data, SCARG(uap, level),
	    SCARG(uap, name), m));
}

int
sunos_fchroot(p, uap, retval)
	register struct proc *p;
	register struct sunos_fchroot_args *uap;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register struct vnode *vp;
	struct file *fp;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	if ((error = getvnode(fdp, SCARG(uap, fd), &fp)) != 0)
		return (error);
	vp = (struct vnode *)fp->f_data;
	VOP_LOCK(vp);
	if (vp->v_type != VDIR)
		error = ENOTDIR;
	else
		error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p);
	VOP_UNLOCK(vp);
	if (error)
		return (error);
	VREF(vp);
	if (fdp->fd_rdir != NULL)
		vrele(fdp->fd_rdir);
	fdp->fd_rdir = vp;
	return (0);
}

/*
 * XXX: This needs cleaning up.
 */
int
sunos_auditsys(...)
{
	return 0;
}

int
sunos_uname(p, uap, retval)
	struct proc *p;
	struct sunos_uname_args *uap;
	register_t *retval;
{
	struct sunos_utsname sut;
	extern char ostype[], machine[], osrelease[];

	bzero(&sut, sizeof(sut));

	bcopy(ostype, sut.sysname, sizeof(sut.sysname) - 1);
	bcopy(hostname, sut.nodename, sizeof(sut.nodename));
	sut.nodename[sizeof(sut.nodename)-1] = '\0';
	bcopy(osrelease, sut.release, sizeof(sut.release) - 1);
	bcopy("1", sut.version, sizeof(sut.version) - 1);
	bcopy(machine, sut.machine, sizeof(sut.machine) - 1);

	return copyout((caddr_t)&sut, (caddr_t)SCARG(uap, name),
	    sizeof(struct sunos_utsname));
}

int
sunos_setpgid(p, uap, retval)
	struct proc *p;
	struct sunos_setpgid_args *uap;
	register_t *retval;
{
	/*
	 * difference to our setpgid call is to include backwards
	 * compatibility to pre-setsid() binaries. Do setsid()
	 * instead of setpgid() in those cases where the process
	 * tries to create a new session the old way.
	 */
	if (!SCARG(uap, pgid) && (!SCARG(uap, pid) || SCARG(uap, pid) == p->p_pid))
		return setsid(p, uap, retval);
	else
		return setpgid(p, uap, retval);
}

int
sunos_open(p, uap, retval)
	struct proc *p;
	struct sunos_open_args *uap;
	register_t *retval;
{
	int l, r;
	int noctty = SCARG(uap, mode) & 0x8000;
	int ret;
	
	/* convert mode into NetBSD mode */
	l = SCARG(uap, mode);
	r =	(l & (0x0001 | 0x0002 | 0x0008 | 0x0040 | 0x0200 | 0x0400 | 0x0800));
	r |=	((l & (0x0004 | 0x1000 | 0x4000)) ? O_NONBLOCK : 0);
	r |=	((l & 0x0080) ? O_SHLOCK : 0);
	r |=	((l & 0x0100) ? O_EXLOCK : 0);
	r |=	((l & 0x2000) ? O_FSYNC : 0);

	SCARG(uap, mode) = r;
	ret = open(p, (struct open_args *)uap, retval);

	if (!ret && !noctty && SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
		struct filedesc *fdp = p->p_fd;
		struct file *fp = fdp->fd_ofiles[*retval];

		/* ignore any error, just give it a try */
		if (fp->f_type == DTYPE_VNODE)
			(fp->f_ops->fo_ioctl)(fp, TIOCSCTTY, (caddr_t) 0, p);
	}
	return ret;
}

#if defined (NFSSERVER)
int
sunos_nfssvc(p, uap, retval)
	struct proc *p;
	struct sunos_nfssvc_args *uap;
	register_t *retval;
{
	struct nfssvc_args outuap;
	struct nfsd_srvargs nfsdarg;
	struct sockaddr sa;
	int error;
	extern char sigcode[], esigcode[];

#if 0
	bzero(&outuap, sizeof outuap);
	SCARG(&outuap, fd) = SCARG(uap, fd);
	SCARG(&outuap, mskval) = STACKGAPBASE;
	SCARG(&outuap, msklen) = sizeof sa;
	SCARG(&outuap, mtchval) = SCARG(&outuap, mskval) + sizeof sa;
	SCARG(&outuap, mtchlen) = sizeof sa;

	bzero(&sa, sizeof sa);
	if (error = copyout(&sa, SCARG(&outuap, mskval), SCARG(&outuap, msklen)))
		return (error);
	if (error = copyout(&sa, SCARG(&outuap, mtchval), SCARG(&outuap, mtchlen)))
		return (error);

	return nfssvc(p, &outuap, retval);
#else
	return (ENOSYS);
#endif
}
#endif /* NFSSERVER */

int
sunos_ustat(p, uap, retval)
	struct proc *p;
	struct sunos_ustat_args *uap;
	register_t *retval;
{
	struct sunos_ustat us;
	int error;

	bzero(&us, sizeof us);

	/*
	 * XXX: should set f_tfree and f_tinode at least
	 * How do we translate dev -> fstat? (and then to sunos_ustat)
	 */

	if (error = copyout(&us, SCARG(uap, buf), sizeof us))
		return (error);
	return 0;
}

int
sunos_quotactl(p, uap, retval)
	struct proc *p;
	struct sunos_quotactl_args *uap;
	register_t *retval;
{
	return EINVAL;
}

int
sunos_vhangup(p, uap, retval)
	struct proc *p;
	void *uap;
	register_t *retval;
{
	return 0;
}

static
sunstatfs(sp, buf)
	struct statfs *sp;
	caddr_t buf;
{
	struct sunos_statfs ssfs;

	bzero(&ssfs, sizeof ssfs);
	ssfs.f_type = 0;
	ssfs.f_bsize = sp->f_bsize;
	ssfs.f_blocks = sp->f_blocks;
	ssfs.f_bfree = sp->f_bfree;
	ssfs.f_bavail = sp->f_bavail;
	ssfs.f_files = sp->f_files;
	ssfs.f_ffree = sp->f_ffree;
	ssfs.f_fsid = sp->f_fsid;
	return copyout((caddr_t)&ssfs, buf, sizeof ssfs);
}	

int
sunos_statfs(p, uap, retval)
	struct proc *p;
	struct sunos_statfs_args *uap;
	register_t *retval;
{
	register struct mount *mp;
	register struct statfs *sp;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	mp = nd.ni_vp->v_mount;
	sp = &mp->mnt_stat;
	vrele(nd.ni_vp);
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return sunstatfs(sp, (caddr_t)SCARG(uap, buf));
}

int
sunos_fstatfs(p, uap, retval)
	struct proc *p;
	struct sunos_fstatfs_args *uap;
	register_t *retval;
{
	struct file *fp;
	struct mount *mp;
	register struct statfs *sp;
	int error;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp))
		return (error);
	mp = ((struct vnode *)fp->f_data)->v_mount;
	sp = &mp->mnt_stat;
	if (error = VFS_STATFS(mp, sp, p))
		return (error);
	sp->f_flags = mp->mnt_flag & MNT_VISFLAGMASK;
	return sunstatfs(sp, (caddr_t)SCARG(uap, buf));
}

int
sunos_exportfs(p, uap, retval)
	struct proc *p;
	struct sunos_exportfs_args *uap;
	register_t *retval;
{
	/*
	 * XXX: should perhaps translate into a mount(2)
	 * with MOUNT_EXPORT?
	 */
	return 0;
}

int
sunos_mknod(p, uap, retval)
	struct proc *p;
	struct sunos_mknod_args *uap;
	register_t *retval;
{
	if (S_ISFIFO(SCARG(uap, mode)))
		return mkfifo(p, uap, retval);

	return mknod(p, (struct mknod_args *)uap, retval);
}

#define SUNOS_SC_ARG_MAX	1
#define SUNOS_SC_CHILD_MAX	2
#define SUNOS_SC_CLK_TCK	3
#define SUNOS_SC_NGROUPS_MAX	4
#define SUNOS_SC_OPEN_MAX	5
#define SUNOS_SC_JOB_CONTROL	6
#define SUNOS_SC_SAVED_IDS	7
#define SUNOS_SC_VERSION	8

int
sunos_sysconf(p, uap, retval)
	struct proc *p;
	struct sunos_sysconf_args *uap;
	register_t *retval;
{
	extern int maxfiles;

	switch(SCARG(uap, name)) {
	case SUNOS_SC_ARG_MAX:
		*retval = ARG_MAX;
		break;
	case SUNOS_SC_CHILD_MAX:
		*retval = maxproc;
		break;
	case SUNOS_SC_CLK_TCK:
		*retval = 60;		/* should this be `hz', ie. 100? */
		break;
	case SUNOS_SC_NGROUPS_MAX:
		*retval = NGROUPS_MAX;
		break;
	case SUNOS_SC_OPEN_MAX:
		*retval = maxfiles;
		break;
	case SUNOS_SC_JOB_CONTROL:
		*retval = 1;
		break;
	case SUNOS_SC_SAVED_IDS:
#ifdef _POSIX_SAVED_IDS
		*retval = 1;
#else
		*retval = 0;
#endif
		break;
	case SUNOS_SC_VERSION:
		*retval = 198808;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

#define SUNOS_RLIMIT_NOFILE	6	/* Other RLIMIT_* are the same */
#define SUNOS_RLIM_NLIMITS	7

int
sunos_getrlimit(p, uap, retval)
	struct proc *p;
	struct sunos_getrlimit_args *uap;
	register_t *retval;
{
	if (SCARG(uap, which) >= SUNOS_RLIM_NLIMITS)
		return EINVAL;

	if (SCARG(uap, which) == SUNOS_RLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;

	return compat_43_getrlimit(p, uap, retval);
}

int
sunos_setrlimit(p, uap, retval)
	struct proc *p;
	struct sunos_getrlimit_args *uap;
	register_t *retval;
{
	if (SCARG(uap, which) >= SUNOS_RLIM_NLIMITS)
		return EINVAL;

	if (SCARG(uap, which) == SUNOS_RLIMIT_NOFILE)
		SCARG(uap, which) = RLIMIT_NOFILE;

	return compat_43_setrlimit(p, uap, retval);
}

/* for the m68k machines */
#ifndef PT_GETFPREGS
#define PT_GETFPREGS -1
#endif
#ifndef PT_SETFPREGS
#define PT_SETFPREGS -1
#endif

static int sreq2breq[] = {
	PT_TRACE_ME,    PT_READ_I,      PT_READ_D,      -1,
	PT_WRITE_I,     PT_WRITE_D,     -1,             PT_CONTINUE,
	PT_KILL,        -1,             PT_ATTACH,      PT_DETACH,
	PT_GETREGS,     PT_SETREGS,     PT_GETFPREGS,   PT_SETFPREGS
};
static int nreqs = sizeof(sreq2breq) / sizeof(sreq2breq[0]);

sunos_ptrace(p, uap, retval)
	struct proc *p;
	struct sunos_ptrace_args *uap;
	int *retval;
{
	struct ptrace_args pa;
	int req;

	req = SCARG(uap, req);

	if (req < 0 || req >= nreqs)
		return (EINVAL);

	req = sreq2breq[req];
	if (req == -1)
		return (EINVAL);

	SCARG(&pa, req) = req;
	SCARG(&pa, pid) = (pid_t)SCARG(uap, pid);
	SCARG(&pa, addr) = (caddr_t)SCARG(uap, addr);
	SCARG(&pa, data) = SCARG(uap, data);

	return ptrace(p, &pa, retval);
}

static void
sunos_pollscan(p, pl, nfd, retval)
	struct proc *p;
	struct sunos_pollfd *pl;
	int nfd;
	register_t *retval;
{
	register struct filedesc *fdp = p->p_fd;
	register int msk, i;
	struct file *fp;
	int n = 0;
	static int flag[3] = { FREAD, FWRITE, 0 };
	static int pflag[3] = { SUNOS_POLLIN|SUNOS_POLLRDNORM, 
				SUNOS_POLLOUT, SUNOS_POLLERR };

	/* 
	 * XXX: We need to implement the rest of the flags.
	 */
	for (i = 0; i < nfd; i++) {
		fp = fdp->fd_ofiles[pl[i].fd];
		if (fp == NULL) {
			if (pl[i].events & SUNOS_POLLNVAL) {
				pl[i].revents |= SUNOS_POLLNVAL;
				n++;
			}
			continue;
		}
		for (msk = 0; msk < 3; msk++) {
			if (pl[i].events & pflag[msk]) {
				if ((*fp->f_ops->fo_select)(fp, flag[msk], p)) {
					pl[i].revents |= 
					    pflag[msk] & pl[i].events;
					n++;
				}
			}
		}
	}
	*retval = n;
}


/*
 * We are using the same mechanism as select only we encode/decode args
 * differently.
 */
int
sunos_poll(p, uap, retval)
	struct proc *p;
	struct sunos_poll_args	*uap;
	register_t *retval;
{
	int i, s;
	int error, error2;
	size_t sz = sizeof(struct sunos_pollfd) * SCARG(uap, nfds);
	struct sunos_pollfd *pl;
	int msec = SCARG(uap, timeout);
	struct timeval atv;
	int timo;
	u_int ni;
	int ncoll;
	extern int nselcoll, selwait;

	pl = (struct sunos_pollfd *) malloc(sz, M_TEMP, M_WAITOK);

	if (error = copyin(SCARG(uap, fds), pl, sz))
		goto bad;

	for (i = 0; i < SCARG(uap, nfds); i++)
		pl[i].revents = 0;

	if (msec != -1) {
		atv.tv_sec = msec / 1000;
		atv.tv_usec = (msec - (atv.tv_sec * 1000)) * 1000;

		if (itimerfix(&atv)) {
			error = EINVAL;
			goto done;
		}
		s = splclock();
		timevaladd(&atv, (struct timeval *)&time);
		timo = hzto(&atv);
		/*
		 * Avoid inadvertently sleeping forever.
		 */
		if (timo == 0)
			timo = 1;
		splx(s);
	} else
		timo = 0;

retry:
	ncoll = nselcoll;
	p->p_flag |= P_SELECT;
	sunos_pollscan(p, pl, SCARG(uap, nfds), retval);
	if (*retval)
		goto done;
	s = splhigh();
	if (timo && timercmp(&time, &atv, >=)) {
		splx(s);
		goto done;
	}
	if ((p->p_flag & P_SELECT) == 0 || nselcoll != ncoll) {
		splx(s);
		goto retry;
	}
	p->p_flag &= ~P_SELECT;
	error = tsleep((caddr_t)&selwait, PSOCK | PCATCH, "sunos_poll", timo);
	splx(s);
	if (error == 0)
		goto retry;

done:
	p->p_flag &= ~P_SELECT;
	/* poll is not restarted after signals... */
	if (error == ERESTART)
		error = EINTR;
	if (error == EWOULDBLOCK)
		error = 0;

	if (error2 = copyout(pl, SCARG(uap, fds), sz))
		error = error2;

bad:
	free((char *) pl, M_TEMP);

	return (error);
}
