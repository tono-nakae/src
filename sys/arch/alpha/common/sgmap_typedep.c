/* $NetBSD: sgmap_typedep.c,v 1.17 2001/07/19 04:27:37 thorpej Exp $ */

/*-
 * Copyright (c) 1997, 1998, 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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

__KERNEL_RCSID(0, "$NetBSD: sgmap_typedep.c,v 1.17 2001/07/19 04:27:37 thorpej Exp $");

#include "opt_ddb.h"

#ifdef SGMAP_DEBUG
int			__C(SGMAP_TYPE,_debug) = 0;
#endif

SGMAP_PTE_TYPE		__C(SGMAP_TYPE,_prefetch_spill_page_pte);

void
__C(SGMAP_TYPE,_init_spill_page_pte)(void)
{

	__C(SGMAP_TYPE,_prefetch_spill_page_pte) =
	    (alpha_sgmap_prefetch_spill_page_pa >>
	     SGPTE_PGADDR_SHIFT) | SGPTE_VALID;
}

int
__C(SGMAP_TYPE,_load)(bus_dma_tag_t t, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags, struct alpha_sgmap *sgmap)
{
	vaddr_t endva, va = (vaddr_t)buf;
	paddr_t pa;
	bus_addr_t dmaoffset, sgva;
	bus_size_t sgvalen, boundary, alignment;
	SGMAP_PTE_TYPE *pte, *page_table = sgmap->aps_pt;
	int pteidx, error, spill;

	/*
	 * Initialize the spill page PTE if that hasn't already been done.
	 */
	if (__C(SGMAP_TYPE,_prefetch_spill_page_pte) == 0)
		__C(SGMAP_TYPE,_init_spill_page_pte)();

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

	if (buflen > map->_dm_size)
		return (EINVAL);

	/*
	 * Remember the offset into the first page and the total
	 * transfer length.
	 */
	dmaoffset = ((u_long)buf) & PGOFSET;

#ifdef SGMAP_DEBUG
	if (__C(SGMAP_TYPE,_debug)) {
		printf("sgmap_load: ----- buf = %p -----\n", buf);
		printf("sgmap_load: dmaoffset = 0x%lx, buflen = 0x%lx\n",
		    dmaoffset, buflen);
	}
#endif

	/*
	 * Allocate the necessary virtual address space for the
	 * mapping.  Round the size, since we deal with whole pages.
	 */

	/* XXX Always allocate a spill page for now. */
	spill = 1;

	endva = round_page(va + buflen);
	va = trunc_page(va);

	boundary = map->_dm_boundary;
	alignment = NBPG;

	sgvalen = (endva - va);
	if (spill) {
		sgvalen += NBPG;

		/*
		 * ARGH!  If the addition of the spill page bumped us
		 * over our boundary, we have to 2x the boundary limit.
		 */
		if (boundary && boundary < sgvalen) {
			alignment = boundary;
			do {
				boundary <<= 1;
			} while (boundary < sgvalen);
		}
	}

#if 0
	printf("len 0x%lx -> 0x%lx, boundary 0x%lx -> 0x%lx -> ",
	    (endva - va), sgvalen, map->_dm_boundary, boundary);
#endif

	error = extent_alloc(sgmap->aps_ex, sgvalen, alignment, boundary,
	    (flags & BUS_DMA_NOWAIT) ? EX_NOWAIT : EX_WAITOK, &sgva);

#if 0
	printf("error %d sgva 0x%lx\n", error, sgva);
#endif

	pteidx = sgva >> SGMAP_ADDR_PTEIDX_SHIFT;
	pte = &page_table[pteidx * SGMAP_PTE_SPACING];

#ifdef SGMAP_DEBUG
	if (__C(SGMAP_TYPE,_debug))
		printf("sgmap_load: sgva = 0x%lx, pteidx = %d, "
		    "pte = %p (pt = %p)\n", sgva, pteidx, pte,
		    page_table);
#endif

	/* Generate the DMA address. */
	map->dm_segs[0].ds_addr = sgmap->aps_wbase | sgva | dmaoffset;
	map->dm_segs[0].ds_len = buflen;

#ifdef SGMAP_DEBUG
	if (__C(SGMAP_TYPE,_debug))
		printf("sgmap_load: wbase = 0x%lx, vpage = 0x%x, "
		    "dma addr = 0x%lx\n", sgmap->aps_wbase,
		    (pteidx << SGMAP_ADDR_PTEIDX_SHIFT),
		    map->dm_segs[0].ds_addr);
#endif

	for (; va < endva; va += NBPG, pteidx++,
	     pte = &page_table[pteidx * SGMAP_PTE_SPACING]) {
		/* Get the physical address for this segment. */
		if (p != NULL)
			(void) pmap_extract(p->p_vmspace->vm_map.pmap, va,
			    &pa);
		else
			pa = vtophys(va);

		/* Load the current PTE with this page. */
		*pte = (pa >> SGPTE_PGADDR_SHIFT) | SGPTE_VALID;
#ifdef SGMAP_DEBUG
		if (__C(SGMAP_TYPE,_debug))
			printf("sgmap_load:     pa = 0x%lx, pte = %p, "
			    "*pte = 0x%lx\n", pa, pte, (u_long)(*pte));
#endif
	}

	if (spill) {
		/* ...and the prefetch-spill page. */
		*pte = __C(SGMAP_TYPE,_prefetch_spill_page_pte);
#ifdef SGMAP_DEBUG
		if (__C(SGMAP_TYPE,_debug)) {
			printf("sgmap_load:     spill page, pte = %p, "
			    "*pte = 0x%lx\n", pte, *pte);
			printf("sgmap_load:     pte count = %d\n",
			    map->_dm_ptecnt);
		}
#endif
	}

	alpha_mb();

#if defined(SGMAP_DEBUG) && defined(DDB)
	if (__C(SGMAP_TYPE,_debug) > 1)
		Debugger();
#endif
	map->dm_mapsize = buflen;
	map->dm_nsegs = 1;
	return (0);
}

int
__C(SGMAP_TYPE,_load_mbuf)(bus_dma_tag_t t, bus_dmamap_t map,
    struct mbuf *m, int flags, struct alpha_sgmap *sgmap)
{

	panic(__S(__C(SGMAP_TYPE,_load_mbuf)) ": not implemented");
}

int
__C(SGMAP_TYPE,_load_uio)(bus_dma_tag_t t, bus_dmamap_t map, struct uio *uio,
    int flags, struct alpha_sgmap *sgmap)
{

	panic(__S(__C(SGMAP_TYPE,_load_uio)) ": not implemented");
}

int
__C(SGMAP_TYPE,_load_raw)(bus_dma_tag_t t, bus_dmamap_t map,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags,
    struct alpha_sgmap *sgmap)
{

	panic(__S(__C(SGMAP_TYPE,_load_raw)) ": not implemented");
}

void
__C(SGMAP_TYPE,_unload)(bus_dma_tag_t t, bus_dmamap_t map,
    struct alpha_sgmap *sgmap)
{
	SGMAP_PTE_TYPE *pte, *page_table = sgmap->aps_pt;
	bus_addr_t osgva, sgva, esgva;
	int spill, seg, pteidx;

	for (seg = 0; seg < map->dm_nsegs; seg++) {
		/* XXX Always have a spill page for now... */
		spill = 1;

		sgva = map->dm_segs[seg].ds_addr & ~sgmap->aps_wbase;

		esgva = round_page(sgva + map->dm_segs[seg].ds_len);
		osgva = sgva = trunc_page(sgva);

		if (spill)
			esgva += NBPG;

		/* Invalidate the PTEs for the mapping. */
		for (pteidx = sgva >> SGMAP_ADDR_PTEIDX_SHIFT;
		     sgva < esgva; sgva += NBPG, pteidx++) {
			pte = &page_table[pteidx * SGMAP_PTE_SPACING];
#ifdef SGMAP_DEBUG
			if (__C(SGMAP_TYPE,_debug))
				printf("sgmap_unload:     pte = %p, "
				    "*pte = 0x%lx\n", pte, (u_long)(*pte));
#endif
			*pte = 0;
		}

		alpha_mb();

		/* Free the virtual address space used by the mapping. */
		if (extent_free(sgmap->aps_ex, osgva, (esgva - osgva),
		    EX_NOWAIT) != 0)
			panic(__S(__C(SGMAP_TYPE,_unload)));
	}

	/* Mark the mapping invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}
