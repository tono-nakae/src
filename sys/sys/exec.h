/*-
 * Copyright (c) 1994 Christopher G. Demetriou
 * Copyright (c) 1993 Theo de Raadt
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	from: @(#)exec.h	7.5 (Berkeley) 2/15/91
 *	$Id: exec.h,v 1.36 1994/05/17 04:24:58 cgd Exp $
 */

#ifndef	_SYS_EXEC_H_
#define	_SYS_EXEC_H_

#include <sys/param.h>
#include <sys/cdefs.h>

/*
 * Header prepended to each a.out file.
 * only manipulate the a_midmag field via the
 * N_SETMAGIC/N_GET{MAGIC,MID,FLAG} macros in a.out.h
 */
struct exec {
	u_long	a_midmag;	/* htonl(flags<<26 | mid<<16 | magic) */
	u_long	a_text;		/* text segment size */
	u_long	a_data;		/* initialized data size */
	u_long	a_bss;		/* uninitialized data size */
	u_long	a_syms;		/* symbol table size */
	u_long	a_entry;	/* entry point */
	u_long	a_trsize;	/* text relocation size */
	u_long	a_drsize;	/* data relocation size */
};

/* a_magic */
#define	OMAGIC		0407	/* old impure format */
#define	NMAGIC		0410	/* read-only text */
#define	ZMAGIC		0413	/* demand load format */
#define	QMAGIC		0314	/* "compact" demand load format; deprecated */

/*
 * a_mid - keep sorted in numerical order for sanity's sake
 * ensure that: 0 < mid < 0x3ff
 */
#define	MID_ZERO	0	/* unknown - implementation dependent */
#define	MID_SUN010	1	/* sun 68010/68020 binary */
#define	MID_SUN020	2	/* sun 68020-only binary */
#define	MID_PC386	100	/* 386 PC binary. (so quoth BFD) */
#define	MID_HP200	200	/* hp200 (68010) BSD binary */
#define	MID_I386	134	/* i386 BSD binary */
#define	MID_M68K	135	/* m68k BSD binary with 8K page sizes */
#define	MID_M68K4K	136	/* m68k BSD binary with 4K page sizes */
#define	MID_NS32532	137	/* ns32532 */
#define	MID_SPARC	138	/* sparc */
#define	MID_PMAX	139	/* pmax */
#define	MID_VAX		140	/* vax */
#define	MID_HP300	300	/* hp300 (68020+68881) BSD binary */
#define	MID_HPUX	0x20C	/* hp200/300 HP-UX binary */
#define	MID_HPUX800     0x20B   /* hp800 HP-UX binary */

/*
 * a_flags
 */
#define EX_DYNAMIC	0x20	/* a.out contains run-time link-edit info */


/*
 * it's nearly impossible to find out where a process's args are without
 * a hint.  4.4 actually puts pointers in the process address space,
 * above the stack.  this is reasonable, and also allows the process to
 * set its ps-visible arguments easily.
 *
 * PS_STRINGS defines the location of a process's ps_strings structure.
 */

struct ps_strings {
	char	*ps_argvstr;       /* the argv strings */
	int	ps_nargvstr;       /* number of argv strings */
	char	*ps_envstr;        /* the environment strings */
	int	ps_nenvstr;        /* number of environment strings */
};
#define PS_STRINGS \
        ((struct ps_strings *)(USRSTACK - sizeof(struct ps_strings)))

/*
 * Below the PS_STRINGS and sigtramp, we may require a gap on the stack
 * (used to copyin/copyout various emulation data structures). The base
 * address of this gap may need alignment, so use this to reference it:
 *	(caddr_t)ALIGN(PS_STRINGS - szsigcode - STACKGAPLEN);
 */
#ifdef COMPAT_SUNOS
#define	STACKGAPLEN	400	/* plenty enough for now */
#else
#define	STACKGAPLEN	0
#endif

/*
 * the following structures allow execve() to put together processes
 * in a more extensible and cleaner way.
 *
 * the exec_package struct defines an executable being execve()'d.
 * it contains the header, the vmspace-building commands, the vnode
 * information, and the arguments associated with the newly-execve'd
 * process.
 *
 * the exec_vmcmd struct defines a command description to be used
 * in creating the new process's vmspace.
 */

struct proc;
struct exec_package;

typedef int (*exec_makecmds_fcn) __P((struct proc *, struct exec_package *));
typedef void (*exec_setup_fcn) __P((struct proc *, struct exec_package *));

struct execsw {
	u_int	es_hdrsz;		/* size of header for this format */
	exec_makecmds_fcn es_check;	/* function to check exec format */
};

/* exec vmspace-creation command set; see below */
struct exec_vmcmd_set {
	u_int	evs_cnt;
	u_int	evs_used;
	struct	exec_vmcmd *evs_cmds;
};

#define	EXEC_DEFAULT_VMCMD_SETSIZE	5	/* # of cmds in set to start */

struct exec_package {
	char	*ep_name;		/* file's name */
	void	*ep_hdr;		/* file's exec header */
	u_int	ep_hdrlen;		/* length of ep_hdr */
	u_int	ep_hdrvalid;		/* bytes of ep_hdr that are valid */
	struct nameidata *ep_ndp;	/* namei data pointer for lookups */
	exec_setup_fcn ep_setup;	/* special setup fn for exec type */
	struct	exec_vmcmd_set ep_vmcmds;  /* vmcmds used to build vmspace */
	struct	vnode *ep_vp;		/* executable's vnode */
	struct	vattr *ep_vap;		/* executable's attributes */
	u_long	ep_taddr;		/* process's text address */
	u_long	ep_tsize;		/* size of process's text */
	u_long	ep_daddr;		/* process's data(+bss) address */
	u_long	ep_dsize;		/* size of process's data(+bss) */
	u_long	ep_maxsaddr;		/* proc's max stack addr ("top") */
	u_long	ep_minsaddr;		/* proc's min stack addr ("bottom") */
	u_long	ep_ssize;		/* size of process's stack */
	u_long	ep_entry;		/* process's entry point */
	u_char	ep_emul;		/* os emulation */
	u_int	ep_flags;		/* flags; see below. */
	char	**ep_fa;		/* a fake args vector for scripts */
	int	ep_fd;			/* a file descriptor we're holding */
};
#define	EXEC_INDIR	0x0001		/* script handling already done */
#define	EXEC_HASFD	0x0002		/* holding a shell script */
#define	EXEC_HASARGL	0x0004		/* has fake args vector */
#define	EXEC_SKIPARG	0x0008		/* don't copy user-supplied argv[0] */
#define	EXEC_DESTR	0x0010		/* destructive ops performed */

struct exec_vmcmd {
	int	(*ev_proc) __P((struct proc *p, struct exec_vmcmd *cmd));
				/* procedure to run for region of vmspace */
	u_long	ev_len;		/* length of the segment to map */
	u_long	ev_addr;	/* address in the vmspace to place it at */
	struct	vnode *ev_vp;	/* vnode pointer for the file w/the data */
	u_long	ev_offset;	/* offset in the file for the data */
	u_int	ev_prot;	/* protections for segment */
};

#ifdef KERNEL
/*
 * funtions used either by execve() or the various cpu-dependent execve()
 * hooks.
 */
void	kill_vmcmd		__P((struct exec_vmcmd **));
int	exec_makecmds		__P((struct proc *, struct exec_package *));
int	exec_runcmds		__P((struct proc *, struct exec_package *));
void	vmcmdset_extend		__P((struct exec_vmcmd_set *));
void	kill_vmcmds		__P((struct exec_vmcmd_set *evsp));
int	vmcmd_map_pagedvn	__P((struct proc *, struct exec_vmcmd *));
int	vmcmd_map_readvn	__P((struct proc *, struct exec_vmcmd *));
int	vmcmd_map_zero		__P((struct proc *, struct exec_vmcmd *));

#ifdef DEBUG
void	new_vmcmd __P((struct exec_vmcmd_set *evsp,
		    int (*proc) __P((struct proc *p, struct exec_vmcmd *)),
		    u_long len, u_long addr, struct vnode *vp, u_long offset,
		    u_int prot));
#define	NEW_VMCMD(evsp,proc,len,addr,vp,offset,prot) \
	new_vmcmd(evsp,proc,len,addr,vp,offset,prot);
#else	/* DEBUG */
#define	NEW_VMCMD(evsp,proc,len,addr,vp,offset,prot) { \
	struct exec_vmcmd *vcp; \
	if ((evsp)->evs_used >= (evsp)->evs_cnt) \
		vmcmdset_extend(evsp); \
	vcp = &(evsp)->evs_cmds[(evsp)->evs_used++]; \
	vcp->ev_proc = (proc); \
	vcp->ev_len = (len); \
	vcp->ev_addr = (addr); \
	if ((vcp->ev_vp = (vp)) != NULLVP) \
                VREF(vp); \
        vcp->ev_offset = (offset); \
        vcp->ev_prot = (prot); \
}
#endif /* EXEC_DEBUG */

/*
 * Exec function switch:
 *
 * Note that each makecmds function is responsible for loading the
 * exec package with the necessary functions for any exec-type-specific
 * handling.
 *
 * Functions for specific exec types should be defined in their own
 * header file.
 */
extern struct	execsw execsw[];
extern int	nexecs;
extern int	exec_maxhdrsz;

#endif /* KERNEL */

#endif /* !_SYS_EXEC_H_ */

