/*	$NetBSD: iommu.c,v 1.41 2000/05/23 11:39:58 pk Exp $ */

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1995 	Paul Kranenburg
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
 *	This product includes software developed by Aaron Brown and
 *	Harvard University.
 *	This product includes software developed by Paul Kranenburg.
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
 */

#include <sys/param.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm.h>

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/ctlreg.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/sparc/iommureg.h>
#include <sparc/sparc/iommuvar.h>

struct iommu_softc {
	struct device	sc_dev;		/* base device */
	struct iommureg	*sc_reg;
	u_int		sc_pagesize;
	u_int		sc_range;
	bus_addr_t	sc_dvmabase;
	iopte_t		*sc_ptes;
	int		sc_hasiocache;
};
struct	iommu_softc *iommu_sc;/*XXX*/
int	has_iocache;
u_long	dvma_cachealign;

/*
 * Note: operations on the extent map are being protected with
 * splhigh(), since we cannot predict at which interrupt priority
 * our clients will run.
 */
struct extent *iommu_dvmamap;


/* autoconfiguration driver */
int	iommu_print __P((void *, const char *));
void	iommu_attach __P((struct device *, struct device *, void *));
int	iommu_match __P((struct device *, struct cfdata *, void *));

struct cfattach iommu_ca = {
	sizeof(struct iommu_softc), iommu_match, iommu_attach
};

/* IOMMU DMA map functions */
int	iommu_dmamap_load __P((bus_dma_tag_t, bus_dmamap_t, void *,
			bus_size_t, struct proc *, int));
int	iommu_dmamap_load_mbuf __P((bus_dma_tag_t, bus_dmamap_t,
			struct mbuf *, int));
int	iommu_dmamap_load_uio __P((bus_dma_tag_t, bus_dmamap_t,
			struct uio *, int));
int	iommu_dmamap_load_raw __P((bus_dma_tag_t, bus_dmamap_t,
			bus_dma_segment_t *, int, bus_size_t, int));
void	iommu_dmamap_unload __P((bus_dma_tag_t, bus_dmamap_t));
void	iommu_dmamap_sync __P((bus_dma_tag_t, bus_dmamap_t, bus_addr_t,
			bus_size_t, int));

int	iommu_dmamem_map __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
			int nsegs, size_t size, caddr_t *kvap, int flags));
int	iommu_dmamem_mmap __P((bus_dma_tag_t tag, bus_dma_segment_t *segs,
			int nsegs, int off, int prot, int flags));
int	iommu_dvma_alloc(bus_dmamap_t, vaddr_t, bus_size_t, int,
			bus_addr_t *, bus_size_t *);


struct sparc_bus_dma_tag iommu_dma_tag = {
	NULL,
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	iommu_dmamap_load,
	iommu_dmamap_load_mbuf,
	iommu_dmamap_load_uio,
	iommu_dmamap_load_raw,
	iommu_dmamap_unload,
	iommu_dmamap_sync,

	_bus_dmamem_alloc,
	_bus_dmamem_free,
	iommu_dmamem_map,
	_bus_dmamem_unmap,
	iommu_dmamem_mmap
};
/*
 * Print the location of some iommu-attached device (called just
 * before attaching that device).  If `iommu' is not NULL, the
 * device was found but not configured; print the iommu as well.
 * Return UNCONF (config_find ignores this if the device was configured).
 */
int
iommu_print(args, iommu)
	void *args;
	const char *iommu;
{
	struct iommu_attach_args *ia = args;

	if (iommu)
		printf("%s at %s", ia->iom_name, iommu);
	return (UNCONF);
}

int
iommu_match(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	if (CPU_ISSUN4OR4C)
		return (0);
	return (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0);
}

/*
 * Attach the iommu.
 */
void
iommu_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
#if defined(SUN4M)
	struct iommu_softc *sc = (struct iommu_softc *)self;
	struct mainbus_attach_args *ma = aux;
	int node;
	bus_space_handle_t bh;
	u_int pbase, pa;
	int i, mmupcrsave, s;
	iopte_t *tpte_p;
	extern u_int *kernel_iopte_table;
	extern u_int kernel_iopte_table_pa;

/*XXX-GCC!*/mmupcrsave=0;
	iommu_sc = sc;
	/*
	 * XXX there is only one iommu, for now -- do not know how to
	 * address children on others
	 */
	if (sc->sc_dev.dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}
	node = ma->ma_node;

#if 0
	if (ra->ra_vaddr)
		sc->sc_reg = (struct iommureg *)ca->ca_ra.ra_vaddr;
#else
	/*
	 * Map registers into our space. The PROM may have done this
	 * already, but I feel better if we have our own copy. Plus, the
	 * prom doesn't map the entire register set
	 *
	 * XXX struct iommureg is bigger than ra->ra_len; what are the
	 *     other fields for?
	 */
	if (bus_space_map2(
			ma->ma_bustag,
			ma->ma_iospace,
			ma->ma_paddr,
			sizeof(struct iommureg),
			0,
			0,
			&bh) != 0) {
		printf("iommu_attach: cannot map registers\n");
		return;
	}
	sc->sc_reg = (struct iommureg *)bh;
#endif

	sc->sc_hasiocache = node_has_property(node, "cache-coherence?");
	if (CACHEINFO.c_enabled == 0) /* XXX - is this correct? */
		sc->sc_hasiocache = 0;
	has_iocache = sc->sc_hasiocache; /* Set global flag */

	sc->sc_pagesize = getpropint(node, "page-size", NBPG),
	sc->sc_range = (1 << 24) <<
	    ((sc->sc_reg->io_cr & IOMMU_CTL_RANGE) >> IOMMU_CTL_RANGESHFT);
#if 0
	sc->sc_dvmabase = (0 - sc->sc_range);
#endif
	pbase = (sc->sc_reg->io_bar & IOMMU_BAR_IBA) <<
			(14 - IOMMU_BAR_IBASHFT);

	/*
	 * Now we build our own copy of the IOMMU page tables. We need to
	 * do this since we're going to change the range to give us 64M of
	 * mappings, and thus we can move DVMA space down to 0xfd000000 to
	 * give us lots of space and to avoid bumping into the PROM, etc.
	 *
	 * XXX Note that this is rather messy.
	 */
	sc->sc_ptes = (iopte_t *) kernel_iopte_table;

	/*
	 * Now discache the page tables so that the IOMMU sees our
	 * changes.
	 */
	kvm_uncache((caddr_t)sc->sc_ptes,
	    (((0 - IOMMU_DVMA_BASE)/sc->sc_pagesize) * sizeof(iopte_t)) / NBPG);

	/*
	 * Ok. We've got to read in the original table using MMU bypass,
	 * and copy all of its entries to the appropriate place in our
	 * new table, even if the sizes are different.
	 * This is pretty easy since we know DVMA ends at 0xffffffff.
	 *
	 * XXX: PGOFSET, NBPG assume same page size as SRMMU
	 */
	if (cpuinfo.cpu_impl == 4 && cpuinfo.mxcc) {
		/* set MMU AC bit */
		sta(SRMMU_PCR, ASI_SRMMU,
		    ((mmupcrsave = lda(SRMMU_PCR, ASI_SRMMU)) | VIKING_PCR_AC));
	}

	for (tpte_p = &sc->sc_ptes[((0 - IOMMU_DVMA_BASE)/NBPG) - 1],
	     pa = (u_int)pbase - sizeof(iopte_t) +
		   ((u_int)sc->sc_range/NBPG)*sizeof(iopte_t);
	     tpte_p >= &sc->sc_ptes[0] && pa >= (u_int)pbase;
	     tpte_p--, pa -= sizeof(iopte_t)) {

		IOMMU_FLUSHPAGE(sc,
			     (tpte_p - &sc->sc_ptes[0])*NBPG + IOMMU_DVMA_BASE);
		*tpte_p = lda(pa, ASI_BYPASS);
	}
	if (cpuinfo.cpu_impl == 4 && cpuinfo.mxcc) {
		/* restore mmu after bug-avoidance */
		sta(SRMMU_PCR, ASI_SRMMU, mmupcrsave);
	}

	/*
	 * Now we can install our new pagetable into the IOMMU
	 */
	sc->sc_range = 0 - IOMMU_DVMA_BASE;
	sc->sc_dvmabase = IOMMU_DVMA_BASE;

	/* calculate log2(sc->sc_range/16MB) */
	i = ffs(sc->sc_range/(1 << 24)) - 1;
	if ((1 << i) != (sc->sc_range/(1 << 24)))
		panic("bad iommu range: %d\n",i);

	s = splhigh();
	IOMMU_FLUSHALL(sc);

	sc->sc_reg->io_cr = (sc->sc_reg->io_cr & ~IOMMU_CTL_RANGE) |
			  (i << IOMMU_CTL_RANGESHFT) | IOMMU_CTL_ME;
	sc->sc_reg->io_bar = (kernel_iopte_table_pa >> 4) & IOMMU_BAR_IBA;

	IOMMU_FLUSHALL(sc);
	splx(s);

	printf(": version 0x%x/0x%x, page-size %d, range %dMB\n",
		(sc->sc_reg->io_cr & IOMMU_CTL_VER) >> 24,
		(sc->sc_reg->io_cr & IOMMU_CTL_IMPL) >> 28,
		sc->sc_pagesize,
		sc->sc_range >> 20);

	iommu_dvmamap = extent_create("iommudvma",
					IOMMU_DVMA_BASE, IOMMU_DVMA_END,
					M_DEVBUF, 0, 0, EX_NOWAIT);
	if (iommu_dvmamap == NULL)
		panic("iommu: unable to allocate DVMA map");

	/*
	 * Loop through ROM children (expect Sbus among them).
	 */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		struct iommu_attach_args ia;

		bzero(&ia, sizeof ia);
		ia.iom_name = getpropstring(node, "name");

		/* Propagate BUS & DMA tags */
		ia.iom_bustag = ma->ma_bustag;
		ia.iom_dmatag = &iommu_dma_tag;

		ia.iom_node = node;

		ia.iom_reg = NULL;
		getprop(node, "reg", sizeof(struct sbus_reg),
			&ia.iom_nreg, (void **)&ia.iom_reg);

		(void) config_found(&sc->sc_dev, (void *)&ia, iommu_print);
		if (ia.iom_reg != NULL)
			free(ia.iom_reg, M_DEVBUF);
	}
#endif
}

void
iommu_enter(dva, pa)
	bus_addr_t dva;
	paddr_t pa;
{
	struct iommu_softc *sc = iommu_sc;
	int pte;

	/* This routine relies on the fact that sc->sc_pagesize == PAGE_SIZE */

#ifdef DIAGNOSTIC
	if (dva < sc->sc_dvmabase)
		panic("iommu_enter: dva 0x%lx not in DVMA space", (long)dva);
#endif

	pte = atop(pa) << IOPTE_PPNSHFT;
	pte &= IOPTE_PPN;
	pte |= IOPTE_V | IOPTE_W | (has_iocache ? IOPTE_C : 0);
	sc->sc_ptes[atop(dva - sc->sc_dvmabase)] = pte;
	IOMMU_FLUSHPAGE(sc, dva);
}

/*
 * iommu_clear: clears mappings created by iommu_enter
 */
void
iommu_remove(va, len)
	bus_addr_t va;
	bus_size_t len;
{
	struct iommu_softc *sc = iommu_sc;
	u_int pagesz = sc->sc_pagesize;
	bus_addr_t base = sc->sc_dvmabase;

#ifdef DEBUG
	if (va < base)
		panic("iommu_enter: va 0x%lx not in DVMA space", (long)va);
#endif

	while ((long)len > 0) {
#ifdef notyet
#ifdef DEBUG
		if ((sc->sc_ptes[atop(va - base)] & IOPTE_V) == 0)
			panic("iommu_clear: clearing invalid pte at va 0x%lx",
			      (long)va);
#endif
#endif
		sc->sc_ptes[atop(va - base)] = 0;
		IOMMU_FLUSHPAGE(sc, va);
		len -= pagesz;
		va += pagesz;
	}
}

#if 0	/* These registers aren't there??? */
void
iommu_error()
{
	struct iommu_softc *sc = X;
	struct iommureg *iop = sc->sc_reg;

	printf("iommu: afsr 0x%x, afar 0x%x\n", iop->io_afsr, iop->io_afar);
	printf("iommu: mfsr 0x%x, mfar 0x%x\n", iop->io_mfsr, iop->io_mfar);
}
int
iommu_alloc(va, len)
	u_int va, len;
{
	struct iommu_softc *sc = X;
	int off, tva, iovaddr, pte;
	paddr_t pa;

	off = (int)va & PGOFSET;
	len = round_page(len + off);
	va -= off;

if ((int)sc->sc_dvmacur + len > 0)
	sc->sc_dvmacur = sc->sc_dvmabase;

	iovaddr = tva = sc->sc_dvmacur;
	sc->sc_dvmacur += len;
	while (len) {
		(void) pmap_extract(pmap_kernel(), va, &pa);

#define IOMMU_PPNSHIFT	8
#define IOMMU_V		0x00000002
#define IOMMU_W		0x00000004

		pte = atop(pa) << IOMMU_PPNSHIFT;
		pte |= IOMMU_V | IOMMU_W;
		sta(sc->sc_ptes + atop(tva - sc->sc_dvmabase), ASI_BYPASS, pte);
		sc->sc_reg->io_flushpage = tva;
		len -= NBPG;
		va += NBPG;
		tva += NBPG;
	}
	return iovaddr + off;
}
#endif


/*
 * Internal routine to allocate space in the IOMMU map.
 */
int
iommu_dvma_alloc(map, va, len, flags, dvap, sgsizep)
	bus_dmamap_t map;
	vaddr_t va;
	bus_size_t len;
	int flags;
	bus_addr_t *dvap;
	bus_size_t *sgsizep;
{
	bus_size_t sgsize;
	u_long align, voff;
	u_long ex_start, ex_end;
	int s, error;
	int pagesz = PAGE_SIZE;

	/*
	 * Remember page offset, then truncate the buffer address to
	 * a page boundary.
	 */
	voff = va & (pagesz - 1);
	va &= -pagesz;

	if (len > map->_dm_size)
		return (EINVAL);

	sgsize = (len + voff + pagesz - 1) & -pagesz;
	align = dvma_cachealign ? dvma_cachealign : pagesz;

	s = splhigh();

	/* Check `24 address bits' in the map's attributes */
	if ((map->_dm_flags & BUS_DMA_24BIT) != 0) {
		ex_start = D24_DVMA_BASE;
		ex_end = D24_DVMA_END;
	} else {
		ex_start = iommu_dvmamap->ex_start;
		ex_end = iommu_dvmamap->ex_end;
	}
	error = extent_alloc_subregion1(iommu_dvmamap,
					ex_start, ex_end,
					sgsize, align, va & (align-1),
					map->_dm_boundary,
					(flags & BUS_DMA_NOWAIT) == 0
						? EX_WAITOK : EX_NOWAIT,
					(u_long *)dvap);
	splx(s);

	*sgsizep = sgsize;
	return (error);
}

/*
 * IOMMU DMA map functions.
 */
int
iommu_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	bus_size_t sgsize;
	bus_addr_t dva;
	vaddr_t va = (vaddr_t)buf;
	int pagesz = PAGE_SIZE;
	pmap_t pmap;
	int error;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	/* Allocate IOMMU resources */
	if ((error = iommu_dvma_alloc(map, va, buflen, flags,
					&dva, &sgsize)) != 0)
		return (error);

	cpuinfo.cache_flush(buf, buflen); /* XXX - move to bus_dma_sync? */

	/*
	 * We always use just one segment.
	 */
	map->dm_mapsize = buflen;
	map->dm_nsegs = 1;
	map->dm_segs[0].ds_addr = dva + (va & (pagesz - 1));
	map->dm_segs[0].ds_len = buflen;
	map->dm_segs[0]._ds_sgsize = sgsize;

	if (p != NULL)
		pmap = p->p_vmspace->vm_map.pmap;
	else
		pmap = pmap_kernel();

	for (; sgsize != 0; ) {
		paddr_t pa;
		/*
		 * Get the physical address for this page.
		 */
		(void) pmap_extract(pmap, va, &pa);

		iommu_enter(dva, pa);

		dva += pagesz;
		va += pagesz;
		sgsize -= pagesz;
	}

	return (0);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
iommu_dmamap_load_mbuf(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{

	panic("_bus_dmamap_load_mbuf: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
iommu_dmamap_load_uio(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{

	panic("_bus_dmamap_load_uio: not implemented");
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
iommu_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{
	vm_page_t m;
	paddr_t pa;
	bus_addr_t dva;
	bus_size_t sgsize;
	struct pglist *mlist;
	int pagesz = PAGE_SIZE;
	int error;

	map->dm_nsegs = 0;

	/* Allocate IOMMU resources */
	if ((error = iommu_dvma_alloc(map, segs[0]._ds_va, size,
				      flags, &dva, &sgsize)) != 0)
		return (error);

	/*
	 * Note DVMA address in case bus_dmamem_map() is called later.
	 * It can then insure cache coherency by choosing a KVA that
	 * is aligned to `ds_addr'.
	 */
	segs[0].ds_addr = dva;
	segs[0].ds_len = size;

	map->dm_segs[0].ds_addr = dva;
	map->dm_segs[0].ds_len = size;
	map->dm_segs[0]._ds_sgsize = sgsize;

	/* Map physical pages into IOMMU */
	mlist = segs[0]._ds_mlist;
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {
		if (sgsize == 0)
			panic("iommu_dmamap_load_raw: size botch");
		pa = VM_PAGE_TO_PHYS(m);
		iommu_enter(dva, pa);
		dva += pagesz;
		sgsize -= pagesz;
	}

	map->dm_nsegs = 1;
	map->dm_mapsize = size;

	return (0);
}

/*
 * Unload an IOMMU DMA map.
 */
void
iommu_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	bus_dma_segment_t *segs = map->dm_segs;
	int nsegs = map->dm_nsegs;
	bus_addr_t dva;
	bus_size_t len;
	int i, s, error;

	for (i = 0; i < nsegs; i++) {
		dva = segs[i].ds_addr & -PAGE_SIZE;
		len = segs[i]._ds_sgsize;

		iommu_remove(dva, len);
		s = splhigh();
		error = extent_free(iommu_dvmamap, dva, len, EX_NOWAIT);
		splx(s);
		if (error != 0)
			printf("warning: %ld of DVMA space lost\n", (long)len);
	}

	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;
}

/*
 * DMA map synchronization.
 */
void
iommu_dmamap_sync(t, map, offset, len, ops)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{

	/*
	 * XXX Should flush CPU write buffers.
	 */
}

/*
 * Map DMA-safe memory.
 */
int
iommu_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vm_page_t m;
	vaddr_t va;
	bus_addr_t addr;
	struct pglist *mlist;
	int cbit;
	u_long align;
	int pagesz = PAGE_SIZE;

	if (nsegs != 1)
		panic("iommu_dmamem_map: nsegs = %d", nsegs);

	cbit = has_iocache ? 0 : PMAP_NC;
	align = dvma_cachealign ? dvma_cachealign : pagesz;

	size = round_page(size);

	/*
	 * In case the segment has already been loaded by
	 * iommu_dmamap_load_raw(), find a region of kernel virtual
	 * addresses that can accomodate our aligment requirements.
	 */
	va = _bus_dma_valloc_skewed(size, 0, align,
				    segs[0].ds_addr & (align - 1));
	if (va == 0)
		return (ENOMEM);

	segs[0]._ds_va = va;
	*kvap = (caddr_t)va;

	/*
	 * Map the pages allocated in _bus_dmamem_alloc() to the
	 * kernel virtual address space.
	 */
	mlist = segs[0]._ds_mlist;
	for (m = TAILQ_FIRST(mlist); m != NULL; m = TAILQ_NEXT(m,pageq)) {

		if (size == 0)
			panic("iommu_dmamem_map: size botch");

		addr = VM_PAGE_TO_PHYS(m);
		pmap_enter(pmap_kernel(), va, addr | cbit,
		    VM_PROT_READ | VM_PROT_WRITE,
		    VM_PROT_READ | VM_PROT_WRITE | PMAP_WIRED);
#if 0
			if (flags & BUS_DMA_COHERENT)
				/* XXX */;
#endif
		va += pagesz;
		size -= pagesz;
	}

	return (0);
}

/*
 * mmap(2)'ing DMA-safe memory.
 */
int
iommu_dmamem_mmap(t, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs, off, prot, flags;
{

	panic("_bus_dmamem_mmap: not implemented");
}
