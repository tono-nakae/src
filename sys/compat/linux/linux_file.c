/*	$NetBSD: linux_file.c,v 1.8 1995/08/14 01:27:50 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * All rights reserved.
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
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
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/malloc.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_fcntl.h>
#include <compat/linux/linux_util.h>

/*
 * Some file-related calls are handled here. The usual flag conversion
 * an structure conversion is done, and alternate emul path searching.
 */

/*
 * The next two functions convert between the Linux and NetBSD values
 * of the flags used in open(2) and fcntl(2).
 */
static int
linux_to_bsd_ioflags(int lflags)
{
	int res = 0;

	res |= cvtto_bsd_mask(lflags, LINUX_O_WRONLY, O_WRONLY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_RDONLY, O_RDONLY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_RDWR, O_RDWR);
	res |= cvtto_bsd_mask(lflags, LINUX_O_CREAT, O_CREAT);
	res |= cvtto_bsd_mask(lflags, LINUX_O_EXCL, O_EXCL);
	res |= cvtto_bsd_mask(lflags, LINUX_O_NOCTTY, O_NOCTTY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_TRUNC, O_TRUNC);
	res |= cvtto_bsd_mask(lflags, LINUX_O_NDELAY, O_NDELAY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_SYNC, O_FSYNC);
	res |= cvtto_bsd_mask(lflags, LINUX_FASYNC, O_ASYNC);
	res |= cvtto_bsd_mask(lflags, LINUX_O_APPEND, O_APPEND);

	return res;
}

static int
bsd_to_linux_ioflags(int bflags)
{
	int res = 0;

	res |= cvtto_linux_mask(bflags, O_WRONLY, LINUX_O_WRONLY);
	res |= cvtto_linux_mask(bflags, O_RDONLY, LINUX_O_RDONLY);
	res |= cvtto_linux_mask(bflags, O_RDWR, LINUX_O_RDWR);
	res |= cvtto_linux_mask(bflags, O_CREAT, LINUX_O_CREAT);
	res |= cvtto_linux_mask(bflags, O_EXCL, LINUX_O_EXCL);
	res |= cvtto_linux_mask(bflags, O_NOCTTY, LINUX_O_NOCTTY);
	res |= cvtto_linux_mask(bflags, O_TRUNC, LINUX_O_TRUNC);
	res |= cvtto_linux_mask(bflags, O_NDELAY, LINUX_O_NDELAY);
	res |= cvtto_linux_mask(bflags, O_FSYNC, LINUX_O_SYNC);
	res |= cvtto_linux_mask(bflags, O_ASYNC, LINUX_FASYNC);
	res |= cvtto_linux_mask(bflags, O_APPEND, LINUX_O_APPEND);

	return res;
}

/*
 * creat(2) is an obsolete function, but it's present as a Linux
 * system call, so let's deal with it.
 *
 * Just call open(2) with the TRUNC, CREAT and WRONLY flags.
 */
int
linux_creat(p, uap, retval)
	struct proc *p;
	struct linux_creat_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap;
	register_t *retval;
{
	struct open_args oa;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));

	SCARG(&oa, path) = SCARG(uap, path);
	SCARG(&oa, flags) = O_CREAT | O_TRUNC | O_WRONLY;
	SCARG(&oa, mode) = SCARG(uap, mode);
	return open(p, &oa, retval);
}

/*
 * open(2). Take care of the different flag values, and let the
 * NetBSD syscall do the real work. See if this operation
 * gives the current process a controlling terminal.
 * (XXX is this necessary?)
 */
int
linux_open(p, uap, retval)
	struct proc *p;
	struct linux_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap;
	register_t *retval;
{
	int error, fl;
	struct open_args boa;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);

	fl = linux_to_bsd_ioflags(SCARG(uap, flags));

	if (fl & O_CREAT)
		LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	else
		LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&boa, path) = SCARG(uap, path);
	SCARG(&boa, flags) = fl;
	SCARG(&boa, mode) = SCARG(uap, mode);

	if ((error = open(p, &boa, retval)))
		return error;

	/*
	 * this bit from sunos_misc.c (and svr4_fcntl.c).
	 * If we are a session leader, and we don't have a controlling
	 * terminal yet, and the O_NOCTTY flag is not set, try to make
	 * this the controlling terminal.
	 */ 
        if (!(fl & O_NOCTTY) && SESS_LEADER(p) && !(p->p_flag & P_CONTROLT)) {
                struct filedesc *fdp = p->p_fd;
                struct file     *fp = fdp->fd_ofiles[*retval];

                /* ignore any error, just give it a try */
                if (fp->f_type == DTYPE_VNODE)
                        (fp->f_ops->fo_ioctl) (fp, TIOCSCTTY, (caddr_t) 0, p);
        }
	return 0;
}

/*
 * This appears to be part of a Linux attempt to switch to 64 bits file sizes.
 */
int
linux_llseek(p, uap, retval)
	struct proc *p;
	struct linux_llseek_args /* {
		syscallarg(int) fd;
		syscallarg(uint32_t) ohigh;
		syscallarg(uint32_t) olow;
		syscallarg(caddr_t) res;
		syscallarg(int) whence;
	} */ *uap;
	register_t *retval;
{
	struct lseek_args bla;
	int error;
	off_t off;

	off = SCARG(uap, olow) | (((off_t) SCARG(uap, ohigh)) << 32);

	SCARG(&bla, fd) = SCARG(uap, fd);
	SCARG(&bla, offset) = off;
	SCARG(&bla, whence) = SCARG(uap, whence);

	if ((error = lseek(p, &bla, retval)))
		return error;

	if ((error = copyout(retval, SCARG(uap, res), sizeof (off_t))))
		return error;

	retval[0] = 0;
	return 0;
}

/*
 * The next two functions take care of converting the flock
 * structure back and forth between Linux and NetBSD format.
 * The only difference in the structures is the order of
 * the fields, and the 'whence' value.
 */
static void
bsd_to_linux_flock(bfp, lfp)
	struct flock *bfp;
	struct linux_flock *lfp;
{
	lfp->l_start = bfp->l_start;
	lfp->l_len = bfp->l_len;
	lfp->l_pid = bfp->l_pid;
	lfp->l_whence = bfp->l_whence;
	switch (bfp->l_type) {
	case F_RDLCK:
		lfp->l_type = LINUX_F_RDLCK;
		break;
	case F_UNLCK:
		lfp->l_type = LINUX_F_UNLCK;
		break;
	case F_WRLCK:
		lfp->l_type = LINUX_F_WRLCK;
		break;
	}
}

static void
linux_to_bsd_flock(lfp, bfp)
	struct linux_flock *lfp;
	struct flock *bfp;
{
	bfp->l_start = lfp->l_start;
	bfp->l_len = lfp->l_len;
	bfp->l_pid = lfp->l_pid;
	bfp->l_whence = lfp->l_whence;
	switch (lfp->l_type) {
	case LINUX_F_RDLCK:
		bfp->l_type = F_RDLCK;
		break;
	case LINUX_F_UNLCK:
		bfp->l_type = F_UNLCK;
		break;
	case LINUX_F_WRLCK:
		bfp->l_type = F_WRLCK;
		break;
	}
}

/*
 * Most actions in the fcntl() call are straightforward; simply
 * pass control to the NetBSD system call. A few commands need
 * conversions after the actual system call has done its work,
 * because the flag values and lock structure are different.
 */
int
linux_fcntl(p, uap, retval)
	struct proc *p;
	struct linux_fcntl_args /* {
		syscallarg(int) fd;
		syscallarg(int) cmd;
		syscallarg(void *) arg;
	} */ *uap;
	register_t *retval;
{
	int fd, cmd, error, val;
	caddr_t arg, sg;
	struct linux_flock lfl;
	struct flock *bfp, bfl;
	struct fcntl_args fca;

	fd = SCARG(uap, fd);
	cmd = SCARG(uap, cmd);
	arg = (caddr_t) SCARG(uap, arg);

	switch (cmd) {
	case LINUX_F_DUPFD:
		cmd = F_DUPFD;
		break;
	case LINUX_F_GETFD:
		cmd = F_GETFD;
		break;
	case LINUX_F_SETFD:
		cmd = F_SETFD;
		break;
	case LINUX_F_GETFL:
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = F_GETFL;
		SCARG(&fca, arg) = arg;
		if ((error = fcntl(p, &fca, retval)))
			return error;
		retval[0] = bsd_to_linux_ioflags(retval[0]);
		return 0;
	case LINUX_F_SETFL:
		val = linux_to_bsd_ioflags((int)SCARG(uap, arg));
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = F_SETFL;
		SCARG(&fca, arg) = (caddr_t) val;
		return fcntl(p, &fca, retval);
	case LINUX_F_GETLK:
		sg = stackgap_init(p->p_emul);
		bfp = (struct flock *) stackgap_alloc(&sg, sizeof *bfp);
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = F_GETLK;
		SCARG(&fca, arg) = bfp;
		if ((error = fcntl(p, &fca, retval)))
			return error;
		if ((error = copyin(bfp, &bfl, sizeof bfl)))
			return error;
		bsd_to_linux_flock(&bfl, &lfl);
		return copyout(&lfl, arg, sizeof lfl);
		break;
	case LINUX_F_SETLK:
	case LINUX_F_SETLKW:
		cmd = (cmd == LINUX_F_SETLK ? F_SETLK : F_SETLKW);
		if ((error = copyin(arg, &lfl, sizeof lfl)))
			return error;
		linux_to_bsd_flock(&lfl, &bfl);
		sg = stackgap_init(p->p_emul);
		bfp = (struct flock *) stackgap_alloc(&sg, sizeof *bfp);
		if ((error = copyout(&bfl, bfp, sizeof bfl)))
			return error;
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = cmd;
		SCARG(&fca, arg) = bfp;
		return fcntl(p, &fca, retval);
		break;
	case LINUX_F_SETOWN:
		cmd = F_SETOWN;
		break;
	case LINUX_F_GETOWN:
		cmd = F_GETOWN;
		break;
	default:
		return EOPNOTSUPP;
	}

	SCARG(&fca, fd) = fd;
	SCARG(&fca, cmd) = cmd;
	SCARG(&fca, arg) = arg;
	return fcntl(p, &fca, retval);
}

/*
 * Convert a NetBSD stat structure to a Linux stat structure.
 * Only the order of the fields and the padding in the structure
 * is different.
 */
static void
bsd_to_linux_stat(bsp, lsp)
	struct stat *bsp;
	struct linux_stat *lsp;
{
	lsp->lst_dev     = bsp->st_dev;
	lsp->lst_ino     = bsp->st_ino;
	lsp->lst_mode    = bsp->st_mode;
	lsp->lst_nlink   = bsp->st_nlink;
	lsp->lst_uid     = bsp->st_uid;
	lsp->lst_gid     = bsp->st_gid;
	lsp->lst_rdev    = bsp->st_rdev;
	lsp->lst_size    = bsp->st_size;
	lsp->lst_blksize = bsp->st_blksize;
	lsp->lst_blocks  = bsp->st_blocks;
	lsp->lst_atime   = bsp->st_atime;
	lsp->lst_mtime   = bsp->st_mtime;
	lsp->lst_ctime   = bsp->st_ctime;
}

/*
 * The stat functions below are plain sailing. stat and lstat are handled
 * by one function to avoid code duplication.
 */
int
linux_fstat(p, uap, retval)
	struct proc *p;
	struct linux_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(linux_stat *) sp;
	} */ *uap;
	register_t *retval;
{
	struct fstat_args fsa;
	struct linux_stat tmplst;
	struct stat *st,tmpst;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);

	st = stackgap_alloc(&sg, sizeof (struct stat));

	SCARG(&fsa, fd) = SCARG(uap, fd);
	SCARG(&fsa, sb) = st;

	if ((error = fstat(p, &fsa, retval)))
		return error;

	if ((error = copyin(st, &tmpst, sizeof tmpst)))
		return error;

	bsd_to_linux_stat(&tmpst, &tmplst);

	if ((error = copyout(&tmplst, SCARG(uap, sp), sizeof tmplst)))
		return error;

	return 0;
}

static int
linux_stat1(p, uap, retval, dolstat)
	struct proc *p;
	struct linux_stat_args *uap;
	register_t *retval;
	int dolstat;
{
	struct stat_args sa;
	struct linux_stat tmplst;
	struct stat *st, tmpst;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	st = stackgap_alloc(&sg, sizeof (struct stat));
	SCARG(&sa, ub) = st;
	SCARG(&sa, path) = SCARG(uap, path);

	if ((error = (dolstat ? lstat(p, &sa, retval) : stat(p, &sa, retval))))
		return error;

	if ((error = copyin(st, &tmpst, sizeof tmpst)))
		return error;

	bsd_to_linux_stat(&tmpst, &tmplst);

	if ((error = copyout(&tmplst, SCARG(uap, sp), sizeof tmplst)))
		return error;

	return 0;
}

int
linux_stat(p, uap, retval)
	struct proc *p;
	struct linux_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct linux_stat *) sp;
	} */ *uap;
	register_t *retval;
{
	return linux_stat1(p, uap, retval, 0);
}

int
linux_lstat(p, uap, retval)
	struct proc *p;
	struct linux_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct linux_stat *) sp;
	} */ *uap;
	register_t *retval;
{
	return linux_stat1(p, uap, retval, 1);
}

/*
 * The following syscalls are only here because of the alternate path check.
 */
int
linux_access(p, uap, retval)
	struct proc *p;
	struct linux_access_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return access(p, uap, retval);
}

int
linux_unlink(p, uap, retval)
	struct proc *p;
	struct linux_unlink_args /* {
		syscallarg(char *) path;
	} */ *uap;
	register_t *retval;

{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return unlink(p, uap, retval);
}

int linux_chdir(p, uap, retval)
	struct proc *p;
	struct linux_chdir_args /* {
		syscallarg(char *) path;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return chdir(p, uap, retval);
}

int
linux_mknod(p, uap, retval)
	struct proc *p;
	struct linux_mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));

	return mknod(p, uap, retval);
}

int
linux_chmod(p, uap, retval)
	struct proc *p;
	struct linux_chmod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return chmod(p, uap, retval);
}

int
linux_chown(p, uap, retval)
	struct proc *p;
	struct linux_chown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return chmod(p, uap, retval);
}

int
linux_rename(p, uap, retval)
	struct proc *p;
	struct linux_rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, from));
	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, to));

	return rename(p, uap, retval);
}

int
linux_mkdir(p, uap, retval)
	struct proc *p;
	struct linux_mkdir_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	return mkdir(p, uap, retval);
}

int
linux_rmdir(p, uap, retval)
	struct proc *p;
	struct linux_rmdir_args /* {
		syscallarg(char *) path;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return rmdir(p, uap, retval);
}

int
linux_symlink(p, uap, retval)
	struct proc *p;
	struct linux_symlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) to;
	} */ *uap;
	register_t *retval;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, to));

	return symlink(p, uap, retval);
}

int
linux_readlink(p, uap, retval)
	struct proc *p;
	struct linux_readlink_args /* {
		syscallarg(char *) name;
		syscallarg(char *) buf;
		syscallarg(int) count;
	} */ *uap;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, name));
	return readlink(p, uap, retval);
}

int
linux_truncate(p, uap, retval)
	struct proc *p;
	struct linux_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(long) length;
	} */ *uap;
{
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return compat_43_truncate(p, uap, retval);
}
