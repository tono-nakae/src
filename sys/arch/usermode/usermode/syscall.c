/* $NetBSD: syscall.c,v 1.11 2011/11/27 21:38:17 reinoud Exp $ */

/*-
 * Copyright (c) 2007 Jared D. McNeill <jmcneill@invisible.ca>
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
__KERNEL_RCSID(0, "$NetBSD: syscall.c,v 1.11 2011/11/27 21:38:17 reinoud Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lwp.h>
#include <sys/sched.h>
#include <sys/ktrace.h>
#include <sys/syscall.h>
#include <sys/syscallvar.h>
#include <sys/syscallargs.h>

#include <sys/userret.h>
#include <machine/pcb.h>
#include <machine/thunk.h>
#include <machine/machdep.h>


void userret(struct lwp *l);

void
userret(struct lwp *l)
{
	/* invoke MI userret code */
	mi_userret(l);
}

void
child_return(void *arg)
{
	lwp_t *l = arg;
//	struct pcb *pcb = lwp_getpcb(l);

	/* XXX? */
//	frame->registers[0] = 0;

	aprint_debug("child return! lwp %p\n", l);
	userret(l);
	ktrsysret(SYS_fork, 0, 0);
}

extern const char *const syscallnames[];

void
syscall(void)
{	
	lwp_t *l = curlwp;
	const struct proc * const p = l->l_proc;
	const struct sysent *callp;
	struct pcb *pcb = lwp_getpcb(l);
	ucontext_t *ucp = &pcb->pcb_userret_ucp;
	register_t copyargs[2+SYS_MAXSYSARGS];
	register_t *args;
	register_t rval[2];
	uint32_t code, opcode;
	uint nargs, argsize;
	int error;

	/* system call accounting */
	curcpu()->ci_data.cpu_nsyscall++;
	LWP_CACHE_CREDS(l, l->l_proc);

	/* XXX do we want do do emulation? */
	md_syscall_get_opcode(ucp, &opcode);
	md_syscall_get_syscallnumber(ucp, &code);
	code &= (SYS_NSYSENT -1);

	callp   = p->p_emul->e_sysent + code;
	nargs   = callp->sy_narg;
	argsize = callp->sy_argsize;

	args  = copyargs;
	rval[0] = rval[1] = 0;
	error = md_syscall_getargs(l, ucp, nargs, argsize, args);

	if (code != 4) {
		printf("code %3d, nargs %d, argsize %3d\t%s(", 
			code, nargs, argsize, syscallnames[code]);
		for (int i = 0; i < nargs; i++)
			printf("%"PRIx32", ", (uint) args[i]);
		if (nargs)
			printf("\b\b");
		printf(") ");
	}
#if 0
	aprint_debug("syscall no. %d, ", code);
	aprint_debug("nargs %d, argsize %d =>  ", nargs, argsize);
	dprintf_debug("syscall no. %d, ", code);
	dprintf_debug("nargs %d, argsize %d =>  ", nargs, argsize);
#endif
#if 0
	if ((code == 4)) {
		dprintf_debug("[us] %s", (char *) args[1]);
		printf("[us] %s", (char *) args[1]);
	}
#endif
	if (code == 440)
		printf("stat(%d, %p) ", (uint32_t) args[0],
			(void *) args[1]);

	md_syscall_inc_pc(ucp, opcode);

	if (!error) 
		error = (*callp->sy_call)(l, args, rval);

	if (code != 4)
		printf("=> %s: %d, (%"PRIx32", %"PRIx32")\n",
			error?"ERROR":"OK", error, (uint) (rval[0]), (uint) (rval[1]));

//out:
	switch (error) {
	default:
		/* fall trough */
	case 0:
		md_syscall_set_returnargs(l, ucp, error, rval);
		/* fall trough */
	case EJUSTRETURN:
		break;
	case ERESTART:
		md_syscall_dec_pc(ucp, opcode);
		/* nothing to do */
		break;
	}
	//dprintf_debug("end of syscall : return to userland\n");
//if (code != 4) printf("userret() code %d\n", code);
	userret(l);
}

