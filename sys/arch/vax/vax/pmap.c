/*	$NetBSD: pmap.c,v 1.52 1998/07/18 20:35:14 ragge Exp $	   */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>

#ifdef PMAPDEBUG
#include <dev/cons.h>
#endif

#if defined(UVM)
#include <uvm/uvm.h>
#endif

#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/mtpr.h>
#include <machine/macros.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/scb.h>

#include "opt_pmap_new.h"

/* QDSS console mapping hack */
#include "qd.h"
#if NQD > 0
/* Pointer to virtual memory for for mapping QDSS */
void *qvmem[NQD];
/* Pointer to page tables for this virtual memory */
struct pte *QVmap[NQD];
extern void *qd_ubaio;
#endif

#define ISTACK_SIZE (4 * CLBYTES)

/* 
 * This code uses bitfield operators for most page table entries.  
 */
#define PROTSHIFT	27
#define PROT_KW		(PG_KW >> PROTSHIFT)
#define PROT_KR		(PG_KR >> PROTSHIFT) 
#define PROT_RW		(PG_RW >> PROTSHIFT)
#define PROT_RO		(PG_RO >> PROTSHIFT)
#define PROT_URKW	(PG_URKW >> PROTSHIFT)

struct pmap kernel_pmap_store;

struct	pte *Sysmap;		/* System page table */
struct	pv_entry *pv_table;	/* array of entries, one per LOGICAL page */
void	*scratch;

vm_offset_t ptemapstart, ptemapend;
vm_map_t pte_map;
#if defined(UVM)
struct	vm_map	pte_map_store;
#endif

extern	caddr_t msgbufaddr;

#ifdef PMAPDEBUG
int	startpmapdebug = 0;
#endif

static inline void rensa __P((int, struct pte *));

vm_offset_t   avail_start, avail_end;
vm_offset_t   virtual_avail, virtual_end; /* Available virtual memory	*/

void pmap_pinit __P((pmap_t));
void pmap_release __P((pmap_t));

/*
 * pmap_bootstrap().
 * Called as part of vm bootstrap, allocates internal pmap structures.
 * Assumes that nothing is mapped, and that kernel stack is located
 * immediately after end.
 */
void
pmap_bootstrap()
{
	unsigned int sysptsize, istack, i;
	extern	unsigned int etext, proc0paddr;
	struct pcb *pcb = (struct pcb *)proc0paddr;
	pmap_t pmap = pmap_kernel();


	/*
	 * Machines older than MicroVAX II have their boot blocks
	 * loaded directly or the boot program loaded from console
	 * media, so we need to figure out their memory size.
	 * This is not easily done on MicroVAXen, so we get it from
	 * VMB instead.
	 */
	if (avail_end == 0)
		while (badaddr((caddr_t)avail_end, 4) == 0)
			avail_end += NBPG * 128;

	avail_end = TRUNC_PAGE(avail_end); /* be sure */

	/*
	 * Calculation of the System Page Table is somewhat a pain,
	 * because it must be in contiguous physical memory and all
	 * size calculations must be done now.
	 * Remember: sysptsize is in PTEs and nothing else!
	 */

	/* Kernel alloc area */
	sysptsize = (((0x100000 * maxproc) >> PGSHIFT) / 4);
	/* reverse mapping struct */
	sysptsize += (avail_end >> PGSHIFT) * 2;
	/* User Page table area. This may grow big */
#define USRPTSIZE ((MAXTSIZ + MAXDSIZ + MAXSSIZ + MMAPSPACE) / NBPG)
	sysptsize += ((USRPTSIZE * 4) / NBPG) * maxproc;
	/* Kernel stacks per process */
	sysptsize += UPAGES * maxproc;

	/*
	 * Virtual_* and avail_* is used for mapping of system page table.
	 * The need for kernel virtual memory is linear dependent of the
	 * amount of physical memory also, therefore sysptsize is 
	 * a variable here that is changed dependent of the physical
	 * memory size.
	 */
	virtual_avail = avail_end + KERNBASE;
	virtual_end = KERNBASE + sysptsize * NBPG;
	blkclr(Sysmap, sysptsize * 4); /* clear SPT before using it */

	/*
	 * The first part of Kernel Virtual memory is the physical
	 * memory mapped in. This makes some mm routines both simpler
	 * and faster, but takes ~0.75% more memory.
	 */
	pmap_map(KERNBASE, 0, avail_end, VM_PROT_READ|VM_PROT_WRITE);
	/*
	 * Kernel code is always readable for user, it must be because
	 * of the emulation code that is somewhere in there.
	 * And it doesn't hurt, /netbsd is also public readable.
	 * There are also a couple of other things that must be in
	 * physical memory and that isn't managed by the vm system.
	 */
	for (i = 0; i < ((unsigned)&etext - KERNBASE) >> PGSHIFT; i++)
		Sysmap[i].pg_prot = PROT_URKW;

	/* Map System Page Table and zero it,  Sysmap already set. */
	mtpr((unsigned)Sysmap - KERNBASE, PR_SBR);

	/* Map Interrupt stack and set red zone */
	istack = (unsigned)Sysmap + ROUND_PAGE(sysptsize * 4);
	mtpr(istack + ISTACK_SIZE, PR_ISP);
	kvtopte(istack)->pg_v = 0;

	/* Some scratch pages */
	scratch = (void *)istack + ISTACK_SIZE;

	/* Physical-to-virtual translation table */
	(unsigned)pv_table = scratch + 4 * NBPG;

	avail_start = (unsigned)pv_table + (ROUND_PAGE(avail_end >> CLSHIFT)) *
	    sizeof(struct pv_entry) - KERNBASE;

	/* Kernel message buffer */
	avail_end -= MSGBUFSIZE;
	msgbufaddr = (void *)(avail_end + KERNBASE);

	/* zero all mapped physical memory from Sysmap to here */
	blkclr((void *)istack, (avail_start + KERNBASE) - istack);

	/* Set logical page size */
#if defined(UVM)
	uvmexp.pagesize = CLBYTES;
	uvm_setpagesize();
#else
	PAGE_SIZE = CLBYTES;
	vm_set_page_size();
#endif

        /* QDSS console mapping hack */
#if NQD > 0
        /*
         * This allocates some kernel virtual address space.  qdcninit
         * maps things here
         */
        MAPVIRT(qvmem[0], 64 * 1024 * NQD / NBPG);
        MAPVIRT(qd_ubaio, 16);
#endif

	/*
	 * We move SCB here from physical address 0 to an address
	 * somewhere else, so that we can dynamically allocate
	 * space for interrupt vectors and other machine-specific
	 * things. We move it here, but the rest of the allocation
	 * is done in a cpu-specific routine.
	 * avail_start is modified in the cpu-specific routine.
	 */
	scb = (struct scb *)(avail_start + KERNBASE);
	bcopy(0, (void *)avail_start, NBPG >> 1);
	mtpr(avail_start, PR_SCBB);
	bzero(0, NBPG >> 1);
	(*dep_call->cpu_steal_pages)();
	avail_start = ROUND_PAGE(avail_start);
	virtual_avail = ROUND_PAGE(virtual_avail);

#ifdef PMAPDEBUG
	cninit();
	printf("Sysmap %p, istack %x, scratch %p\n",Sysmap,istack,scratch);
	printf("etext %p\n", &etext);
	printf("SYSPTSIZE %x\n",sysptsize);
	printf("pv_table %p, \n", pv_table);
	printf("avail_start %lx, avail_end %lx\n",avail_start,avail_end);
	printf("virtual_avail %lx,virtual_end %lx\n",virtual_avail,virtual_end);
/*	  printf("faultdebug %x, startsysc %x\n",&faultdebug, &startsysc);*/
	printf("startpmapdebug %p\n",&startpmapdebug);
#endif


	/* Init kernel pmap */
	pmap->pm_p1br = (void *)0x80000000;
	pmap->pm_p0br = (void *)0x80000000;
	pmap->pm_p1lr = 0x200000;
	pmap->pm_p0lr = AST_PCB;

	pmap->ref_count = 1;

	/* Activate the kernel pmap. */
	mtpr(pcb->P1BR = pmap->pm_p1br, PR_P1BR);
	mtpr(pcb->P0BR = pmap->pm_p0br, PR_P0BR);
	mtpr(pcb->P1LR = pmap->pm_p1lr, PR_P1LR);
	mtpr(pcb->P0LR = pmap->pm_p0lr, PR_P0LR);

	/*
	 * Now everything should be complete, start virtual memory.
	 */
#if defined(UVM)
	uvm_page_physload(avail_start >> CLSHIFT, avail_end >> CLSHIFT,
	    avail_start >> CLSHIFT, avail_end >> CLSHIFT,
	    VM_FREELIST_DEFAULT);
#endif
	mtpr(sysptsize, PR_SLR);
	mtpr(1, PR_MAPEN);
}

#if defined(UVM)
/*
 * How much virtual space does this kernel have?
 * (After mapping kernel text, data, etc.)
 */
void
pmap_virtual_space(v_start, v_end)
	vm_offset_t *v_start;
	vm_offset_t *v_end;
{
	*v_start = virtual_avail;
	*v_end	 = virtual_end;
}
#endif

/*
 * pmap_init() is called as part of vm init after memory management
 * is enabled. It is meant to do machine-specific allocations.
 * Here we allocate virtual memory for user page tables.
 */
void 
#if defined(UVM)
pmap_init() 
#else
pmap_init(start, end) 
	vm_offset_t start, end;
#endif
{
	/* reserve place on SPT for UPT */
#if !defined(UVM)
	pte_map = kmem_suballoc(kernel_map, &ptemapstart, &ptemapend, 
	    USRPTSIZE * 4 * maxproc, TRUE);
#else
	pte_map = uvm_km_suballoc(kernel_map, &ptemapstart, &ptemapend, 
	    USRPTSIZE * 4 * maxproc, TRUE, FALSE, &pte_map_store);
#endif
}


/*
 * pmap_create() creates a pmap for a new task.
 * If not already allocated, malloc space for one.
 */
#ifdef PMAP_NEW
struct pmap * 
pmap_create()
{
	struct pmap *pmap;

	MALLOC(pmap, struct pmap *, sizeof(*pmap), M_VMPMAP, M_WAITOK);
	pmap_pinit(pmap);
	return(pmap);
}
#else
pmap_t 
pmap_create(phys_size)
	vm_size_t phys_size;
{
	pmap_t	 pmap;

#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_create: phys_size %x\n",(int)phys_size);
#endif
	if (phys_size)
		return NULL;

	pmap = (pmap_t) malloc(sizeof(struct pmap), M_VMPMAP, M_WAITOK);
	bzero(pmap, sizeof(struct pmap));
	pmap_pinit(pmap); 
	pmap->pm_stack = USRSTACK;
	return (pmap);
}
#endif

/*
 * Initialize a preallocated an zeroed pmap structure,
 */
void
pmap_pinit(pmap)
	pmap_t pmap;
{
	int bytesiz;

	/*
	 * Allocate PTEs and stash them away in the pmap.
	 * XXX Ok to use kmem_alloc_wait() here?
	 */
	bytesiz = USRPTSIZE * sizeof(struct pte);
#if defined(UVM)
	pmap->pm_p0br = (void *)uvm_km_valloc_wait(pte_map, bytesiz);
#else
	pmap->pm_p0br = (void *)kmem_alloc_wait(pte_map, bytesiz);
#endif
	pmap->pm_p0lr = btoc(MAXTSIZ + MAXDSIZ + MMAPSPACE) | AST_PCB;
	pmap->pm_p1br = (void *)pmap->pm_p0br + bytesiz - 0x800000;
	pmap->pm_p1lr = (0x200000 - btoc(MAXSSIZ));

#ifdef PMAPDEBUG
if (startpmapdebug)
	printf("pmap_pinit(%p): p0br=%p p0lr=0x%lx p1br=%p p1lr=0x%lx\n",
	pmap, pmap->pm_p0br, pmap->pm_p0lr, pmap->pm_p1br, pmap->pm_p1lr);
#endif

	pmap->ref_count = 1;
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap)
	struct pmap *pmap;
{
#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_release: pmap %p\n",pmap);
#endif

	if (pmap->pm_p0br)
#if defined(UVM)
		uvm_km_free_wakeup(pte_map, (vm_offset_t)pmap->pm_p0br, 
		    USRPTSIZE * sizeof(struct pte));
#else
		kmem_free_wakeup(pte_map, (vm_offset_t)pmap->pm_p0br, 
		    USRPTSIZE * sizeof(struct pte));
#endif
}


/*
 * pmap_destroy(pmap): Remove a reference from the pmap. 
 * If the pmap is NULL then just return else decrese pm_count.
 * If this was the last reference we call's pmap_relaese to release this pmap.
 * OBS! remember to set pm_lock
 */

void
pmap_destroy(pmap)
	pmap_t pmap;
{
	int count;
  
#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_destroy: pmap %p\n",pmap);
#endif
	if (pmap == NULL)
		return;

	simple_lock(&pmap->pm_lock);
	count = --pmap->ref_count;
	simple_unlock(&pmap->pm_lock);
  
	if (count == 0) {
		pmap_release(pmap);
		free((caddr_t)pmap, M_VMPMAP);
	}
}

/*
 * Rensa is a help routine to remove a pv_entry from the pv list.
 * Arguments are physical clustering page and page table entry pointer.
 */
static inline void
rensa(clp, ptp)
	int clp;
	struct pte *ptp;
{
	struct	pv_entry *pf, *pv = pv_table + clp;
	int	s;

	s = splimp();
	if (pv->pv_pte == ptp) {
		pv->pv_pte = 0;
		splx(s);
		return;
	}
	for (; pv->pv_next; pv = pv->pv_next) {
		if (pv->pv_next->pv_pte == ptp) {
			pf = pv->pv_next;
			pv->pv_next = pv->pv_next->pv_next;
			FREE(pf, M_VMPVENT);
			splx(s);
			return;
		}
	}
	panic("rensa");
}

#ifdef PMAP_NEW
/*
 * New (real nice!) function that allocates memory in kernel space
 * without tracking it in the MD code.
 */
void
pmap_kenter_pa(va, pa, prot)
	vm_offset_t va, pa;
	vm_prot_t prot;
{
#if 0
	pmap_enter(pmap_kernel(), va, pa, prot, 0);
#endif
	int *ptp;

	ptp = (int *)kvtopte(va);
#ifdef PMAPDEBUG
if(startpmapdebug)
printf("pmap_kenter_pa: va: %lx, pa %lx, prot %x ptp %p\n", va, pa, prot, ptp);
#endif
	ptp[0] = PG_V | ((prot & VM_PROT_WRITE)? PG_KW : PG_KR) |
	    PG_PFNUM(pa) | PG_SREF;
	ptp[1] = ptp[0] + 1;
	mtpr(va, PR_TBIS);
	mtpr(va + NBPG, PR_TBIS);
}

void
pmap_kremove(va, len)
	vm_offset_t va;
	vm_size_t len;
{
	struct pte *pte;
	int i;

#ifdef PMAPDEBUG
if(startpmapdebug)
printf("pmap_kremove: va: %lx, len %lx, ptp %p\n", va, len, kvtopte(va));
#endif

	/*
	 * Unfortunately we must check if any page may be on the pv list. 
	 */
	pte = kvtopte(va);
	len >>= CLSHIFT;

	for (i = 0; i < len; i++) {
		if (pte->pg_pfn == 0)
			continue;
		if (pte->pg_sref == 0)
			rensa(pte->pg_pfn >> CLSIZELOG2, pte);
		*(long long *)pte = 0;
#ifdef notdef
		bzero(pte, CLSIZE * sizeof(struct pte));
#endif
		pte += CLSIZE;
	}
	mtpr(0, PR_TBIA);
}

void
pmap_kenter_pgs(va, pgs, npgs)
	vm_offset_t va;
	struct vm_page **pgs;
	int npgs;
{
	int i;
	int *ptp;

#ifdef PMAPDEBUG
if(startpmapdebug)
printf("pmap_kenter_pgs: va: %lx, pgs %p, npgs %x\n", va, pgs, npgs);
#endif

	/*
	 * May this routine affect page tables? 
	 * We assume that, and uses TBIA.
	 */
	ptp = (int *)kvtopte(va);
	for (i = 0 ; i < npgs ; i++) {
		ptp[0] = PG_V | PG_KW |
		    PG_PFNUM(VM_PAGE_TO_PHYS(pgs[i])) | PG_SREF;
		ptp[1] = ptp[0] + 1;
		ptp += 2;
	}
	mtpr(0, PR_TBIA);
}
#endif

void 
pmap_enter(pmap, v, p, prot, wired)
	register pmap_t pmap;
	vm_offset_t	v;
	vm_offset_t	p;
	vm_prot_t	prot;
	boolean_t	wired;
{
	struct	pv_entry *pv, *tmp;
	int	i, s, nypte, *patch;


#ifdef PMAPDEBUG
if(startpmapdebug)
printf("pmap_enter: pmap: %p,virt %lx, phys %lx, prot %x w %x\n",
	pmap,v,p,prot, wired);
#endif
	if (pmap == 0)
		return;

	if (v < 0x40000000) {
		patch = (int *)pmap->pm_p0br;
		i = (v >> PGSHIFT);
		if (i >= (pmap->pm_p0lr & ~AST_MASK))
			panic("P0 too small in pmap_enter");
		patch = (int *)pmap->pm_p0br;
		nypte = PG_V|(p>>PGSHIFT)|(prot&VM_PROT_WRITE?PG_RW:PG_RO);
	} else if (v & KERNBASE) {
		patch = (int *)Sysmap;
		i = (v - KERNBASE) >> PGSHIFT;
		nypte = PG_V|(p>>PGSHIFT)|(prot&VM_PROT_WRITE?PG_KW:PG_KR);
	} else {
		patch = (int *)pmap->pm_p1br;
		i = (v - 0x40000000) >> PGSHIFT;
		if (i < pmap->pm_p1lr)
			panic("pmap_enter: must expand P1");
		if (v < pmap->pm_stack)
			pmap->pm_stack = v;
		nypte = PG_V|(p>>PGSHIFT)|(prot&VM_PROT_WRITE?PG_RW:PG_RO);
	}

	if ((patch[i] & ~PG_M) == nypte)
		return;

	if ((patch[i] & PG_FRAME) &&
	    ((patch[i] & PG_FRAME) != (nypte & PG_FRAME)))
#if defined(UVM)
#ifdef PMAP_NEW
		pmap_page_protect(PHYS_TO_VM_PAGE((patch[i] & PG_FRAME)
		    << PGSHIFT), 0);
#else
		pmap_page_protect((patch[i] & PG_FRAME) << PGSHIFT, 0);
#endif
#else
		panic("pmap_enter: mapping onto old map");
#endif

	/*
	 * If we map in a new physical page we also must add it
	 * in the pv_table.
	 */
	if ((patch[i] & PG_FRAME) != (nypte & PG_FRAME)) {
		pv = pv_table + (p >> CLSHIFT);
		s = splimp();
		if (pv->pv_pte == 0) {
			pv->pv_pte = (struct pte *)&patch[i];
		} else {
			MALLOC(tmp, struct pv_entry *, sizeof(struct pv_entry),
			    M_VMPVENT, M_NOWAIT);
#ifdef DIAGNOSTIC
			if (tmp == 0) 
				panic("pmap_enter: cannot alloc pv_entry");
#endif
			tmp->pv_pte = (struct pte *)&patch[i];
			tmp->pv_next = pv->pv_next;
			pv->pv_next = tmp;
		}
		splx(s);
	}

	patch[i] = nypte;
	patch[i+1] = nypte+1;

	if (v > KERNBASE && v < ptemapend) { /* If we're playing with PTEs */
		mtpr(0, PR_TBIA);
	} else {
		mtpr(v, PR_TBIS);
		mtpr(v + NBPG, PR_TBIS);
	}
}

void *
pmap_bootstrap_alloc(size)
	int size;
{
	void *mem;

#ifdef PMAPDEBUG
if(startpmapdebug)
printf("pmap_bootstrap_alloc: size 0x %x\n",size);
#endif
	size = round_page(size);
	mem = (void *)avail_start + KERNBASE;
	avail_start += size;
	blkclr(mem, size);
	return (mem);
}

vm_offset_t
pmap_map(virtuell, pstart, pend, prot)
	vm_offset_t virtuell, pstart, pend;
	int prot;
{
	vm_offset_t count;
	int *pentry;

#ifdef PMAPDEBUG
if(startpmapdebug)
	printf("pmap_map: virt %lx, pstart %lx, pend %lx, Sysmap %p\n",
	    virtuell, pstart, pend, Sysmap);
#endif

	pstart=(uint)pstart &0x7fffffff;
	pend=(uint)pend &0x7fffffff;
	virtuell=(uint)virtuell &0x7fffffff;
	(uint)pentry= (((uint)(virtuell)>>PGSHIFT)*4)+(uint)Sysmap;
	for(count=pstart;count<pend;count+=NBPG){
		*pentry++ = (count>>PGSHIFT)|PG_V|
		    (prot & VM_PROT_WRITE ? PG_KW : PG_KR);
	}
	mtpr(0,PR_TBIA);
	return(virtuell+(count-pstart)+0x80000000);
}

#define POFF(x) (((unsigned)(x) & 0x3fffffff) >> PGSHIFT)

vm_offset_t 
pmap_extract(pmap, va)
	pmap_t pmap;
	vm_offset_t va;
{
	struct pte *pte;

#ifdef PMAPDEBUG
if(startpmapdebug)printf("pmap_extract: pmap %p, va %lx\n",pmap, va);
#endif
#ifdef DIAGNOSTIC
	if (va & PGOFSET)
		printf("Warning, pmap_extract va not aligned\n");
#endif

	if (va < 0x40000000) {
		pte = pmap->pm_p0br;
		if ((va >> PGSHIFT) > (pmap->pm_p0lr & ~AST_MASK))
			return 0;
	} else if (va & KERNBASE) {
		pte = Sysmap;
	} else {
		pte = pmap->pm_p1br;
		if (POFF(va) < pmap->pm_p1lr)
			return 0;
	}

	return (pte[POFF(va)].pg_pfn << PGSHIFT);
}

/*
 * Sets protection for a given region to prot. If prot == none then
 * unmap region. pmap_remove is implemented as pmap_protect with
 * protection none.
 */
void
pmap_protect(pmap, start, end, prot)
	pmap_t pmap;
	vm_offset_t start;
	vm_offset_t	end;
	vm_prot_t	prot;
{
	struct	pte *pt, *pts, *ptd;
	int	pr;

#ifdef PMAPDEBUG
if(startpmapdebug) printf("pmap_protect: pmap %p, start %lx, end %lx, prot %x\n",
	pmap, start, end,prot);
#endif

	if (pmap == 0)
		return;

	if (start & KERNBASE) { /* System space */
		pt = Sysmap;
#ifdef DIAGNOSTIC
		if (((end & 0x3fffffff) >> PGSHIFT) > mfpr(PR_SLR))
			panic("pmap_protect: outside SLR: %lx", end);
#endif
		start &= ~KERNBASE;
		end &= ~KERNBASE;
		pr = (prot & VM_PROT_WRITE ? PROT_KW : PROT_KR);
	} else {
		if (start & 0x40000000) { /* P1 space */
			if (end <= pmap->pm_stack)
				return;
			if (start < pmap->pm_stack)
				start = pmap->pm_stack;
			pt = pmap->pm_p1br;
#ifdef DIAGNOSTIC
			if (((start & 0x3fffffff) >> PGSHIFT) < pmap->pm_p1lr)
				panic("pmap_protect: outside P1LR");
#endif
			start &= 0x3fffffff;
			end = (end == KERNBASE ? end >> 1 : end & 0x3fffffff);
		} else { /* P0 space */
			pt = pmap->pm_p0br;
#ifdef DIAGNOSTIC
			if ((end >> PGSHIFT) > (pmap->pm_p0lr & ~AST_MASK))
				panic("pmap_protect: outside P0LR");
#endif
		}
		pr = (prot & VM_PROT_WRITE ? PROT_RW : PROT_RO);
	}
	pts = &pt[start >> PGSHIFT];
	ptd = &pt[end >> PGSHIFT];
#ifdef DEBUG
	if (((int)pts - (int)pt) & 7)
		panic("pmap_remove: pts not even");
	if (((int)ptd - (int)pt) & 7)
		panic("pmap_remove: ptd not even");
#endif

	if (prot == VM_PROT_NONE) {
		while (pts < ptd) {
			if (*(int *)pts) {
#ifdef PMAP_NEW
				if ((*(int *)pts & PG_SREF) == 0)
#endif
					rensa(pts->pg_pfn >> CLSIZELOG2, pts);
				bzero(pts, sizeof(struct pte) * CLSIZE);
			}
			pts += CLSIZE;
		}
	} else {
		while (pts < ptd) {
			if (*(int *)pts) {
				pts[0].pg_prot = pr;
				pts[1].pg_prot = pr;
			}
			pts += CLSIZE;
		}
	}
	mtpr(0,PR_TBIA);
}

/*
 * Checks if page is referenced; returns true or false depending on result.
 */
#ifdef PMAP_NEW
boolean_t
pmap_is_referenced(pg)
	struct vm_page *pg;
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(pg);
#else
boolean_t
pmap_is_referenced(pa)
	vm_offset_t	pa;
{
#endif
	struct	pv_entry *pv;

	pv = pv_table + (pa >> CLSHIFT);

	if (pv->pv_pte)
		if ((pv->pv_pte[0].pg_v))
			return 1;

	while ((pv = pv->pv_next)) {
		if ((pv->pv_pte[0].pg_v))
			return 1;
	}
	return 0;
}

/*
 * Clears valid bit in all ptes referenced to this physical page.
 */
#ifdef PMAP_NEW
boolean_t
pmap_clear_reference(pg)
	struct vm_page *pg;
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(pg);
#else
void 
pmap_clear_reference(pa)
	vm_offset_t	pa;
{
#endif
	struct	pv_entry *pv;

	pv = pv_table + (pa >> CLSHIFT);

	if (pv->pv_pte)
		pv->pv_pte[0].pg_v = pv->pv_pte[1].pg_v = 0;

	while ((pv = pv->pv_next))
		pv->pv_pte[0].pg_v = pv->pv_pte[1].pg_v = 0;
#ifdef PMAP_NEW
	return TRUE; /* XXX */
#endif
}

/*
 * Checks if page is modified; returns true or false depending on result.
 */
#ifdef PMAP_NEW
boolean_t
pmap_is_modified(pg)
	struct vm_page *pg;
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(pg);
#else
boolean_t
pmap_is_modified(pa)
     vm_offset_t     pa;
{
#endif
	struct	pv_entry *pv;

	pv = pv_table + (pa >> CLSHIFT);
#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_is_modified: pa %lx pv_entry %p\n", pa, pv);
#endif

	if (pv->pv_pte)
		if ((pv->pv_pte[0].pg_m | pv->pv_pte[1].pg_m))
			return 1;

	while ((pv = pv->pv_next)) {
		if ((pv->pv_pte[0].pg_m | pv->pv_pte[1].pg_m))
			return 1;
	}
	return 0;
}

/*
 * Clears modify bit in all ptes referenced to this physical page.
 */
#ifdef PMAP_NEW
boolean_t
pmap_clear_modify(pg)
	struct vm_page *pg;
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(pg);
#else
void 
pmap_clear_modify(pa)
	vm_offset_t	pa;
{
#endif
	struct	pv_entry *pv;

	pv = pv_table + (pa >> CLSHIFT);

#ifdef PMAPDEBUG
	if (startpmapdebug)
		printf("pmap_clear_modify: pa %lx pv_entry %p\n", pa, pv);
#endif
	if (pv->pv_pte)
		pv->pv_pte[0].pg_m = pv->pv_pte[1].pg_m = 0;

	while ((pv = pv->pv_next))
		pv->pv_pte[0].pg_m = pv->pv_pte[1].pg_m = 0;
#ifdef PMAP_NEW
	return TRUE; /* XXX */
#endif
}

/*
 * Lower the permission for all mappings to a given page.
 * Lower permission can only mean setting protection to either read-only
 * or none; where none is unmapping of the page.
 */
#ifdef PMAP_NEW
void
pmap_page_protect(pg, prot)
	struct vm_page *pg;
	vm_prot_t       prot;
{
	vm_offset_t pa = VM_PAGE_TO_PHYS(pg);
#else
void
pmap_page_protect(pa, prot)
	vm_offset_t	pa;
	vm_prot_t	prot;
{
#endif
	struct	pte *pt;
	struct	pv_entry *pv, *opv;
	int	s;
  
#ifdef PMAPDEBUG
if(startpmapdebug) printf("pmap_page_protect: pa %lx, prot %x\n",pa, prot);
#endif
	pv = pv_table + (pa >> CLSHIFT);
	if (pv->pv_pte == 0 && pv->pv_next == 0)
		return;

	if (prot == VM_PROT_ALL) /* 'cannot happen' */
		return;

	if (prot == VM_PROT_NONE) {
		pt = pv->pv_pte;
		s = splimp();
		if (pt)
			bzero(pt, sizeof(struct pte) * CLSIZE);
		opv = pv;
		pv = pv->pv_next;
		bzero(opv, sizeof(struct pv_entry));
		while (pv) {
			pt = pv->pv_pte;
			bzero(pt, sizeof(struct pte) * CLSIZE);
			opv = pv;
			pv = pv->pv_next;
			FREE(opv, M_VMPVENT);
		}
		splx(s);
	} else { /* read-only */
		do {
			pt = pv->pv_pte;
			if (pt == 0)
				continue;
			pt[0].pg_prot = pt[1].pg_prot = 
			    ((vm_offset_t)pv->pv_pte < ptemapstart ? 
			    PROT_KR : PROT_RO);

		} while ((pv = pv->pv_next));
	}
	mtpr(0, PR_TBIA);
}

/*
 * Activate the address space for the specified process.
 * Note that if the process to activate is the current process, then
 * the processor internal registers must also be loaded; otherwise
 * the current process will have wrong pagetables.
 */
void
pmap_activate(p)
	struct proc *p;
{
	pmap_t pmap;
	struct pcb *pcb;

#ifdef PMAPDEBUG
if(startpmapdebug) printf("pmap_activate: p %p\n", p);
#endif

	pmap = p->p_vmspace->vm_map.pmap;
	pcb = &p->p_addr->u_pcb;

	pcb->P0BR = pmap->pm_p0br;
	pcb->P0LR = pmap->pm_p0lr;
	pcb->P1BR = pmap->pm_p1br;
	pcb->P1LR = pmap->pm_p1lr;

	if (p == curproc) {
		mtpr(pmap->pm_p0br, PR_P0BR);
		mtpr(pmap->pm_p0lr, PR_P0LR);
		mtpr(pmap->pm_p1br, PR_P1BR);
		mtpr(pmap->pm_p1lr, PR_P1LR);
	}
	mtpr(0, PR_TBIA);
}

/*
 * Deactivate the address space for the specified process.
 */
void
pmap_deactivate(p)
	struct proc *p;
{
}
