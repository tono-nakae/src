/*	$NetBSD: machdep.c,v 1.16 1998/08/21 16:13:29 tsubai Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_atalk.h"
#include "opt_ccitt.h"
#include "opt_iso.h"
#include "opt_ns.h"
#include "opt_natm.h"
#include "opt_uvm.h"
#include "ipkdb.h"

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/callout.h>
#include <sys/exec.h>
#include <sys/malloc.h>
#include <sys/map.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/user.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>

#if defined(UVM)
#include <uvm/uvm_extern.h>
#endif

#include <net/netisr.h>

#ifdef INET
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_inarp.h>
#include <netinet/ip_var.h>
#endif

#include <machine/bat.h>
#include <machine/pmap.h>
#include <machine/powerpc.h>
#include <machine/trap.h>

#include <dev/cons.h>
#include <dev/ofw/openfirm.h>

#include "adb.h"

#if defined(UVM)
vm_map_t exec_map = NULL;
vm_map_t mb_map = NULL;
vm_map_t phys_map = NULL;
#else
vm_map_t buffer_map;
#endif

/*
 * Global variables used here and there
 */
struct pcb *curpcb;
struct pmap *curpm;
struct proc *fpuproc;

extern struct user *proc0paddr;

struct bat battable[16];
int astpending;
char *bootpath;

/*
 * We use the page just above the interrupt vector as message buffer
 */
#define MSGBUFADDR 0x3000
#define MSGBUFSIZE 4096
int msgbufmapped = 0;

#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif

caddr_t allocsys __P((caddr_t));
void install_extint __P((void (*)(void)));

extern u_int openfirmware_entry;
static u_int ofw_pa;

int cold = 1;

void
initppc(startkernel, endkernel, args)
	u_int startkernel, endkernel;
	char *args;
{
	extern trapcode, trapsize;
	extern dsitrap, dsisize;
	extern isitrap, isisize;
	extern decrint, decrsize;
	extern tlbimiss, tlbimsize;
	extern tlbdlmiss, tlbdlmsize;
	extern tlbdsmiss, tlbdsmsize;
#ifdef DDB
	extern ddblow, ddbsize;
	extern void *startsym, *endsym;
#endif
#if NIPKDB > 0
	extern ipkdblow, ipkdbsize;
#endif
	extern void consinit __P((void));
	extern void callback __P((void *));
	extern void ext_intr __P((void));
	int exc, scratch;

	int chosen, mmu, mode, exists;

	/*
	 * Read translations for Openfirmware call.
	 */
	chosen = OF_finddevice("/chosen");
	OF_getprop(chosen, "mmu", &mmu, 4);
	OF_call_method("translate", mmu, 1, 3,
		       0xff800000, &ofw_pa, &mode, &exists);

	proc0.p_addr = proc0paddr;
	bzero(proc0.p_addr, sizeof *proc0.p_addr);

	curpcb = &proc0paddr->u_pcb;

	curpm = curpcb->pcb_pmreal = curpcb->pcb_pm = pmap_kernel();

#if 0
	/*
	 * i386 port says, that this shouldn't be here,
	 * but I really think the console should be initialized
	 * as early as possible.
	 */
	consinit();
#endif

#ifdef	__notyet__		/* Needs some rethinking regarding real/virtual OFW */
	OF_set_callback(callback);
#endif
	/*
	 * Initialize BAT registers to unmapped to not generate
	 * overlapping mappings below.
	 */
	asm volatile ("mtibatu 0,%0" :: "r"(0));
	asm volatile ("mtibatu 1,%0" :: "r"(0));
	asm volatile ("mtibatu 2,%0" :: "r"(0));
	asm volatile ("mtibatu 3,%0" :: "r"(0));
	asm volatile ("mtdbatu 0,%0" :: "r"(0));
	asm volatile ("mtdbatu 1,%0" :: "r"(0));
	asm volatile ("mtdbatu 2,%0" :: "r"(0));
	asm volatile ("mtdbatu 3,%0" :: "r"(0));

	/*
	 * Set up BAT0 to only map the lowest 256 MB area
	 */
	battable[0].batl = BATL(0x00000000, BAT_M);
	battable[0].batu = BATU(0x00000000);

	/*
	 * BAT1 maps Openfirmware working area
	 */
	battable[1].batl = ofw_pa | 0x02;
	battable[1].batu = 0xff80001e;		/* 1MB */

	/*
	 * BAT2 maps most I/O devices
	 * 0xf0000000-0xf7ffffff (128MB) --> 0xf0000000-
	 */
	battable[2].batl = 0xf0000002 | BAT_I;
	battable[2].batu = 0xf0000ffe;

	/*
	 * BAT3 maps video ram
	 * (XXX upper half slots only...)
	 */
	battable[3].batl = BATL(0x80000000, BAT_I);
	battable[3].batu = BATU(0x80000000);

	/*
	 * Now setup fixed bat registers
	 *
	 * Note that we still run in real mode, and the BAT
	 * registers were cleared above.
	 */
	/* BAT0 used for initial 256 MB segment */
	asm volatile ("mtibatl 0,%0; mtibatu 0,%1;"
		      "mtdbatl 0,%0; mtdbatu 0,%1;"
		      :: "r"(battable[0].batl), "r"(battable[0].batu));
	/* BAT1 Openfirmware */
	asm volatile ("mtdbatl 1,%0; mtdbatu 1,%1;"
		      "mtibatl 1,%0; mtibatu 1,%1;"
		      :: "r"(battable[1].batl), "r"(battable[1].batu));
	/* BAT2 obio devices */
	asm volatile ("mtdbatl 2,%0; mtdbatu 2,%1"
		      :: "r"(battable[2].batl), "r"(battable[2].batu));
	/* BAT3 video ram */
	asm volatile ("mtdbatl 3,%0; mtdbatu 3,%1"
		      :: "r"(battable[3].batl), "r"(battable[3].batu));

	/*
	 * Set up trap vectors
	 */
	for (exc = EXC_RSVD; exc <= EXC_LAST; exc += 0x100)
		switch (exc) {
		default:
			bcopy(&trapcode, (void *)exc, (size_t)&trapsize);
			break;
		case EXC_EXI:
			/*
			 * This one is (potentially) installed during autoconf
			 */
			break;
		case EXC_DSI:
			bcopy(&dsitrap, (void *)EXC_DSI, (size_t)&dsisize);
			break;
		case EXC_ISI:
			bcopy(&isitrap, (void *)EXC_ISI, (size_t)&isisize);
			break;
		case EXC_DECR:
			bcopy(&decrint, (void *)EXC_DECR, (size_t)&decrsize);
			break;
		case EXC_IMISS:
			bcopy(&tlbimiss, (void *)EXC_IMISS, (size_t)&tlbimsize);
			break;
		case EXC_DLMISS:
			bcopy(&tlbdlmiss, (void *)EXC_DLMISS, (size_t)&tlbdlmsize);
			break;
		case EXC_DSMISS:
			bcopy(&tlbdsmiss, (void *)EXC_DSMISS, (size_t)&tlbdsmsize);
			break;
#if defined(DDB) || NIPKDB > 0
		case EXC_PGM:
		case EXC_TRC:
		case EXC_BPT:
#if defined(DDB)
			bcopy(&ddblow, (void *)exc, (size_t)&ddbsize);
#else
			bcopy(&ipkdblow, (void *)exc, (size_t)&ipkdbsize);
#endif
			break;
#endif /* DDB || NIPKDB > 0 */
		}

	/*
	 * external interrupt handler install
	 */
	install_extint(ext_intr);

	syncicache((void *)EXC_RST, EXC_LAST - EXC_RST + 0x100);

	/*
	 * Now enable translation (and machine checks/recoverable interrupts).
	 */
	asm volatile ("mfmsr %0; ori %0,%0,%1; mtmsr %0; isync"
		      : "=r"(scratch) : "K"(PSL_IR|PSL_DR|PSL_ME|PSL_RI));

	/*
	 * Parse arg string.
	 */
	bootpath = args;
	while (*++args && *args != ' ');
	if (*args) {
		*args++ = 0;
		while (*args) {
			switch (*args++) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			}
		}
	}

#ifdef DDB
	/* ddb_init(startsym, endsym); */
#endif
#if NIPKDB > 0
	/*
	 * Now trap to IPKDB
	 */
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#endif

	/*
	 * Set the page size.
	 */
#if defined(UVM)
	uvm_setpagesize();
#else
	vm_set_page_size();
#endif

	/*
	 * Initialize pmap module.
	 */
	pmap_bootstrap(startkernel, endkernel);
}

/*
 * This should probably be in autoconf!				XXX
 */
int cpu;
char cpu_model[80];
char machine[] = MACHINE;		/* from <machine/param.h> */
char machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */

void
identifycpu()
{
	int pvr;

	/*
	 * Find cpu type (Do it by OpenFirmware?)
	 */
	asm ("mfpvr %0" : "=r"(pvr));
	cpu = pvr >> 16;
	switch (cpu) {
	case 1:
		sprintf(cpu_model, "601");
		break;
	case 3:
		sprintf(cpu_model, "603");
		break;
	case 4:
		sprintf(cpu_model, "604");
		break;
	case 5:
		sprintf(cpu_model, "602");
		break;
	case 6:
		sprintf(cpu_model, "603e");
		break;
	case 7:
		sprintf(cpu_model, "603ev");
		break;
	case 9:
		sprintf(cpu_model, "604ev");
		break;
	case 20:
		sprintf(cpu_model, "620");
		break;
	default:
		sprintf(cpu_model, "Version %x", cpu);
		break;
	}
	sprintf(cpu_model + strlen(cpu_model), " (Revision %x)", pvr & 0xffff);
	printf("CPU: %s\n", cpu_model);
}

void
install_extint(handler)
	void (*handler) __P((void));
{
	extern extint, extsize;
	extern u_long extint_call;
	u_long offset = (u_long)handler - (u_long)&extint_call;
	int omsr, msr;

#ifdef	DIAGNOSTIC
	if (offset > 0x1ffffff)
		panic("install_extint: too far away");
#endif
	asm volatile ("mfmsr %0; andi. %1,%0,%2; mtmsr %1"
		      : "=r"(omsr), "=r"(msr) : "K"((u_short)~PSL_EE));
	extint_call = (extint_call & 0xfc000003) | offset;
	bcopy(&extint, (void *)EXC_EXI, (size_t)&extsize);
	syncicache((void *)&extint_call, sizeof extint_call);
	syncicache((void *)EXC_EXI, (int)&extsize);
	asm volatile ("mtmsr %0" :: "r"(omsr));
}

/*
 * Machine dependent startup code.
 */
void
cpu_startup()
{
	int sz, i;
	caddr_t v;
	vaddr_t minaddr, maxaddr;
	int base, residual;

	initmsgbuf((caddr_t)MSGBUFADDR, round_page(MSGBUFSIZE));
	msgbufmapped = 1;

	proc0.p_addr = proc0paddr;
	v = (caddr_t)proc0paddr + USPACE;

	printf("%s", version);
	identifycpu();

	printf("real mem  = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);
#if defined(UVM)
	if ((v = (caddr_t)uvm_km_zalloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
#else
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
#endif
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	sz = MAXBSIZE * nbuf;
#if defined(UVM)
	minaddr = 0;
	if (uvm_map(kernel_map, (vaddr_t *)&minaddr, round_page(sz),
		NULL, UVM_UNKNOWN_OFFSET,
		UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
			    UVM_ADV_NORMAL, 0)) != KERN_SUCCESS)
		panic("startup: cannot allocate VM for buffers");
	buffers = (char *)minaddr;
#else
	buffer_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr, sz, TRUE);
	buffers = (char *)minaddr;
	if (vm_map_find(buffer_map, vm_object_allocate(sz), (vaddr_t)0,
			&minaddr, sz, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
#endif /* UVM */
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE) {
		/* Don't want to alloc more physical mem than ever needed */
		base = MAXBSIZE;
		residual = 0;
	}
	for (i = 0; i < nbuf; i++) {
#if defined(UVM)
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		curbuf = (vaddr_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base + 1 : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for "
				    "buffer cache");
			pmap_enter(kernel_map->pmap, curbuf,
				   VM_PAGE_TO_PHYS(pg), VM_PROT_ALL, TRUE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
#else /* !UVM */
		vsize_t curbufsize;
		vaddr_t curbuf;

		curbuf = (vaddr_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base + 1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf + curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
#endif /* UVM */
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
#if defined(UVM)
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE, FALSE, NULL);
#else
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);
#endif

	/*
	 * Allocate a submap for physio
	 */
#if defined(UVM)
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE, FALSE, NULL);
#else
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);
#endif

	/*
	 * Finally, allocate mbuf cluster submap.
	 */
#if defined(UVM)
	mb_map = uvm_km_suballoc(kernel_map, (vaddr_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE, FALSE, NULL);
#else
	mb_map = kmem_suballoc(kernel_map, (vaddr_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);
#endif

	/*
	 * Initialize callouts.
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i - 1].c_next = &callout[i];

#if defined(UVM)
	printf("avail mem = %ld\n", ptoa(uvmexp.free));
#else
	printf("avail mem = %ld\n", ptoa(cnt.v_free_count));
#endif
	printf("using %d buffers containing %d bytes of memory\n",
	       nbuf, bufpages * CLBYTES);

	/*
	 * Set up the buffers.
	 */
	bufinit();

	/*
	 * Now allow hardware interrupts.
	 */
	{
		int msr;

		splhigh();
		asm volatile ("mfmsr %0; ori %0,%0,%1; mtmsr %0"
			      : "=r"(msr) : "K"((u_short)(PSL_EE|PSL_RI)));
	}

	/*
	 * Configure devices.
	 */
	configure();
}

/*
 * Allocate space for system data structures.
 */
caddr_t
allocsys(v)
	caddr_t v;
{
#define	valloc(name, type, num) \
	v = (caddr_t)(((name) = (type *)v) + (num))

	valloc(callout, struct callout, ncallout);
#ifdef	SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef	SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef	SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	/*
	 * Decide on buffer space to use.
	 */
	if (bufpages == 0)
		bufpages = (physmem / 20) / CLSIZE;
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;
		if (nswbuf > 256)
			nswbuf = 256;
	}
#if !defined(UVM)
	valloc(swbuf, struct buf, nswbuf);
#endif
	valloc(buf, struct buf, nbuf);

	return v;
#undef valloc
}

/*
 * consinit
 * Initialize system console.
 */
void
consinit()
{
	static int initted;

	if (initted)
		return;
	initted = 1;
	cninit();
}

/*
 * Set set up registers on exec.
 */
void
setregs(p, pack, stack)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
{
	struct trapframe *tf = trapframe(p);
	struct ps_strings arginfo;

	bzero(tf, sizeof *tf);
	tf->fixreg[1] = -roundup(-stack + 8, 16);

	/*
	 * XXX Machine-independent code has already copied arguments and
	 * XXX environment to userland.  Get them back here.
	 */
	(void)copyin((char *)PS_STRINGS, &arginfo, sizeof(arginfo));

	/*
	 * Set up arguments for _start():
	 *	_start(argc, argv, envp, obj, cleanup, ps_strings);
	 *
	 * Notes:
	 *	- obj and cleanup are the auxilliary and termination
	 *	  vectors.  They are fixed up by ld.elf_so.
	 *	- ps_strings is a NetBSD extention, and will be
	 * 	  ignored by executables which are strictly
	 *	  compliant with the SVR4 ABI.
	 *
	 * XXX We have to set both regs and retval here due to different
	 * XXX calling convention in trap.c and init_main.c.
	 */
	tf->fixreg[3] = arginfo.ps_nargvstr;
	tf->fixreg[4] = (register_t)arginfo.ps_argvstr;
	tf->fixreg[5] = (register_t)arginfo.ps_envstr;
	tf->fixreg[6] = 0;			/* auxillary vector */
	tf->fixreg[7] = 0;			/* termination vector */
	tf->fixreg[8] = (register_t)PS_STRINGS;	/* NetBSD extension */

	tf->srr0 = pack->ep_entry;
	tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
	p->p_addr->u_pcb.pcb_flags = 0;
}

/*
 * Send a signal to process.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oldonstack;

	frame.sf_signum = sig;

	tf = trapframe(p);
	oldonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate stack space for signal handler.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK)
	    && !oldonstack
	    && (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)((caddr_t)psp->ps_sigstk.ss_sp
		                                  + psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)tf->fixreg[1];
	fp = (struct sigframe *)((int)(fp - 1) & ~0xf);

	frame.sf_code = code;

	/*
	 * Generate signal context for SYS_sigreturn.
	 */
	frame.sf_sc.sc_onstack = oldonstack;
	frame.sf_sc.sc_mask = mask;
	bcopy(tf, &frame.sf_sc.sc_frame, sizeof *tf);
	if (copyout(&frame, fp, sizeof frame) != 0)
		sigexit(p, SIGILL);

	tf->fixreg[1] = (int)fp;
	tf->lr = (int)catcher;
	tf->fixreg[3] = (int)sig;
	tf->fixreg[4] = (int)code;
	tf->fixreg[5] = (int)&frame.sf_sc;
	tf->srr0 = (int)(((char *)PS_STRINGS)
			 - (p->p_emul->e_esigcode - p->p_emul->e_sigcode));
}

/*
 * System call to cleanup state after a signal handler returns.
 */
int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext sc;
	struct trapframe *tf;
	int error;

	if (error = copyin(SCARG(uap, sigcntxp), &sc, sizeof sc))
		return error;
	tf = trapframe(p);
	if ((sc.sc_frame.srr1 & PSL_USERSTATIC) != (tf->srr1 & PSL_USERSTATIC))
		return EINVAL;
	bcopy(&sc.sc_frame, tf, sizeof *tf);
	if (sc.sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = sc.sc_mask & ~sigcantmask;
	return EJUSTRETURN;
}

/*
 * Machine dependent system variables.
 * None for now.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return ENOTDIR;
	switch (name[0]) {
	default:
		return EOPNOTSUPP;
	}
}

/*
 * Crash dump handling.
 */
u_long dumpmag = 0x8fca0101;		/* magic number */
int dumpsize = 0;			/* size of dump in pages */
long dumplo = -1;			/* blocks */

void
dumpsys()
{
	printf("dumpsys: TBD\n");
}

/*
 * Soft networking interrupts.
 */
void
softnet()
{
	extern volatile int netisr;
	int isr;

	isr = netisr;
	netisr = 0;

#ifdef	INET
#include "arp.h"
#if NARP > 0
	if (isr & (1 << NETISR_ARP))
		arpintr();
#endif
	if (isr & (1 << NETISR_IP))
		ipintr();
#endif
#ifdef	IMP
	if (isr & (1 << NETISR_IMP))
		impintr();
#endif
#ifdef	NS
	if (isr & (1 << NETISR_NS))
		nsintr();
#endif
#ifdef	ISO
	if (isr & (1 << NETISR_ISO))
		clnlintr();
#endif
#ifdef	CCITT
	if (isr & (1 << NETISR_CCITT))
		ccittintr();
#endif
#ifdef	NATM
	if (isr & (1 << NETISR_NATM))
		natmintr();
#endif
#ifdef	NETATALK
	if (isr & (1 << NETISR_ATALK))
		atintr();
#endif
#include "ppp.h"
#if NPPP > 0
	if (isr & (1 << NETISR_PPP))
		pppintr();
#endif
}

#include "zsc.h"
/*
 * Soft tty interrupts.
 */
void
softserial()
{
#if NZSC > 0
	zssoft();
#endif
}

/*
 * Stray interrupts.
 */
void
strayintr(irq)
	int irq;
{
	log(LOG_ERR, "stray interrupt %d\n", irq);
}

/*
 * Halt or reboot the machine after syncing/dumping according to howto.
 */
void
cpu_reboot(howto, what)
	int howto;
	char *what;
{
	static int syncing;
	static char str[256];
	char *ap = str, *ap1 = ap;

	boothowto = howto;
	if (!cold && !(howto & RB_NOSYNC) && !syncing) {
		syncing = 1;
		vfs_shutdown();		/* sync */
		resettodr();		/* set wall clock */
	}

	splhigh();

	if (!cold && (howto & RB_DUMP))
		dumpsys();

	doshutdownhooks();

	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
#if NADB > 0
		powermac_powerdown();
		printf("WARNING: powerdown failed!\n");
#endif
	}

	if (howto & RB_HALT) {
		printf("halted\n\n");
		ppc_exit();
	}

	printf("rebooting\n\n");
	if (what && *what) {
		if (strlen(what) > sizeof str - 5)
			printf("boot string too large, ignored\n");
		else {
			strcpy(str, what);
			ap1 = ap = str + strlen(str);
			*ap++ = ' ';
		}
	}
	*ap++ = '-';
	if (howto & RB_SINGLE)
		*ap++ = 's';
	if (howto & RB_KDB)
		*ap++ = 'd';
	*ap++ = 0;
	if (ap[-2] == '-')
		*ap1 = 0;
#if NADB > 0
	powermac_restart();	/* not return */
#endif
	ppc_boot(str);
}

/*
 * OpenFirmware callback routine
 */
void
callback(p)
	void *p;
{
	panic("callback");	/* for now			XXX */
}

void
lcsplx(ipl)
	int ipl;
{
	splx(ipl); 
}


/*
 * Convert kernel VA to physical address
 */
int
kvtop(addr)
	caddr_t addr;
{
	vaddr_t va;
	paddr_t pa;
	int off;
	extern char end[];

	if (addr < end)
		return (int)addr;

	va = trunc_page(addr);
	off = (int)addr - va;

	pa = pmap_extract(pmap_kernel(), va);
	if (pa == 0) {
		/*printf("kvtop: zero page frame (va=0x%x)\n", addr);*/
		return (int)addr;
	}

	return((int)pa + off);
}

/*
 * Allocate vm space and mapin the I/O address
 */
void *
mapiodev(pa, len)
	paddr_t pa;
	psize_t len;
{
	paddr_t faddr;
	vaddr_t taddr, va;
	int off;

	faddr = trunc_page(pa);
	off = pa - faddr;
	len = round_page(off + len);
#if defined(UVM)
	va = taddr = uvm_km_valloc(kernel_map, len);
#else
	va = taddr = kmem_alloc_pageable(kernel_map, len);
#endif

	if (va == 0)
		return NULL;

	for (; len > 0; len -= NBPG) {
		pmap_enter(pmap_kernel(), taddr, faddr,
			   VM_PROT_READ | VM_PROT_WRITE, 1);
		faddr += NBPG;
		taddr += NBPG;
	}
	return (void *)(va + off);
}

#include "ite.h"
#include "zstty.h"

int console_node = -1;

void
cninit()
{
	struct consdev *cp;
	int l, node;
	int chosen, stdout;
	char type[16];

	chosen = OF_finddevice("/chosen");
	if (chosen == -1)
		goto nocons;

	l = OF_getprop(chosen, "stdout", &stdout, sizeof(stdout));
	if (l != sizeof(stdout))
		goto nocons;
	node = OF_instance_to_package(stdout);

	bzero(type, sizeof(type));
	l = OF_getprop(node, "device_type", type, sizeof(type));
	if (l == -1 || l >= sizeof(type) - 1)
		goto nocons;

	console_node = node;

#if NITE > 0
	if (strcmp(type, "display") == 0) {
		extern struct consdev consdev_ite;

		cp = &consdev_ite;
		(*cp->cn_probe)(cp);
		(*cp->cn_init)(cp);
		cn_tab = cp;

		return;
	}
#endif

#if NZSTTY > 0
	if (strcmp(type, "serial") == 0) {
		extern struct consdev consdev_zs;

		cp = &consdev_zs;
		(*cp->cn_probe)(cp);
		(*cp->cn_init)(cp);
		cn_tab = cp;

		return;
	}
#endif

nocons:
	return;
}
