/*	$NetBSD: machdep.c,v 1.10 2002/02/28 01:56:59 uch Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Jason R. Thorpe of the Numerical Aerospace
 * Simulation Facility, NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

/*-
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include "opt_ddb.h"
#include "opt_kgdb.h"
#include "opt_syscall_debug.h"
#include "opt_memsize.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/syscallargs.h>

#ifdef KGDB
#include <sys/kgdb.h>
#endif

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_extern.h>
#endif

#include <dev/cons.h>

#include <uvm/uvm_extern.h>

#include <sys/sysctl.h>

#include <machine/cpu.h>
#include <machine/bootinfo.h>
#include <sh3/cpufunc.h>
#include <sh3/mmu.h>

#include <sys/termios.h>


/* the following is used externally (sysctl_hw) */
char machine[] = MACHINE;		/* cpu "architecture" */
char machine_arch[] = MACHINE_ARCH;	/* machine_arch = "sh3" */

#ifdef sh3_debug
int cpu_debug_mode = 1;
#else
int cpu_debug_mode = 0;
#endif

char bootinfo[BOOTINFO_MAXSIZE];

int physmem;
int dumpmem_low;
int dumpmem_high;
vaddr_t atdevbase;	/* location of start of iomem in virtual */
paddr_t msgbuf_paddr;
struct user *proc0paddr;

extern int boothowto;
extern paddr_t avail_start, avail_end;

#ifdef	SYSCALL_DEBUG
#define	SCDEBUG_ALL 0x0004
extern int	scdebug;
#endif

#define IOM_RAM_END	((paddr_t)IOM_RAM_BEGIN + IOM_RAM_SIZE - 1)

/*
 * Extent maps to manage I/O and ISA memory hole space.  Allocate
 * storage for 8 regions in each, initially.  Later, ioport_malloc_safe
 * will indicate that it's safe to use malloc() to dynamically allocate
 * region descriptors.
 *
 * N.B. At least two regions are _always_ allocated from the iomem
 * extent map; (0 -> ISA hole) and (end of ISA hole -> end of RAM).
 *
 * The extent maps are not static!  Machine-dependent ISA and EISA
 * routines need access to them for bus address space allocation.
 */
static	long iomem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(8) / sizeof(long)];
struct	extent *ioport_ex;
struct	extent *iomem_ex;
static	int ioport_malloc_safe;

void main(void) __attribute__((__noreturn__));
void dreamcast_startup(void) __attribute__((__noreturn__));
void setup_bootinfo __P((void));
void dumpsys __P((void));
void consinit __P((void));

/*
 * Machine-dependent startup code
 *
 * This is called from main() in kern/main.c.
 */
void
cpu_startup()
{

	sh3_startup();
	printf("%s\n", cpu_model);

	/* Safe for i/o port allocation to use malloc now. */
	ioport_malloc_safe = 1;

#ifdef SYSCALL_DEBUG
	scdebug |= SCDEBUG_ALL;
#endif

#ifdef FORCE_RB_SINGLE
	boothowto |= RB_SINGLE;
#endif
}

#define CPUDEBUG

/*
 * machine dependent system variables.
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
	dev_t consdev;
	struct btinfo_bootpath *bibp;
	struct trapframe *tf;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));

	case CPU_NKPDE:
		return (sysctl_rdint(oldp, oldlenp, newp, nkpde));

	case CPU_BOOTED_KERNEL:
	        bibp = lookup_bootinfo(BTINFO_BOOTPATH);
	        if (!bibp)
			return (ENOENT); /* ??? */
		return (sysctl_rdstring(oldp, oldlenp, newp, bibp->bootpath));

	case CPU_SETPRIVPROC:
		if (newp == NULL)
			return (0);

		/* set current process to priviledged process */
		tf = p->p_md.md_regs;
		tf->tf_ssr |= PSL_MD;
		return (0);

	case CPU_DEBUGMODE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
				   &cpu_debug_mode));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

int waittime = -1;
struct pcb dumppcb;

void
cpu_reboot(howto, bootstr)
	int howto;
	char *bootstr;
{

	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		/* resettodr(); */
	}

	/* Disable interrupts. */
	splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();

haltsys:
	doshutdownhooks();

	if (howto & RB_HALT) {
		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
	cpu_reset();
	for(;;)
		;
	/*NOTREACHED*/
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
cpu_dumpconf()
{
#ifdef	TODO
	int nblks;	/* size of dump area */
	int maj;

	if (dumpdev == NODEV)
		return;
	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize)(dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = btoc(IOM_END + ctob(dumpmem_high));

	/* Always skip the first CLBYTES, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
#endif
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP  NBPG	/* must be a multiple of pagesize XXX small */
static vaddr_t dumpspace;

vaddr_t
reserve_dumppages(p)
	vaddr_t p;
{

	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

void
dumpsys()
{
#ifdef	TODO
	unsigned bytes, i, n;
	int maddr, psize;
	daddr_t blkno;
	int (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int error;

	/* Save registers. */
	savectx(&dumppcb);

	msgbufmapped = 0;	/* don't record dump msgs in msgbuf */
	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		cpu_dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

#if 0	/* XXX this doesn't work.  grr. */
        /* toss any characters present prior to dump */
	while (sget() != NULL); /* syscons and pccons differ */
#endif

	bytes = ctob(dumpmem_high) + IOM_END;
	maddr = 0;
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;
	for (i = 0; i < bytes; i += n) {
		/*
		 * Avoid dumping the ISA memory hole, and areas that
		 * BIOS claims aren't in low memory.
		 */
		if (i >= ctob(dumpmem_low) && i < IOM_END) {
			n = IOM_END - i;
			maddr += n;
			blkno += btodb(n);
			continue;
		}

		/* Print out how many MBs we to go. */
		n = bytes - i;
		if (n && (n % (1024*1024)) == 0)
			printf("%d ", n / (1024 * 1024));

		/* Limit size for next transfer. */
		if (n > BYTES_PER_DUMP)
			n =  BYTES_PER_DUMP;

		(void) pmap_map(dumpspace, maddr, maddr + n, VM_PROT_READ);
		error = (*dump)(dumpdev, blkno, (caddr_t)dumpspace, n);
		if (error)
			break;
		maddr += n;
		blkno += btodb(n);			/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
		/* operator aborting dump? */
		if (sget() != NULL) {
			error = EINTR;
			break;
		}
#endif
	}

	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
	printf("\n\n");
	delay(5000000);		/* 5 seconds */
#endif	/* TODO */
}

/*
 * Initialize segments and descriptor tables
 */
#define VADDRSTART	VM_MIN_KERNEL_ADDRESS

extern int nkpde;
extern char start[], _etext[], _edata[], _end[];

void
dreamcast_startup()
{
	paddr_t avail;
	pd_entry_t *pagedir;
	pt_entry_t *pagetab, pte;
	u_int sp;
	int x;
	char *p;

	avail = sh3_round_page(_end);

	/* XXX nkpde = kernel page dir area (IOM_RAM_SIZE*2 Mbyte (why?)) */
	nkpde = IOM_RAM_SIZE >> (PDSHIFT - 1);

	/*
	 * clear .bss, .common area, page dir area,
	 *	process0 stack, page table area
	 */
	p = (char *)avail + (1 + UPAGES) * NBPG + NBPG * (1 + nkpde); /* XXX */
	memset(_edata, 0, p - _edata);

	sh_cpu_init(CPU_ARCH_SH4, CPU_PRODUCT_7750);
/*
 *                          edata  end
 *	+-------------+------+-----+----------+-------------+------------+
 *	| kernel text | data | bss | Page Dir | Proc0 Stack | Page Table |
 *	+-------------+------+-----+----------+-------------+------------+
 *                                     NBPG       USPACE    (1+nkpde)*NBPG
 *                                                (= 4*NBPG)
 *	Build initial page tables
 */
	pagedir = (void *)avail;
	pagetab = (void *)(avail + SYSMAP);

	/*
	 * Construct a page table directory
	 * In SH3 H/W does not support PTD,
	 * these structures are used by S/W.
	 */
	pte = (pt_entry_t)pagetab;
	pte |= PG_KW | PG_V | PG_4K | PG_M | PG_N;
	pagedir[KERNTEXTOFF >> PDSHIFT] = pte;

	/* make pde for 0xd0000000, 0xd0400000, 0xd0800000,0xd0c00000,
		0xd1000000, 0xd1400000, 0xd1800000, 0xd1c00000 */
	pte += NBPG;
	for (x = 0; x < nkpde; x++) {
		pagedir[(VADDRSTART >> PDSHIFT) + x] = pte;
		pte += NBPG;
	}

	/* Install a PDE recursively mapping page directory as a page table! */
	pte = (u_int)pagedir;
	pte |= PG_V | PG_4K | PG_KW | PG_M | PG_N;
	pagedir[PDSLOT_PTE] = pte;

	/* set PageDirReg */
	SH_MMU_TTB_WRITE((u_int32_t)pagedir);

	/*
	 * Activate MMU
	 */
	sh_mmu_start();

	/*
	 * Now here is virtual address
	 */

	/* Set proc0paddr */
	proc0paddr = (void *)(avail + NBPG);

	/* Set pcb->PageDirReg of proc0 */
	proc0paddr->u_pcb.pageDirReg = (int)pagedir;

	/* avail_start is first available physical memory address */
	avail_start = avail + NBPG + USPACE + NBPG + NBPG * nkpde;

	/* atdevbase is first available logical memory address */
	atdevbase = VADDRSTART;

	proc0.p_addr = proc0paddr; /* page dir address */

	/* XXX: PMAP_NEW requires valid curpcb.   also init'd in cpu_startup */
	curpcb = &proc0.p_addr->u_pcb;

	/*
	 * Initialize the I/O port and I/O mem extent maps.
	 * Note: we don't have to check the return value since
	 * creation of a fixed extent map will never fail (since
	 * descriptor storage has already been allocated).
	 *
	 * N.B. The iomem extent manages _all_ physical addresses
	 * on the machine.  When the amount of RAM is found, the two
	 * extents of RAM are allocated from the map (0 -> ISA hole
	 * and end of ISA hole -> end of RAM).
	 */
	iomem_ex = extent_create("iomem", 0x0, 0xffffffff, M_DEVBUF,
	    (caddr_t)iomem_ex_storage, sizeof(iomem_ex_storage),
	    EX_NOCOALESCE|EX_NOWAIT);

#if 1
	consinit();	/* XXX SHOULD NOT BE DONE HERE */
#endif

	splraise(-1);
	_cpu_exception_resume(0);	/* SR.BL = 0 */

	avail_end = sh3_trunc_page(IOM_RAM_END + 1);

	printf("initSH3\r\n");

	/*
	 * Calculate check sum
	 */
    {
	u_short *p, sum;
	int size;

	size = _etext - start;
	p = (u_short *)start;
	sum = 0;
	size >>= 1;
	while (size--)
		sum += *p++;
	printf("Check Sum = 0x%x\r\n", sum);
    }
	/*
	 * Allocate the physical addresses used by RAM from the iomem
	 * extent map.  This is done before the addresses are
	 * page rounded just to make sure we get them all.
	 */
	if (extent_alloc_region(iomem_ex, IOM_RAM_BEGIN,
				(IOM_RAM_END-IOM_RAM_BEGIN) + 1,
				EX_NOWAIT)) {
		/* XXX What should we do? */
		printf("WARNING: CAN'T ALLOCATE RAM MEMORY FROM IOMEM EXTENT MAP!\n");
	}

	/* number of pages of physmem addr space */
	physmem = btoc(IOM_RAM_END - IOM_RAM_BEGIN +1);
#ifdef	TODO
	dumpmem = physmem;
#endif

	/*
	 * Initialize for pmap_free_pages and pmap_next_page.
	 * These guys should be page-aligned.
	 */
	if (physmem < btoc(2 * 1024 * 1024)) {
		printf("warning: too little memory available; "
		       "have %d bytes, want %d bytes\n"
		       "running in degraded mode\n"
		       "press a key to confirm\n\n",
		       ctob(physmem), 2*1024*1024);
		cngetc();
	}

	/* Call pmap initialization to make new kernel address space */
	pmap_bootstrap(atdevbase);

	/*
	 * Initialize error message buffer (at end of core).
	 */
	initmsgbuf((caddr_t)msgbuf_paddr, round_page(MSGBUFSIZE));

	/*
	 * set boot device information
	 */
	setup_bootinfo();

	/* setup proc0 stack */
	sp = avail + NBPG + USPACE - 16 - sizeof(struct trapframe);

	/* jump to main */
	__asm__ __volatile__(
		"jmp	@%0;"
		"mov	%1, sp" :: "r"(main), "r"(sp));
	/* NOTREACHED */
	while (1)
		;
}

void
setup_bootinfo(void)
{
	struct btinfo_bootdisk *help;

	*(int *)bootinfo = 1;
	help = (struct btinfo_bootdisk *)(bootinfo + sizeof(int));
	help->biosdev = 0;
	help->partition = 0;
	((struct btinfo_common *)help)->len = sizeof(struct btinfo_bootdisk);
	((struct btinfo_common *)help)->type = BTINFO_BOOTDISK;
}

void *
lookup_bootinfo(type)
	int type;
{
	struct btinfo_common *help;
	int n = *(int*)bootinfo;
	help = (struct btinfo_common *)(bootinfo + sizeof(int));
	while (n--) {
		if (help->type == type)
			return (help);
		help = (struct btinfo_common *)((char*)help + help->len);
	}
	return (0);
}


/*
 * consinit:
 * initialize the system console.
 * XXX - shouldn't deal with this initted thing, but then,
 * it shouldn't be called from init386 either.
 */
void
consinit()
{
	static int initted;

	if (initted)
		return;
	initted = 1;

	cninit();

#ifdef DDB
	ddb_init(0, NULL, NULL);	/* XXX XXX XXX */
#endif
}

void
cpu_reset()
{

	_cpu_exception_suspend();

	goto *(u_int32_t *)0xa0000000;
	for (;;)
		;
}
