/*	$NetBSD: pmap.h,v 1.67 2003/04/18 22:44:54 thorpej Exp $	*/

/*
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe & Steve C. Woodford for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994,1995 Mark Brinicombe.
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
 *	This product includes software developed by Mark Brinicombe
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef	_ARM32_PMAP_H_
#define	_ARM32_PMAP_H_

#ifdef _KERNEL

#include <arm/cpuconf.h>
#include <arm/cpufunc.h>
#include <arm/arm32/pte.h>
#include <uvm/uvm_object.h>

/*
 * a pmap describes a processes' 4GB virtual address space.  this
 * virtual address space can be broken up into 4096 1MB regions which
 * are described by L1 PTEs in the L1 table.
 *
 * There is a line drawn at KERNEL_BASE.  Everything below that line
 * changes when the VM context is switched.  Everything above that line
 * is the same no matter which VM context is running.  This is achieved
 * by making the L1 PTEs for those slots above KERNEL_BASE reference
 * kernel L2 tables.
 *
 *#ifndef ARM32_PMAP_NEW
 * The L2 tables are mapped linearly starting at PTE_BASE.  PTE_BASE
 * is below KERNEL_BASE, which means that the current process's PTEs
 * are always available starting at PTE_BASE.  Another region of KVA
 * above KERNEL_BASE, APTE_BASE, is reserved for mapping in the PTEs
 * of another process, should we need to manipulate them.
 *#endif
 *
 * The basic layout of the virtual address space thus looks like this:
 *
 *	0xffffffff
 *	.
 *	.
 *	.
 *	KERNEL_BASE
 *	--------------------
 *#ifndef ARM32_PMAP_NEW
 *	PTE_BASE
 *#endif
 *	.
 *	.
 *	.
 *	0x00000000
 */

#ifdef ARM32_PMAP_NEW
/*
 * The number of L2 descriptor tables which can be tracked by an l2_dtable.
 * A bucket size of 16 provides for 16MB of contiguous virtual address
 * space per l2_dtable. Most processes will, therefore, require only two or
 * three of these to map their whole working set.
 */
#define	L2_BUCKET_LOG2	4
#define	L2_BUCKET_SIZE	(1 << L2_BUCKET_LOG2)

/*
 * Given the above "L2-descriptors-per-l2_dtable" constant, the number
 * of l2_dtable structures required to track all possible page descriptors
 * mappable by an L1 translation table is given by the following constants:
 */
#define	L2_LOG2		((32 - L1_S_SHIFT) - L2_BUCKET_LOG2)
#define	L2_SIZE		(1 << L2_LOG2)

struct l1_ttable;
struct l2_dtable;

/*
 * Track cache/tlb occupancy using the following structure
 */
union pmap_cache_state {
	struct {
		union {
			u_int8_t csu_cache_b[2];
			u_int16_t csu_cache;
		} cs_cache_u;

		union {
			u_int8_t csu_tlb_b[2];
			u_int16_t csu_tlb;
		} cs_tlb_u;
	} cs_s;
	u_int32_t cs_all;
};
#define	cs_cache_id	cs_s.cs_cache_u.csu_cache_b[0]
#define	cs_cache_d	cs_s.cs_cache_u.csu_cache_b[1]
#define	cs_cache	cs_s.cs_cache_u.csu_cache
#define	cs_tlb_id	cs_s.cs_tlb_u.csu_tlb_b[0]
#define	cs_tlb_d	cs_s.cs_tlb_u.csu_tlb_b[1]
#define	cs_tlb		cs_s.cs_tlb_u.csu_tlb

/*
 * Assigned to cs_all to force cacheops to work for a particular pmap
 */
#define	PMAP_CACHE_STATE_ALL	0xffffffffu

/*
 * The pmap structure itself
 */
struct pmap {
	u_int8_t		pm_domain;
	boolean_t		pm_remove_all;
	struct l1_ttable	*pm_l1;
	union pmap_cache_state	pm_cstate;
	struct uvm_object	pm_obj;
#define	pm_lock pm_obj.vmobjlock
	struct l2_dtable	*pm_l2[L2_SIZE];
	struct pmap_statistics	pm_stats;
	LIST_ENTRY(pmap)	pm_list;
};

#else	/* !ARM32_PMAP_NEW */

/*
 * The pmap structure itself.
 */
struct pmap {
	struct uvm_object	pm_obj;		/* uvm_object */
#define	pm_lock	pm_obj.vmobjlock	
	LIST_ENTRY(pmap)	pm_list;	/* list (lck by pm_list lock) */
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	struct l1pt		*pm_l1pt;	/* L1 table metadata */
	paddr_t                 pm_pptpt;	/* PA of pt's page table */
	vaddr_t                 pm_vptpt;	/* VA of pt's page table */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	struct vm_page		*pm_ptphint;	/* recently used PT */
};
#endif	/* ARM32_PMAP_NEW */

typedef struct pmap *pmap_t;

/*
 * Physical / virtual address structure. In a number of places (particularly
 * during bootstrapping) we need to keep track of the physical and virtual
 * addresses of various pages
 */
typedef struct pv_addr {
	SLIST_ENTRY(pv_addr) pv_list;
	paddr_t pv_pa;
	vaddr_t pv_va;
} pv_addr_t;

/*
 * Determine various modes for PTEs (user vs. kernel, cacheable
 * vs. non-cacheable).
 */
#define	PTE_KERNEL	0
#define	PTE_USER	1
#define	PTE_NOCACHE	0
#define	PTE_CACHE	1
#ifdef ARM32_PMAP_NEW
#define	PTE_PAGETABLE	2
#endif

/*
 * Flags that indicate attributes of pages or mappings of pages.
 *
 * The PVF_MOD and PVF_REF flags are stored in the mdpage for each
 * page.  PVF_WIRED, PVF_WRITE, and PVF_NC are kept in individual
 * pv_entry's for each page.  They live in the same "namespace" so
 * that we can clear multiple attributes at a time.
 *
 * Note the "non-cacheable" flag generally means the page has
 * multiple mappings in a given address space.
 */
#define	PVF_MOD		0x01		/* page is modified */
#define	PVF_REF		0x02		/* page is referenced */
#define	PVF_WIRED	0x04		/* mapping is wired */
#define	PVF_WRITE	0x08		/* mapping is writable */
#define	PVF_EXEC	0x10		/* mapping is executable */
#ifndef ARM32_PMAP_NEW
#define	PVF_NC		0x20		/* mapping is non-cacheable */
#else
#define	PVF_UNC		0x20		/* mapping is 'user' non-cacheable */
#define	PVF_KNC		0x40		/* mapping is 'kernel' non-cacheable */
#define	PVF_NC		(PVF_UNC|PVF_KNC)
#endif

/*
 * Commonly referenced structures
 */
extern struct pmap	kernel_pmap_store;
extern int		pmap_debug_level; /* Only exists if PMAP_DEBUG */

/*
 * Macros that we need to export
 */
#define pmap_kernel()			(&kernel_pmap_store)
#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)
#define	pmap_wired_count(pmap)		((pmap)->pm_stats.wired_count)

#define	pmap_is_modified(pg)	\
	(((pg)->mdpage.pvh_attrs & PVF_MOD) != 0)
#define	pmap_is_referenced(pg)	\
	(((pg)->mdpage.pvh_attrs & PVF_REF) != 0)

#define	pmap_copy(dp, sp, da, l, sa)	/* nothing */

#ifndef ARM32_PMAP_NEW
/* ARGSUSED */
static __inline void
pmap_remove_all(struct pmap *pmap)
{
	/* Nothing. */
}
#endif

#define pmap_phys_address(ppn)		(arm_ptob((ppn)))

/*
 * Functions that we need to export
 */
void	pmap_procwr(struct proc *, vaddr_t, int);
#ifdef ARM32_PMAP_NEW
void	pmap_remove_all(pmap_t);
boolean_t pmap_extract(pmap_t, vaddr_t, paddr_t *);
#endif

#define	PMAP_NEED_PROCWR
#define PMAP_GROWKERNEL		/* turn on pmap_growkernel interface */

/* Functions we use internally. */
#ifndef ARM32_PMAP_NEW
/*
 * Old pmap
 */
void	pmap_bootstrap(pd_entry_t *, pv_addr_t);
int	pmap_handled_emulation(struct pmap *, vaddr_t);
int	pmap_modified_emulation(struct pmap *, vaddr_t);
#else
/*
 * New pmap
 */
#ifdef ARM32_NEW_VM_LAYOUT
void	pmap_bootstrap(pd_entry_t *, vaddr_t);
#else
void	pmap_bootstrap(pd_entry_t *);
#endif

int	pmap_fault_fixup(pmap_t, vaddr_t, vm_prot_t);
boolean_t pmap_get_pde_pte(pmap_t, vaddr_t, pd_entry_t **, pt_entry_t **);
boolean_t pmap_get_pde(pmap_t, vaddr_t, pd_entry_t **);
void	pmap_set_pcb_pagedir(pmap_t, struct pcb *);
#endif	/* ARM32_PMAP_NEW */

void	pmap_debug(int);
void	pmap_postinit(void);

void	vector_page_setprot(int);

/* Bootstrapping routines. */
void	pmap_map_section(vaddr_t, vaddr_t, paddr_t, int, int);
void	pmap_map_entry(vaddr_t, vaddr_t, paddr_t, int, int);
vsize_t	pmap_map_chunk(vaddr_t, vaddr_t, paddr_t, vsize_t, int, int);
void	pmap_link_l2pt(vaddr_t, vaddr_t, pv_addr_t *);

/*
 * Special page zero routine for use by the idle loop (no cache cleans). 
 */
boolean_t	pmap_pageidlezero(paddr_t);
#define PMAP_PAGEIDLEZERO(pa)	pmap_pageidlezero((pa))

/*
 * The current top of kernel VM
 */
extern vaddr_t	pmap_curmaxkvaddr;

/*
 * Useful macros and constants 
 */

#ifndef ARM32_PMAP_NEW
/*
 * While the ARM MMU's L1 descriptors describe a 1M "section", each
 * one pointing to a 1K L2 table, NetBSD's VM system allocates the
 * page tables in 4K chunks, and thus we describe 4M "super sections".
 *
 * We'll lift terminology from another architecture and refer to this as
 * the "page directory" size.
 */
#define	PD_SIZE		(L1_S_SIZE * 4)		/* 4M */
#define	PD_OFFSET	(PD_SIZE - 1)
#define	PD_FRAME	(~PD_OFFSET)
#define	PD_SHIFT	22

/* Virtual address to page table entry */
#define vtopte(va) \
	(((pt_entry_t *)PTE_BASE) + arm_btop((vaddr_t) (va)))

/* Virtual address to physical address */
#define vtophys(va) \
	((*vtopte(va) & L2_S_FRAME) | ((vaddr_t) (va) & L2_S_OFFSET))

#define	PTE_SYNC(pte) \
	cpu_dcache_wb_range((vaddr_t)(pte), sizeof(pt_entry_t))
#define	PTE_FLUSH(pte) \
	cpu_dcache_wbinv_range((vaddr_t)(pte), sizeof(pt_entry_t))

#define	PTE_SYNC_RANGE(pte, cnt) \
	cpu_dcache_wb_range((vaddr_t)(pte), (cnt) << 2) /* * sizeof(...) */
#define	PTE_FLUSH_RANGE(pte, cnt) \
	cpu_dcache_wbinv_range((vaddr_t)(pte), (cnt) << 2) /* * sizeof(...) */

#else	/* ARM32_PMAP_NEW */

/* Virtual address to page table entry */
static __inline pt_entry_t *
vtopte(vaddr_t va)
{
	pd_entry_t *pdep;
	pt_entry_t *ptep;

	if (pmap_get_pde_pte(pmap_kernel(), va, &pdep, &ptep) == FALSE)
		return (NULL);
	return (ptep);
}

/*
 * Virtual address to physical address
 */
static __inline paddr_t
vtophys(vaddr_t va)
{
	paddr_t pa;

	if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
		return (0);	/* XXXSCW: Panic? */

	return (pa);
}
#endif	/* ARM32_PMAP_NEW */

/*
 * The new pmap ensures that page-tables are always mapping Write-Thru.
 * Thus, on some platforms we can run fast and loose and avoid syncing PTEs
 * on every change.
 *
 * Actually, this may not work out quite as well as I'd planned.
 * According to some documentation, the cache-mode "write-thru, unbuffered",
 * as used by the pmap for page tables, may not work correctly on all types
 * of cache.
 */
#if !defined(ARM32_PMAP_NEW) || defined(ARM32_PMAP_NEEDS_PTE_SYNC)
#define	PTE_SYNC(pte) \
	cpu_dcache_wb_range((vaddr_t)(pte), sizeof(pt_entry_t))
#define	PTE_FLUSH(pte) \
	cpu_dcache_wbinv_range((vaddr_t)(pte), sizeof(pt_entry_t))

#define	PTE_SYNC_RANGE(pte, cnt) \
	cpu_dcache_wb_range((vaddr_t)(pte), (cnt) << 2) /* * sizeof(...) */
#define	PTE_FLUSH_RANGE(pte, cnt) \
	cpu_dcache_wbinv_range((vaddr_t)(pte), (cnt) << 2) /* * sizeof(...) */
#else
#define	PTE_SYNC(x)		/* no-op */
#define	PTE_FLUSH(x)		/* no-op */
#define	PTE_SYNC_RANGE(x,y)	/* no-op */
#define	PTE_FLUSH_RANGE(x,y)	/* no-op */
#endif

#define	l1pte_valid(pde)	((pde) != 0)
#define	l1pte_section_p(pde)	(((pde) & L1_TYPE_MASK) == L1_TYPE_S)
#define	l1pte_page_p(pde)	(((pde) & L1_TYPE_MASK) == L1_TYPE_C)
#define	l1pte_fpage_p(pde)	(((pde) & L1_TYPE_MASK) == L1_TYPE_F)

#ifdef ARM32_PMAP_NEW
#define l2pte_index(v)		(((v) & L2_ADDR_BITS) >> L2_S_SHIFT)
#endif
#define	l2pte_valid(pte)	((pte) != 0)
#define	l2pte_pa(pte)		((pte) & L2_S_FRAME)

/* L1 and L2 page table macros */
#ifndef ARM32_PMAP_NEW
#define pmap_pdei(v)		((v & L1_S_FRAME) >> L1_S_SHIFT)
#define pmap_pde(m, v)		(&((m)->pm_pdir[pmap_pdei(v)]))
#endif

#define pmap_pde_v(pde)		l1pte_valid(*(pde))
#define pmap_pde_section(pde)	l1pte_section_p(*(pde))
#define pmap_pde_page(pde)	l1pte_page_p(*(pde))
#define pmap_pde_fpage(pde)	l1pte_fpage_p(*(pde))

#define	pmap_pte_v(pte)		l2pte_valid(*(pte))
#define	pmap_pte_pa(pte)	l2pte_pa(*(pte))

/* Size of the kernel part of the L1 page table */
#define KERNEL_PD_SIZE	\
	(L1_TABLE_SIZE - (KERNEL_BASE >> L1_S_SHIFT) * sizeof(pd_entry_t))

/************************* ARM MMU configuration *****************************/

#if ARM_MMU_GENERIC == 1
void	pmap_copy_page_generic(paddr_t, paddr_t);
void	pmap_zero_page_generic(paddr_t);

void	pmap_pte_init_generic(void);
#if defined(CPU_ARM9)
void	pmap_pte_init_arm9(void);
#endif /* CPU_ARM9 */
#endif /* ARM_MMU_GENERIC == 1 */

#if ARM_MMU_XSCALE == 1
void	pmap_copy_page_xscale(paddr_t, paddr_t);
void	pmap_zero_page_xscale(paddr_t);

void	pmap_pte_init_xscale(void);

void	xscale_setup_minidata(vaddr_t, vaddr_t, paddr_t);
#endif /* ARM_MMU_XSCALE == 1 */

extern pt_entry_t		pte_l1_s_cache_mode;
extern pt_entry_t		pte_l1_s_cache_mask;

extern pt_entry_t		pte_l2_l_cache_mode;
extern pt_entry_t		pte_l2_l_cache_mask;

extern pt_entry_t		pte_l2_s_cache_mode;
extern pt_entry_t		pte_l2_s_cache_mask;

#ifdef ARM32_PMAP_NEW
extern pt_entry_t		pte_l1_s_cache_mode_pt;
extern pt_entry_t		pte_l2_l_cache_mode_pt;
extern pt_entry_t		pte_l2_s_cache_mode_pt;
#endif

extern pt_entry_t		pte_l2_s_prot_u;
extern pt_entry_t		pte_l2_s_prot_w;
extern pt_entry_t		pte_l2_s_prot_mask;
 
extern pt_entry_t		pte_l1_s_proto;
extern pt_entry_t		pte_l1_c_proto;
extern pt_entry_t		pte_l2_s_proto;

extern void (*pmap_copy_page_func)(paddr_t, paddr_t);
extern void (*pmap_zero_page_func)(paddr_t);

/*****************************************************************************/

/*
 * tell MI code that the cache is virtually-indexed *and* virtually-tagged.
 */
#define PMAP_CACHE_VIVT

#ifdef ARM32_PMAP_NEW
/*
 * Definitions for MMU domains
 */
#define	PMAP_DOMAINS		15	/* 15 'user' domains (0-14) */
#define	PMAP_DOMAIN_KERNEL	15	/* The kernel uses domain #15 */
#endif

/*
 * These macros define the various bit masks in the PTE.
 *
 * We use these macros since we use different bits on different processor
 * models.
 */
#define	L1_S_PROT_U		(L1_S_AP(AP_U))
#define	L1_S_PROT_W		(L1_S_AP(AP_W))
#define	L1_S_PROT_MASK		(L1_S_PROT_U|L1_S_PROT_W)

#define	L1_S_CACHE_MASK_generic	(L1_S_B|L1_S_C)
#define	L1_S_CACHE_MASK_xscale	(L1_S_B|L1_S_C|L1_S_XSCALE_TEX(TEX_XSCALE_X))

#define	L2_L_PROT_U		(L2_AP(AP_U))
#define	L2_L_PROT_W		(L2_AP(AP_W))
#define	L2_L_PROT_MASK		(L2_L_PROT_U|L2_L_PROT_W)

#define	L2_L_CACHE_MASK_generic	(L2_B|L2_C)
#define	L2_L_CACHE_MASK_xscale	(L2_B|L2_C|L2_XSCALE_L_TEX(TEX_XSCALE_X))

#define	L2_S_PROT_U_generic	(L2_AP(AP_U))
#define	L2_S_PROT_W_generic	(L2_AP(AP_W))
#define	L2_S_PROT_MASK_generic	(L2_S_PROT_U|L2_S_PROT_W)

#define	L2_S_PROT_U_xscale	(L2_AP0(AP_U))
#define	L2_S_PROT_W_xscale	(L2_AP0(AP_W))
#define	L2_S_PROT_MASK_xscale	(L2_S_PROT_U|L2_S_PROT_W)

#define	L2_S_CACHE_MASK_generic	(L2_B|L2_C)
#define	L2_S_CACHE_MASK_xscale	(L2_B|L2_C|L2_XSCALE_T_TEX(TEX_XSCALE_X))

#define	L1_S_PROTO_generic	(L1_TYPE_S | L1_S_IMP)
#define	L1_S_PROTO_xscale	(L1_TYPE_S)

#define	L1_C_PROTO_generic	(L1_TYPE_C | L1_C_IMP2)
#define	L1_C_PROTO_xscale	(L1_TYPE_C)

#define	L2_L_PROTO		(L2_TYPE_L)

#define	L2_S_PROTO_generic	(L2_TYPE_S)
#define	L2_S_PROTO_xscale	(L2_TYPE_XSCALE_XS)

/*
 * User-visible names for the ones that vary with MMU class.
 */

#if ARM_NMMUS > 1
/* More than one MMU class configured; use variables. */
#define	L2_S_PROT_U		pte_l2_s_prot_u
#define	L2_S_PROT_W		pte_l2_s_prot_w
#define	L2_S_PROT_MASK		pte_l2_s_prot_mask

#define	L1_S_CACHE_MASK		pte_l1_s_cache_mask
#define	L2_L_CACHE_MASK		pte_l2_l_cache_mask
#define	L2_S_CACHE_MASK		pte_l2_s_cache_mask

#define	L1_S_PROTO		pte_l1_s_proto
#define	L1_C_PROTO		pte_l1_c_proto
#define	L2_S_PROTO		pte_l2_s_proto

#define	pmap_copy_page(s, d)	(*pmap_copy_page_func)((s), (d))
#define	pmap_zero_page(d)	(*pmap_zero_page_func)((d))
#elif ARM_MMU_GENERIC == 1
#define	L2_S_PROT_U		L2_S_PROT_U_generic
#define	L2_S_PROT_W		L2_S_PROT_W_generic
#define	L2_S_PROT_MASK		L2_S_PROT_MASK_generic

#define	L1_S_CACHE_MASK		L1_S_CACHE_MASK_generic
#define	L2_L_CACHE_MASK		L2_L_CACHE_MASK_generic
#define	L2_S_CACHE_MASK		L2_S_CACHE_MASK_generic

#define	L1_S_PROTO		L1_S_PROTO_generic
#define	L1_C_PROTO		L1_C_PROTO_generic
#define	L2_S_PROTO		L2_S_PROTO_generic

#define	pmap_copy_page(s, d)	pmap_copy_page_generic((s), (d))
#define	pmap_zero_page(d)	pmap_zero_page_generic((d))
#elif ARM_MMU_XSCALE == 1
#define	L2_S_PROT_U		L2_S_PROT_U_xscale
#define	L2_S_PROT_W		L2_S_PROT_W_xscale
#define	L2_S_PROT_MASK		L2_S_PROT_MASK_xscale

#define	L1_S_CACHE_MASK		L1_S_CACHE_MASK_xscale
#define	L2_L_CACHE_MASK		L2_L_CACHE_MASK_xscale
#define	L2_S_CACHE_MASK		L2_S_CACHE_MASK_xscale

#define	L1_S_PROTO		L1_S_PROTO_xscale
#define	L1_C_PROTO		L1_C_PROTO_xscale
#define	L2_S_PROTO		L2_S_PROTO_xscale

#define	pmap_copy_page(s, d)	pmap_copy_page_xscale((s), (d))
#define	pmap_zero_page(d)	pmap_zero_page_xscale((d))
#endif /* ARM_NMMUS > 1 */

/*
 * These macros return various bits based on kernel/user and protection.
 * Note that the compiler will usually fold these at compile time.
 */
#define	L1_S_PROT(ku, pr)	((((ku) == PTE_USER) ? L1_S_PROT_U : 0) | \
				 (((pr) & VM_PROT_WRITE) ? L1_S_PROT_W : 0))

#define	L2_L_PROT(ku, pr)	((((ku) == PTE_USER) ? L2_L_PROT_U : 0) | \
				 (((pr) & VM_PROT_WRITE) ? L2_L_PROT_W : 0))

#define	L2_S_PROT(ku, pr)	((((ku) == PTE_USER) ? L2_S_PROT_U : 0) | \
				 (((pr) & VM_PROT_WRITE) ? L2_S_PROT_W : 0))

/*
 * Macros to test if a mapping is mappable with an L1 Section mapping
 * or an L2 Large Page mapping.
 */
#define	L1_S_MAPPABLE_P(va, pa, size)					\
	((((va) | (pa)) & L1_S_OFFSET) == 0 && (size) >= L1_S_SIZE)

#define	L2_L_MAPPABLE_P(va, pa, size)					\
	((((va) | (pa)) & L2_S_OFFSET) == 0 && (size) >= L2_L_SIZE)

/*
 * Hooks for the pool allocator.
 */
#define	POOL_VTOPHYS(va)	vtophys((vaddr_t) (va))

#endif /* _KERNEL */

#endif	/* _ARM32_PMAP_H_ */
