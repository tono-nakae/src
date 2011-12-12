/* $NetBSD: machdep.c,v 1.34 2011/12/12 19:57:12 reinoud Exp $ */

/*-
 * Copyright (c) 2011 Reinoud Zandijk <reinoud@netbsd.org>
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

#include "opt_memsize.h"
#include "opt_sdl.h"
#include "opt_urkelvisor.h"

#include <sys/cdefs.h>
__KERNEL_RCSID(0, "$NetBSD: machdep.c,v 1.34 2011/12/12 19:57:12 reinoud Exp $");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/boot_flag.h>
#include <sys/ucontext.h>
#include <machine/pcb.h>
#include <machine/psl.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_page.h>

#include <dev/mm.h>
#include <machine/machdep.h>
#include <machine/thunk.h>

#if defined(URKELVISOR)
#include <machine/urkelvisor.h>
#endif

char machine[] = "usermode";
char machine_arch[] = "usermode";

static char **saved_argv;
char *usermode_root_image_path = NULL;

void	main(int argc, char *argv[]);
void	usermode_reboot(void);

void
main(int argc, char *argv[])
{
#if defined(SDL)
	extern int genfb_thunkbus_cnattach(void);
#endif
	extern void ttycons_consinit(void);
	extern void pmap_bootstrap(void);
	extern void kernmain(void);
	int i, j, r, tmpopt = 0;

	saved_argv = argv;

#if defined(SDL)
	if (genfb_thunkbus_cnattach() == 0)
#endif
		ttycons_consinit();

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			usermode_root_image_path = argv[i];
			continue;
		}
		for (j = 1; argv[i][j] != '\0'; j++) {
			r = 0;
			BOOT_FLAG(argv[i][j], r);
			if (r == 0) {
				printf("-%c: unknown flag\n", argv[i][j]);
				printf("usage: %s [-acdqsvxz]\n", argv[0]);
				printf("       (ex. \"%s -s\")\n", argv[0]);
				return;
			}
			tmpopt |= r;
		}
	}
	boothowto = tmpopt;

	uvm_setpagesize();
	uvmexp.ncolors = 2;

	pmap_bootstrap();

#if defined(URKELVISOR)
	urkelvisor_init();
#endif

	splinit();
	splraise(IPL_HIGH);

	kernmain();
}

void
usermode_reboot(void)
{
	struct thunk_itimerval itimer;

	/* make sure the timer is turned off */
	memset(&itimer, 0, sizeof(itimer));
	thunk_setitimer(ITIMER_REAL, &itimer, NULL);

	if (thunk_execv(saved_argv[0], saved_argv) == -1)
		thunk_abort();
	/* NOTREACHED */
}

void
setstatclockrate(int arg)
{
}

void
consinit(void)
{
	printf("NetBSD/usermode startup\n");
}

void
sendsig_siginfo(const ksiginfo_t *ksi, const sigset_t *mask)
{
	panic("%s not implemented", __func__);
}

int
mm_md_physacc(paddr_t pa, vm_prot_t prog)
{
	return 0;
}


#ifdef __i386__

#if 0
static void dump_regs(ucontext_t *ctx);

static void
dump_regs(register_t *reg)
{
	/* register dump before call */
	const char *name[] = {"GS", "FS", "ES", "DS", "EDI", "ESI", "EBP", "ESP",
		"EBX", "EDX", "ECX", "EAX", "TRAPNO", "ERR", "EIP", "CS", "EFL",
		"UESP", "SS"};

	for (i =0; i < 19; i++)
		printf("reg[%02d] (%6s) = %"PRIx32"\n", i, name[i], reg[i]);
}
#endif

void
setregs(struct lwp *l, struct exec_package *pack, vaddr_t stack)
{
	struct pcb *pcb = lwp_getpcb(l);
	ucontext_t *ucp = &pcb->pcb_userret_ucp;
	uint *reg, i;

#ifdef DEBUG_EXEC
	printf("setregs called: lwp %p, exec package %p, stack %p\n",
		l, pack, (void *) stack);
	printf("current stat of pcb %p\n", pcb);
	printf("\tpcb->pcb_ucp.uc_stack.ss_sp   = %p\n",
		pcb->pcb_ucp.uc_stack.ss_sp);
	printf("\tpcb->pcb_ucp.uc_stack.ss_size = %d\n",
		(int) pcb->pcb_ucp.uc_stack.ss_size);
	printf("\tpcb->pcb_userret_ucp.uc_stack.ss_sp   = %p\n",
		pcb->pcb_userret_ucp.uc_stack.ss_sp);
	printf("\tpcb->pcb_userret_ucp.uc_stack.ss_size = %d\n",
		(int) pcb->pcb_userret_ucp.uc_stack.ss_size);
#endif

	reg = (int *) &ucp->uc_mcontext;
	for (i = 4; i < 11; i++)
		reg[i] = 0;

	ucp->uc_stack.ss_sp = (void *) (stack-4);	/* to prevent clearing */
	ucp->uc_stack.ss_size = 0; //pack->ep_ssize;
	thunk_makecontext(ucp, (void *) pack->ep_entry, 0, NULL, NULL, NULL);

	/* patch up */
	reg[ 8] = l->l_proc->p_psstrp;	/* _REG_EBX */
	reg[17] = (stack);		/* _REG_UESP */

	//dump_regs(reg);

#ifdef DEBUG_EXEC
	printf("updated pcb %p\n", pcb);
	printf("\tpcb->pcb_ucp.uc_stack.ss_sp   = %p\n",
		pcb->pcb_ucp.uc_stack.ss_sp);
	printf("\tpcb->pcb_ucp.uc_stack.ss_size = %d\n",
		(int) pcb->pcb_ucp.uc_stack.ss_size);
	printf("\tpcb->pcb_userret_ucp.uc_stack.ss_sp   = %p\n",
		pcb->pcb_userret_ucp.uc_stack.ss_sp);
	printf("\tpcb->pcb_userret_ucp.uc_stack.ss_size = %d\n",
		(int) pcb->pcb_userret_ucp.uc_stack.ss_size);
	printf("\tpack->ep_entry                = %p\n",
		(void *) pack->ep_entry);
#endif
}

void
md_syscall_get_syscallnumber(ucontext_t *ucp, uint32_t *code)
{
	uint *reg = (int *) &ucp->uc_mcontext;
	*code = reg[11];			/* EAX */
}

int
md_syscall_getargs(lwp_t *l, ucontext_t *ucp, int nargs, int argsize,
	register_t *args)
{
	uint *reg = (int *) &ucp->uc_mcontext;
	register_t *sp = (register_t *) reg[17];/* ESP */
	int ret;

	//dump_regs(reg);
	ret = copyin(sp + 1, args, argsize);

	return ret;
}

void
md_syscall_set_returnargs(lwp_t *l, ucontext_t *ucp,
	int error, register_t *rval)
{
	register_t *reg = (register_t *) &ucp->uc_mcontext;

	reg[16] &= ~PSL_C;		/* EFL */
	if (error > 0) {
		rval[0] = error;
		reg[16] |= PSL_C;	/* EFL */
	}

	/* set return parameters */
	reg[11]	= rval[0];		/* EAX */
	if (error == 0)
		reg[ 9] = rval[1];	/* EDX */

	//dump_regs(reg);
}

int
md_syscall_check_opcode(void *ptr)
{
	uint16_t *p16;

	/* undefined instruction */
	p16 = (uint16_t *) ptr;
	if (*p16 == 0xff0f)
		return 1;
	if (*p16 == 0xff0b)
		return 1;

	/* TODO int $80 and sysenter */
	return 0;
}

void
md_syscall_get_opcode(ucontext_t *ucp, uint32_t *opcode)
{
	uint *reg = (int *) &ucp->uc_mcontext;
//	uint8_t  *p8  = (uint8_t *) (reg[14]);
	uint16_t *p16 = (uint16_t*) (reg[14]);

	*opcode = 0;

	if (*p16 == 0xff0f)
		*opcode = *p16;
	if (*p16 == 0xff0b)
		*opcode = *p16;

	/* TODO int $80 and sysenter */
}

void
md_syscall_inc_pc(ucontext_t *ucp, uint32_t opcode)
{
	uint *reg = (int *) &ucp->uc_mcontext;

	/* advance program counter */
	if (opcode == 0xff0f)
		reg[14] += 2;	/* EIP */
	if (opcode == 0xff0b)
		reg[14] += 2;	/* EIP */

	/* TODO int $80 and sysenter */
}

void
md_syscall_dec_pc(ucontext_t *ucp, uint32_t opcode)
{
	uint *reg = (int *) &ucp->uc_mcontext;

	/* advance program counter */
	if (opcode == 0xff0f)
		reg[14] -= 2;	/* EIP */
	if (opcode == 0xff0b)
		reg[14] -= 2;	/* EIP */

	/* TODO int $80 and sysenter */
}


#else
#	error machdep functions not yet ported to this architecture
#endif

