/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 * from: Utah $Hdr: trap.c 1.32 91/04/06$
 *
 *	@(#)trap.c	7.15 (Berkeley) 8/2/91
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "acct.h"
#include "kernel.h"
#include "signalvar.h"
#include "resourcevar.h"
#include "syslog.h"
#include "user.h"
#ifdef KTRACE
#include "ktrace.h"
#endif

#include "machine/psl.h"
#include "machine/trap.h"
#include "machine/cpu.h"
#include "machine/reg.h"
#include "machine/mtpr.h"
#include "pte.h"

#include "vm/vm.h"
#include "vm/pmap.h"
#include "vmmeter.h"

#ifdef HPUXCOMPAT
#include "../hpux/hpux.h"
#endif

#ifdef COMPAT_SUNOS
#include "../../sunos/sun_syscall.h"
#endif

struct	sysent	sysent[];
int	nsysent;
#ifdef COMPAT_SUNOS
struct	sysent	sun_sysent[];
int	nsun_sysent;
#endif

char	*trap_type[] = {
	"Bus error",
	"Address error",
	"Illegal instruction",
	"Zero divide",
	"CHK instruction",
	"TRAPV instruction",
	"Privilege violation",
	"Trace trap",
	"MMU fault",
	"SSIR trap",
	"Format error",
	"68881 exception",
	"Coprocessor violation",
	"Async system trap"
};
#define	TRAP_TYPES	(sizeof trap_type / sizeof trap_type[0])

/*
 * Size of various exception stack frames (minus the standard 8 bytes)
 */
short	exframesize[] = {
	FMT0SIZE,	/* type 0 - normal (68020/030/040) */
	FMT1SIZE,	/* type 1 - throwaway (68020/030/040) */
	FMT2SIZE,	/* type 2 - normal 6-word (68020/030/040) */
	-1,		/* type 3 - FP post-instruction (68040) */
	-1, -1, -1,	/* type 4-6 - undefined */
	-1,		/* type 7 - access error (68040) */
	58,		/* type 8 - bus fault (68010) */
	FMT9SIZE,	/* type 9 - coprocessor mid-instruction (68020/030) */
	FMTASIZE,	/* type A - short bus fault (68020/030) */
	FMTBSIZE,	/* type B - long bus fault (68020/030) */
	-1, -1, -1, -1	/* type C-F - undefined */
};

#ifdef DEBUG
int mmudebug = 0;
#endif

/*
 * Trap is called from locore to handle most types of processor traps,
 * including events such as simulated software interrupts/AST's.
 * System calls are broken out for efficiency.
 */
/*ARGSUSED*/
trap(type, code, v, frame)
	int type;
	unsigned code;
	register unsigned v;
	struct frame frame;
{
	register int i;
	unsigned ucode = 0;
	register struct proc *p = curproc;
	struct timeval syst;
	unsigned ncode;
	static int panicing = 0;
	int s;

	cnt.v_trap++;
	syst = p->p_stime;
	if (USERMODE(frame.f_sr)) {
		type |= T_USER;
		p->p_regs = frame.f_regs;
	}
	switch (type) {

	default:
dopanic:
		if (! panicing++)
		  {
		    printf("trap type %d, code = %x, v = %x\n", type, code, v);
		    regdump(frame.f_regs, 128);
		  }
		type &= ~T_USER;
		if ((unsigned)type < TRAP_TYPES)
			panic(trap_type[type]);
		panic("trap");

	case T_BUSERR:		/* kernel bus error */
		if (!p->p_addr->u_pcb.pcb_onfault)
			goto dopanic;
		/*
		 * If we have arranged to catch this fault in any of the
		 * copy to/from user space routines, set PC to return to
		 * indicated location and set flag informing buserror code
		 * that it may need to clean up stack frame.
		 */
copyfault:
		frame.f_stackadj = exframesize[frame.f_format];
		frame.f_format = frame.f_vector = 0;
		frame.f_pc = (int) p->p_addr->u_pcb.pcb_onfault;
		return;

	case T_BUSERR|T_USER:	/* bus error */
	case T_ADDRERR|T_USER:	/* address error */
		i = SIGBUS;
		break;

#ifdef FPCOPROC
	case T_COPERR:		/* kernel coprocessor violation */
#endif
	case T_FMTERR:		/* kernel format error */
	/*
	 * The user has most likely trashed the RTE or FP state info
	 * in the stack frame of a signal handler.
	 */
		type |= T_USER;
		printf("pid %d: kernel %s exception\n", p->p_pid,
		       type==T_COPERR ? "coprocessor" : "format");
		p->p_sigacts->ps_sigact[SIGILL] = SIG_DFL;
		i = sigmask(SIGILL);
		p->p_sigignore &= ~i;
		p->p_sigcatch &= ~i;
		p->p_sigmask &= ~i;
		i = SIGILL;
		ucode = frame.f_format;	/* XXX was ILL_RESAD_FAULT */
		break;

#ifdef FPCOPROC
	case T_COPERR|T_USER:	/* user coprocessor violation */
	/* What is a proper response here? */
		ucode = 0;
		i = SIGFPE;
		break;

	case T_FPERR|T_USER:	/* 68881 exceptions */
	/*
	 * We pass along the 68881 status register which locore stashed
	 * in code for us.  Note that there is a possibility that the
	 * bit pattern of this register will conflict with one of the
	 * FPE_* codes defined in signal.h.  Fortunately for us, the
	 * only such codes we use are all in the range 1-7 and the low
	 * 3 bits of the status register are defined as 0 so there is
	 * no clash.
	 */
		ucode = code;
		i = SIGFPE;
		break;
#endif

	case T_ILLINST|T_USER:	/* illegal instruction fault */
#ifdef HPUXCOMPAT
		if (p->p_flag & SHPUX) {
			ucode = HPUX_ILL_ILLINST_TRAP;
			i = SIGILL;
			break;
		}
		/* fall through */
#endif
	case T_PRIVINST|T_USER:	/* privileged instruction fault */
#ifdef HPUXCOMPAT
		if (p->p_flag & SHPUX)
			ucode = HPUX_ILL_PRIV_TRAP;
		else
#endif
		ucode = frame.f_format;	/* XXX was ILL_PRIVIN_FAULT */
		i = SIGILL;
		break;

	case T_ZERODIV|T_USER:	/* Divide by zero */
#ifdef HPUXCOMPAT
		if (p->p_flag & SHPUX)
			ucode = HPUX_FPE_INTDIV_TRAP;
		else
#endif
		ucode = frame.f_format;	/* XXX was FPE_INTDIV_TRAP */
		i = SIGFPE;
		break;

	case T_CHKINST|T_USER:	/* CHK instruction trap */
#ifdef HPUXCOMPAT
		if (p->p_flag & SHPUX) {
			/* handled differently under hp-ux */
			i = SIGILL;
			ucode = HPUX_ILL_CHK_TRAP;
			break;
		}
#endif
		ucode = frame.f_format;	/* XXX was FPE_SUBRNG_TRAP */
		i = SIGFPE;
		break;

	case T_TRAPVINST|T_USER:	/* TRAPV instruction trap */
#ifdef HPUXCOMPAT
		if (p->p_flag & SHPUX) {
			/* handled differently under hp-ux */
			i = SIGILL;
			ucode = HPUX_ILL_TRAPV_TRAP;
			break;
		}
#endif
		ucode = frame.f_format;	/* XXX was FPE_INTOVF_TRAP */
		i = SIGFPE;
		break;

	/*
	 * XXX: Trace traps are a nightmare.
	 *
	 *	HP-UX uses trap #1 for breakpoints,
	 *	HPBSD uses trap #2,
	 *	SUN 3.x uses trap #15,
	 *	KGDB uses trap #15 (for kernel breakpoints; handled elsewhere).
	 *
	 * HPBSD and HP-UX traps both get mapped by locore.s into T_TRACE.
	 * SUN 3.x traps get passed through as T_TRAP15 and are not really
	 * supported yet.
	 */
	case T_TRACE:		/* kernel trace trap */
	case T_TRAP15:		/* SUN trace trap */
		frame.f_sr &= ~PSL_T;
		i = SIGTRAP;
		break;

	case T_TRACE|T_USER:	/* user trace trap */
	case T_TRAP15|T_USER:	/* SUN user trace trap */
#ifdef COMPAT_SUNOS
		/* SunOS seems to use Trap #2 for some obscure fpu operations.
		   So far, just ignore it, but DONT trap on it.. */
		if (p->p_md.md_emul == MDPE_SUNOS)
		  goto out;
#endif
		frame.f_sr &= ~PSL_T;
		i = SIGTRAP;
		break;

	case T_ASTFLT:		/* system async trap, cannot happen */
		goto dopanic;

	case T_ASTFLT|T_USER:	/* user async trap */
		astpending = 0;
		/*
		 * We check for software interrupts first.  This is because
		 * they are at a higher level than ASTs, and on a VAX would
		 * interrupt the AST.  We assume that if we are processing
		 * an AST that we must be at IPL0 so we don't bother to
		 * check.  Note that we ensure that we are at least at SIR
		 * IPL while processing the SIR.
		 */
		spl1();
		/* fall into... */

	case T_SSIR:		/* software interrupt */
	case T_SSIR|T_USER:
		if (ssir & SIR_NET) {
			siroff(SIR_NET);
			cnt.v_soft++;
			netintr();
		}
		if (ssir & SIR_CLOCK) {
			siroff(SIR_CLOCK);
			cnt.v_soft++;
			softclock((caddr_t)frame.f_pc, (int)frame.f_sr);
		}
		/*
		 * If this was not an AST trap, we are all done.
		 */
		if (type != (T_ASTFLT|T_USER)) {
			cnt.v_trap--;
			return;
		}
		spl0();
#ifndef PROFTIMER
		if ((p->p_flag&SOWEUPC) && p->p_stats->p_prof.pr_scale) {
			addupc(frame.f_pc, &p->p_stats->p_prof, 1);
			p->p_flag &= ~SOWEUPC;
		}
#endif
		goto out;

	case T_MMUFLT:		/* kernel mode page fault */
		/* fall into ... */

	case T_MMUFLT|T_USER:	/* page fault */
	    {
		register vm_offset_t va;
		register struct vmspace *vm = p->p_vmspace;
		register vm_map_t map;
		int rv;
		vm_prot_t ftype;
		extern vm_map_t kernel_map;
		unsigned nss;


		/*
		 * It is only a kernel address space fault iff:
		 * 	1. (type & T_USER) == 0  and
		 * 	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space data fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (type == T_MMUFLT &&
		    (!p->p_addr->u_pcb.pcb_onfault ||
		     (code & (SSW_DF|FC_SUPERD)) == (SSW_DF|FC_SUPERD)))
			map = kernel_map;
		else
			map = &vm->vm_map;
		if ((code & (SSW_DF|SSW_RW)) == SSW_DF)	/* what about RMW? */
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
		va = trunc_page((vm_offset_t)v);
#ifdef DEBUG
		if (map == kernel_map && va == 0) {
			printf("trap: bad kernel access at %x\n", v);
			goto dopanic;
		}
#endif
#ifndef no_386bsd_code
		/*
		 * XXX: rude hack to make stack limits "work"
		 */
		nss = 0;
		if ((caddr_t)va >= vm->vm_maxsaddr && map != kernel_map) {
			nss = clrnd(btoc(USRSTACK - (unsigned)va));
			if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
/*pg("trap rlimit %d, maxsaddr %x va %x ", nss, vm->vm_maxsaddr, va);*/
				rv = KERN_FAILURE;
				goto nogo;
			}
		}
#endif

		rv = vm_fault(map, va, ftype, FALSE);
/*printf("vmfault %s %x returned %d\n", map == kernel_map ? "kernel" : "user", va, rv);*/
#ifdef no_386bsd_code
		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((caddr_t)va >= vm->vm_maxsaddr && map != kernel_map) {
			if (rv == KERN_SUCCESS) {
				nss = clrnd(btoc(USRSTACK-(unsigned)va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}

		if (rv == KERN_SUCCESS) {
			if (type == T_MMUFLT)
				return;
			goto out;
		}
#else /* use hacky 386bsd_code */
		if (rv == KERN_SUCCESS) {
			/*
			 * XXX: continuation of rude stack hack
			 */
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
			if (type == T_MMUFLT)
				return;
			goto out;
		}
nogo:
#endif
		if (type == T_MMUFLT) {
			if (p->p_addr->u_pcb.pcb_onfault)
				goto copyfault;
			printf("vm_fault(%x, %x, %x, 0) -> %x\n",
			       map, va, ftype, rv);
			printf("  type %x, code [mmu,,ssw]: %x\n",
			       type, code);
			goto dopanic;
		}
		ucode = v;
		i = (rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV;
		break;
	    }
	}
printf("trapsignal(%d, %d, %d, %x, %x)\n", p->p_pid, i, ucode, v, frame.f_regs[PC]);
	trapsignal(p, i, ucode);
	if ((type & T_USER) == 0)
		return;
out:
	while (i = CURSIG(p))
		psig(i);
	p->p_pri = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		s = splclock();
		setrq(p);
		p->p_stats->p_ru.ru_nivcsw++;
		swtch();
		splx(s);
		while (i = CURSIG(p))
			psig(i);
	}
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(frame.f_pc, &p->p_stats->p_prof,
			    ticks * profscale);
#else
			addupc(frame.f_pc, &p->p_stats->p_prof, ticks);
#endif
		}
	}
	curpri = p->p_pri;
}

/*
 * Proces a system call.
 */
syscall(code, frame)
	volatile int code;
	struct frame frame;
{
	register caddr_t params;
	register int i;
	register struct sysent *callp;
	register struct proc *p = curproc;
	int error, opc, numsys, s;
	struct args {
		int i[8];
	} args;
	int rval[2];
	struct timeval syst;
	struct sysent *systab;

	cnt.v_syscall++;
	syst = p->p_stime;
	if (!USERMODE(frame.f_sr))
		panic("syscall");
	p->p_regs = frame.f_regs;
	opc = frame.f_pc - 2;
	switch (p->p_md.md_emul)
	  {
#ifdef COMPAT_SUNOS
	  case MDPE_SUNOS:
	    systab = sun_sysent;
	    numsys = nsun_sysent;

	    /* SunOS passes the syscall-number on the stack, whereas
	       BSD passes it in D0. So, we have to get the real "code"
	       from the stack, and clean up the stack, as SunOS glue
	       code assumes the kernel pops the syscall argument the
	       glue pushed on the stack. Sigh... */
	    code = fuword ((caddr_t) frame.f_regs[SP]);
	    /* XXX don't do this for sun_sigreturn, as there's no
	       XXX stored pc on the stack to skip, the argument follows
	       XXX the syscall number without a gap. */
	    if (code != SYS_sun_sigreturn)
	      {
		frame.f_regs[SP] += sizeof (int);
		/* remember that we adjusted the SP, might have to undo
		   this if the system call returns ERESTART. */
		p->p_md.md_flags |= MDP_STACKADJ;
	      }
	    break;
#endif

	  case MDPE_NETBSD:
	  default:
	    systab = sysent;
	    numsys = nsysent;
	    break;
	  }
	params = (caddr_t)frame.f_regs[SP] + sizeof(int);
	if (code == 0) {			/* indir */
		code = fuword(params);
		params += sizeof(int);
	}
	if (code >= numsys)
		callp = &systab[0];		/* indir (illegal) */
	else
		callp = &systab[code];
	if ((i = callp->sy_narg * sizeof (int)) &&
	    (error = copyin(params, (caddr_t)&args, (u_int)i))) {
		frame.f_regs[D0] = error;
		frame.f_sr |= PSL_C;	/* carry bit */
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, callp->sy_narg, args.i);
#endif
		goto done;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_narg, args.i);
#endif

	rval[0] = 0;
	rval[1] = frame.f_regs[D1];
#if 0
if (p->p_md.md_emul == MDPE_SUNOS)
  {
    int arg;
    extern char *sun_syscallnames[];
    printf("{%d [%d]%s} ", p->p_pid, code, sun_syscallnames[code]);
  }
#endif
	error = (*callp->sy_call)(p, &args, rval);
#if 0
if (p->p_md.md_emul == MDPE_SUNOS)
  {
    int arg;
    extern char *sun_syscallnames[];
    printf("%d:[%d]%s (", p->p_pid, code, sun_syscallnames[code]);
    for (arg = 0; arg < callp->sy_narg; arg++)
      printf ("%s0x%x", arg ? ", " : "", args.i[arg]);
    printf(") = %d/%d [%d]\n", rval[0], rval[1], error);
  }
#endif
	if (error == ERESTART)
		frame.f_pc = opc;
	else if (error != EJUSTRETURN) {
		if (error) {
			frame.f_regs[D0] = error;
			frame.f_sr |= PSL_C;	/* carry bit */
		} else {
			frame.f_regs[D0] = rval[0];
			frame.f_regs[D1] = rval[1];
			frame.f_sr &= ~PSL_C;
		}
	}
	/* else if (error == EJUSTRETURN) */
		/* nothing to do */

done:
	/*
	 * Reinitialize proc pointer `p' as it may be different
	 * if this is a child returning from fork syscall.
	 */
	p = curproc;
#ifdef COMPAT_SUNOS
	/* need new p-value for this */
	if (error == ERESTART && (p->p_md.md_flags & MDP_STACKADJ))
	  {
	    frame.f_regs[SP] -= sizeof (int);
	    p->p_md.md_flags &= ~MDP_STACKADJ;
	  }
#endif
	while (i = CURSIG(p))
		psig(i);
	p->p_pri = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrq ourselves but before we
		 * swtch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		s = splclock();
		setrq(p);
		p->p_stats->p_ru.ru_nivcsw++;
		swtch();
		splx(s);
		while (i = CURSIG(p))
			psig(i);
	}
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(frame.f_pc, &p->p_stats->p_prof,
			    ticks * profscale);
#else
			addupc(frame.f_pc, &p->p_stats->p_prof, ticks);
#endif
		}
	}
	curpri = p->p_pri;
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}
