#ifndef	_PMAP_MACHINE_
#define	_PMAP_MACHINE_

/*
 * Pmap stuff
 * [some ideas borrowed from torek, but no code]
 */

struct context_state {
    queue_chain_t  context_link;    
    int            context_num;
    struct pmap   *context_upmap;
};

typedef struct context_state *context_t;

struct pmap {
    int		                pm_refcount;	/* pmap reference count */
    simple_lock_data_t	        pm_lock;	/* lock on pmap */
    struct pmap_statistics	pm_stats;	/* pmap statistics */
    context_t                   pm_context;     /* context if any */
    int                         pm_version;
    unsigned char               *pm_segmap;
};

typedef struct pmap *pmap_t;

extern pmap_t kernel_pmap;

struct pmeg_state {
    queue_chain_t  pmeg_link;
    int            pmeg_index;
    pmap_t         pmeg_owner;
    int            pmeg_owner_version;
    vm_offset_t    pmeg_va;
    int            pmeg_wired_count;
    int            pmeg_reserved;
    int            pmeg_vpages;
};

typedef struct pmeg_state *pmeg_t;

#define PMAP_ACTIVATE(pmap, pcbp, iscurproc) \
      pmap_activate(pmap, pcbp, iscurproc);
#define PMAP_DEACTIVATE(pmap, pcbp) \
      pmap_deactivate(pmap, pcbp)

#define	pmap_kernel()			(kernel_pmap)

/* like the sparc port, use the lower bits of a pa which must be page
 *  aligned anyway to pass memtype, caching information.
 */
#define PMAP_MMEM      0x0
#define PMAP_OBIO      0x1
#define PMAP_VME16D    0x2
#define PMAP_VME32D    0x3
#define PMAP_MEMMASK   0x3
#define PMAP_NC        0x4
#define PMAP_SPECMASK  0x7

#endif	_PMAP_MACHINE_
