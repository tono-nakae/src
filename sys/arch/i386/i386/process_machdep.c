/*	$NetBSD: process_machdep.c,v 1.12 1995/01/15 00:42:01 mycroft Exp $	*/

/*
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1993 Jan-Simon Pendry
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 * From:
 *	Id: procfs_i386.c,v 4.1 1993/12/17 10:47:45 jsp Rel
 */

/*
 * This file may seem a bit stylized, but that so that it's easier to port.
 * Functions to be implemented here are:
 *
 * process_read_regs(proc, regs)
 *	Get the current user-visible register set from the process
 *	and copy it into the regs structure (<machine/reg.h>).
 *	The process is stopped at the time read_regs is called.
 *
 * process_write_regs(proc, regs)
 *	Update the current register set from the passed in regs
 *	structure.  Take care to avoid clobbering special CPU
 *	registers or privileged bits in the PSL.
 *	The process is stopped at the time write_regs is called.
 *
 * process_sstep(proc)
 *	Arrange for the process to trap after executing a single instruction.
 *
 * process_set_pc(proc)
 *	Set the process's program counter.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/ptrace.h>
#include <machine/psl.h>
#include <machine/reg.h>

extern int kstack[];		/* XXX */

static inline struct trapframe *
process_frame(p)
	struct proc *p;
{
	void *ptr;

	if ((p->p_flag & P_INMEM) == 0)
		return (NULL);

	ptr = (char *)p->p_addr + ((char *)p->p_md.md_regs - (char *)kstack);
	return (ptr);
}

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf;

	tf = process_frame(p);
	if (tf == NULL)
		return (EIO);

	regs->r_es     = tf->tf_es;
	regs->r_ds     = tf->tf_ds;
	regs->r_edi    = tf->tf_edi;
	regs->r_esi    = tf->tf_esi;
	regs->r_ebp    = tf->tf_ebp;
	regs->r_ebx    = tf->tf_ebx;
	regs->r_edx    = tf->tf_edx;
	regs->r_ecx    = tf->tf_ecx;
	regs->r_eax    = tf->tf_eax;
	regs->r_eip    = tf->tf_eip;
	regs->r_cs     = tf->tf_cs;
	regs->r_eflags = tf->tf_eflags;
	regs->r_esp    = tf->tf_esp;
	regs->r_ss     = tf->tf_ss;

	return (0);
}

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	struct trapframe *tf;

	tf = process_frame(p);
	if (tf == NULL)
		return (EIO);

	/*
	 * Check for security violations.
	 */
	if (((regs->r_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
	    ISPL(regs->r_cs) != SEL_UPL)
		return (EINVAL);

	tf->tf_es     = regs->r_es;
	tf->tf_ds     = regs->r_ds;
	tf->tf_edi    = regs->r_edi;
	tf->tf_esi    = regs->r_esi;
	tf->tf_ebp    = regs->r_ebp;
	tf->tf_ebx    = regs->r_ebx;
	tf->tf_edx    = regs->r_edx;
	tf->tf_ecx    = regs->r_ecx;
	tf->tf_eax    = regs->r_eax;
	tf->tf_eip    = regs->r_eip;
	tf->tf_cs     = regs->r_cs;
	tf->tf_eflags = regs->r_eflags;
	tf->tf_esp    = regs->r_esp;
	tf->tf_ss     = regs->r_ss;

	return (0);
}

int
process_sstep(p, sstep)
	struct proc *p;
{
	struct trapframe *tf;

	tf = process_frame(p);
	if (tf == NULL)
		return (EIO);

	if (sstep)
		tf->tf_eflags |= PSL_T;
	else
		tf->tf_eflags &= ~PSL_T;
	
	return (0);
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	struct trapframe *tf;

	tf = process_frame(p);
	if (tf == NULL)
		return (EIO);

	tf->tf_eip = (int)addr;

	return (0);
}
