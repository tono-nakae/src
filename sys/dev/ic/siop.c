/*	$NetBSD: siop.c,v 1.9 2000/05/04 17:18:27 bouyer Exp $	*/

/*
 * Copyright (c) 2000 Manuel Bouyer.
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
 *	This product includes software developed by Manuel Bouyer
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* SYM53c7/8xx PCI-SCSI I/O Processors driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/scsiio.h>

#include <machine/endian.h>
#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>

#include <dev/microcode/siop/siop.out>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsi_message.h>
#include <dev/scsipi/scsipi_all.h>

#include <dev/scsipi/scsiconf.h>

#include <dev/ic/siopreg.h>
#include <dev/ic/siopvar.h>

#undef DEBUG
#undef DEBUG_DR
#undef DEBUG_INTR
#undef DEBUG_SHED
#undef DUMP_SCRIPT

#define SIOP_STATS

#ifndef SIOP_DEFAULT_TARGET
#define SIOP_DEFAULT_TARGET 7
#endif

#define MEM_SIZE 8192
#define CMD_OFF 4096

/* tables used by SCRIPT */
typedef struct scr_table {
	u_int32_t count;
	u_int32_t addr;
} scr_table_t ;

/* Number of scatter/gather entries */
#define SIOP_NSG	(MAXPHYS/NBPG + 1)

/*
 * This structure interfaces the SCRIPT with the driver; it describes a full
 * transfert. It lives in the same chunk of DMA-safe memory as the script.
 */
struct siop_xfer {
	u_int8_t msg_out[8];	/* 0 */
	u_int8_t msg_in[8];	/* 8 */
	int status;		/* 16 */
	u_int32_t pad1;		/* 20 */
	u_int32_t id;		/* 24 */
	u_int32_t pad2;		/* 28 */
	scr_table_t t_msgin;	/* 32 */
	scr_table_t t_extmsgin;	/* 40 */
	scr_table_t t_extmsgdata; /* 48 */
	scr_table_t t_extmsgtag; /* 56 */
	scr_table_t t_msgout;	/* 64 */
	scr_table_t cmd;	/* 72 */
	scr_table_t t_status;	/* 80 */
	scr_table_t data[SIOP_NSG]; /* 88 */
} __attribute__((__packed__));

/*
 * This decribes a command handled by the SCSI controller
 * These are chained in either a free list or a active list
 * We have one queue per target + (one at the adapter's target for probe)
 */
struct siop_cmd {
	TAILQ_ENTRY (siop_cmd) next;
	struct siop_target *siop_target; /* pointer to our target def */
	struct scsipi_xfer *xs; /* xfer from the upper level */
	struct siop_xfer *siop_table; /* tables dealing with this xfer */
	bus_addr_t	dsa; /* DSA value to load */
	bus_dmamap_t	dmamap_cmd;
	bus_dmamap_t	dmamap_data;
	struct scsipi_sense rs_cmd; /* request sense command buffer */
	int       status;
	int       flags;
};

/* status defs */
#define CMDST_FREE		0 /* cmd slot is free */
#define CMDST_READY		1 /* cmd slot is waiting for processing */
#define CMDST_ACTIVE		2 /* cmd slot is being processed */
#define CMDST_SENSE		3 /* cmd slot is being requesting sense */
#define CMDST_SENSE_ACTIVE	4 /* request sense active */
#define CMDST_SENSE_DONE 	5 /* request sense done */
#define CMDST_DONE		6 /* cmd slot has been processed */
/* flags defs */
#define CMDFL_TIMEOUT	0x0001 /* cmd timed out */

/* initial number of cmd descriptors */
#define SIOP_NCMD 10

/* per-target struct */
struct siop_target {
	int status;	/* target status see below */
	int flags;	/* target flags, see below */
	u_int32_t id;	/* for SELECT FROM */
	struct cmd_list active_list[8]; /* per-lun active cmds */
	struct siop_softc *siop_sc; /* points back to our adapter */
};

/* target status */
#define TARST_PROBING	0 /* target is being probed */
#define TARST_ASYNC	1 /* target needs sync/wide negotiation */
#define TARST_WIDE_NEG	2 /* target is doing wide negotiation */
#define TARST_SYNC_NEG	3 /* target is doing sync negotiation */
#define TARST_OK	4 /* sync/wide agreement is valid */

/* target flags */
#define TARF_SYNC	0x00 /* target is sync */
#define TARF_WIDE	0x01 /* target is wide */

void	siop_reset __P((struct siop_softc *));
void	siop_handle_reset __P((struct siop_softc *));
void	siop_scsicmd_end __P((struct siop_cmd *));
void	siop_wdtr_neg __P((struct siop_cmd *siop_cmd));
void	siop_sdtr_neg __P((struct siop_cmd *siop_cmd));
void	siop_start __P((struct siop_softc *));
void 	siop_timeout __P((void *));
void	siop_minphys __P((struct buf *));
int	siop_ioctl __P((struct scsipi_link *, u_long,
		caddr_t, int, struct proc *));
int	siop_scsicmd __P((struct scsipi_xfer *));
void 	siop_sdp __P((struct siop_cmd *));
void	siop_clearfifo __P((struct siop_softc *));
void 	siop_ssg __P((struct siop_cmd *));
void	siop_dump_script __P((struct siop_softc *));

struct scsipi_adapter siop_adapter = {
	0,
	siop_scsicmd,
	siop_minphys,
	siop_ioctl,
	NULL,
};

struct scsipi_device siop_dev = {
	NULL,
	NULL,
	NULL,
	NULL,
};

#ifdef SIOP_STATS
static int siop_stat_intr = 0;
static int siop_stat_intr_shortxfer = 0;
static int siop_stat_intr_sdp = 0;
static int siop_stat_intr_done = 0;
static int siop_stat_intr_reselect = 0;
static int siop_stat_intr_xferdisc = 0;
void siop_printstats __P((void));
#define INCSTAT(x) x++
#else
#define INCSTAT(x) 
#endif

static __inline__ void siop_table_sync __P((struct siop_cmd *, int));
static __inline__ void
siop_table_sync(siop_cmd, ops)
	struct siop_cmd *siop_cmd;
	int ops;
{
	struct siop_softc *sc  = siop_cmd->siop_target->siop_sc;
	bus_addr_t offset;
	
	offset = siop_cmd->dsa - sc->sc_scriptdma->dm_segs[0].ds_addr;
	bus_dmamap_sync(sc->sc_dmat, sc->sc_scriptdma, offset,
	    sizeof(struct siop_xfer), ops);
}

static __inline__ void siop_script_sync __P((struct siop_softc *, int));
static __inline__ void
siop_script_sync(sc, ops)
	struct siop_softc *sc;
	int ops;
{
	bus_dmamap_sync(sc->sc_dmat, sc->sc_scriptdma, 0, CMD_OFF, ops);
}

void
siop_attach(sc)
	struct siop_softc *sc;
{
	int error, i;
	bus_dma_segment_t seg;
	int rseg;

	/*
	 * Allocate DMA-safe memory for the script itself and internal
	 * variables and map it.
	 */
	error = bus_dmamem_alloc(sc->sc_dmat, MEM_SIZE, 
	    NBPG, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to allocate script DMA memory, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	error = bus_dmamem_map(sc->sc_dmat, &seg, rseg, MEM_SIZE,
	    (caddr_t *)&sc->sc_script, BUS_DMA_NOWAIT|BUS_DMA_COHERENT);
	if (error) {
		printf("%s: unable to map script DMA memory, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	error = bus_dmamap_create(sc->sc_dmat, MEM_SIZE, 1,
	    MEM_SIZE, 0, BUS_DMA_NOWAIT, &sc->sc_scriptdma);
	if (error) {
		printf("%s: unable to create script DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	error = bus_dmamap_load(sc->sc_dmat, sc->sc_scriptdma, sc->sc_script,
	    MEM_SIZE, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to load script DMA map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	TAILQ_INIT(&sc->free_list);
	/* allocate cmd list */
	sc->cmds =
	    malloc(sizeof(struct siop_cmd) * SIOP_NCMD, M_DEVBUF, M_NOWAIT);
	if (sc->cmds == NULL) {
		printf("%s: can't allocate memory for command descriptors\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	for (i = 0; i < SIOP_NCMD; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MAXPHYS, SIOP_NSG,
		    MAXPHYS, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &sc->cmds[i].dmamap_data);
		if (error) {
			printf("%s: unable to create data DMA map for cbd %d\n",
			    sc->sc_dev.dv_xname, error);
			return;
		}
		error = bus_dmamap_create(sc->sc_dmat,
		    sizeof(struct scsipi_generic), 1,
		    sizeof(struct scsipi_generic), 0,
		    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &sc->cmds[i].dmamap_cmd);
		if (error) {
			printf("%s: unable to create cmd DMA map for cbd %d\n",
			    sc->sc_dev.dv_xname, error);
			return;
		}
		sc->cmds[i].siop_table =
		    &((struct siop_xfer *)(&sc->sc_script[CMD_OFF/4]))[i];
		memset(sc->cmds[i].siop_table, 0, sizeof(struct siop_xfer));
		sc->cmds[i].dsa = sc->sc_scriptdma->dm_segs[0].ds_addr +
		    CMD_OFF + i * sizeof(struct siop_xfer);
		sc->cmds[i].status = CMDST_FREE;
		sc->cmds[i].siop_table->t_msgout.count= htole32(1);
		sc->cmds[i].siop_table->t_msgout.addr =
		    htole32(sc->cmds[i].dsa);
		sc->cmds[i].siop_table->t_msgin.count= htole32(1);
		sc->cmds[i].siop_table->t_msgin.addr =
		    htole32(sc->cmds[i].dsa + 8);
		sc->cmds[i].siop_table->t_extmsgin.count= htole32(2);
		sc->cmds[i].siop_table->t_extmsgin.addr =
		    htole32(le32toh(sc->cmds[i].siop_table->t_msgin.addr) + 1);
		sc->cmds[i].siop_table->t_status.count= htole32(1);
		sc->cmds[i].siop_table->t_status.addr =
		    htole32(le32toh(sc->cmds[i].siop_table->t_msgin.addr) + 8);
		TAILQ_INSERT_TAIL(&sc->free_list, &sc->cmds[i], next);
#ifdef DEBUG
		printf("tables[%d]: out=0x%x in=0x%x status=0x%x\n", i,
		    le32toh(sc->cmds[i].siop_table->t_msgin.addr),
		    le32toh(sc->cmds[i].siop_table->t_msgout.addr),
		    le32toh(sc->cmds[i].siop_table->t_status.addr));
#endif
	}
	/* compute number of sheduler slots */
	sc->sc_nshedslots = (
	    CMD_OFF /* memory size allocated for scripts */
	    - sizeof(siop_script) /* memory for main script */
	    + 8		/* extra NOP at end of main script */
	    - sizeof(endslot_script) /* memory needed at end of sheduler */
	    ) / (sizeof(slot_script) - 8);
	sc->sc_currshedslot = 0;
#ifdef DEBUG
	printf("%s: script size = %d, PHY addr=0x%x, VIRT=%p nslots %d\n",
	    sc->sc_dev.dv_xname, (int)sizeof(siop_script),
	    (u_int)sc->sc_scriptdma->dm_segs[0].ds_addr, sc->sc_script,
	    sc->sc_nshedslots);
#endif

	sc->sc_link.adapter_softc = sc;
	sc->sc_link.openings = 1;
	sc->sc_link.scsipi_scsi.channel = SCSI_CHANNEL_ONLY_ONE;
	sc->sc_link.scsipi_scsi.max_target  =
	    (sc->features & SF_BUS_WIDE) ? 15 : 7;
	sc->sc_link.scsipi_scsi.max_lun = 7;
	sc->sc_link.scsipi_scsi.adapter_target = bus_space_read_1(sc->sc_rt,
	    sc->sc_rh, SIOP_SCID);
	if (sc->sc_link.scsipi_scsi.adapter_target == 0 ||
	    sc->sc_link.scsipi_scsi.adapter_target >
	    sc->sc_link.scsipi_scsi.max_target)
		sc->sc_link.scsipi_scsi.adapter_target = SIOP_DEFAULT_TARGET;
	sc->sc_link.type = BUS_SCSI;
	sc->sc_link.adapter = &siop_adapter;
	sc->sc_link.device = &siop_dev;
	sc->sc_link.flags  = 0;

	for (i = 0; i < 16; i++)
		sc->targets[i] = NULL;

	/* find min/max sync period for this chip */
	sc->maxsync = 0;
	sc->minsync = 255;
	for (i = 0; i < sizeof(scf_period) / sizeof(scf_period[0]); i++) {
		if (sc->clock_period != scf_period[i].clock)
			continue;
		if (sc->maxsync < scf_period[i].period)
			sc->maxsync = scf_period[i].period;
		if (sc->minsync > scf_period[i].period)
			sc->minsync = scf_period[i].period;
	}
	if (sc->maxsync == 255 || sc->minsync == 0)
		panic("siop: can't find my sync parameters\n");
	siop_reset(sc);
#ifdef DUMP_SCRIPT
	siop_dump_script(sc);
#endif

	config_found((struct device*)sc, &sc->sc_link, scsiprint);
}

void
siop_reset(sc)
	struct siop_softc *sc;
{
	int i, j;
	u_int32_t *scr;
	u_int32_t stest3;
	bus_addr_t physaddr;

	/* reset the chip */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_SRST);
	delay(1000);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, 0);

	/* copy and patch the script */
	for (j = 0; j < (sizeof(siop_script) / sizeof(siop_script[0])); j++) {
		sc->sc_script[j] = htole32(siop_script[j]);
	}
	/* copy the sheduler slots script */
	for (i = 0; i < sc->sc_nshedslots; i++) {
		scr = &sc->sc_script[Ent_sheduler / 4 + (Ent_nextslot / 4) * i];
		physaddr = sc->sc_scriptdma->dm_segs[0].ds_addr + Ent_sheduler
		    + Ent_nextslot * i;
		for (j = 0; j < (sizeof(slot_script) / sizeof(slot_script[0]));
		    j++) {
			scr[j] = htole32(slot_script[j]);
		}
		/*
		 * save current jump offset and patch MOVE MEMORY operands
		 * to restore it.
		 */
		scr[Ent_slotdata/4 + 1] = scr[Ent_slot/4 + 1];
		scr[E_slot_nextp_Used[0]] = htole32(physaddr + Ent_slot + 4);
		scr[E_slot_shed_addrsrc_Used[0]] = htole32(physaddr +
		    Ent_slotdata + 4);
		/* JUMP selected, in main script */
		scr[E_slot_abs_selected_Used[0]] =
		   htole32(sc->sc_scriptdma->dm_segs[0].ds_addr + Ent_selected);
		/* JUMP addr if SELECT fail */
		scr[E_slot_abs_reselect_Used[0]] = 
		   htole32(sc->sc_scriptdma->dm_segs[0].ds_addr + Ent_reselect);
	}
	/* Now the final JUMP */
	scr = &sc->sc_script[Ent_sheduler / 4 +
	    (Ent_nextslot / 4) * sc->sc_nshedslots];
	for (j = 0; j < (sizeof(endslot_script) / sizeof(endslot_script[0]));
	    j++) {
		scr[j] = htole32(endslot_script[j]);
	}
	scr[E_endslot_abs_reselect_Used[0]] = 
	    htole32(sc->sc_scriptdma->dm_segs[0].ds_addr + Ent_reselect);

	/* init registers */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL0,
	    SCNTL0_ARB_MASK | SCNTL0_EPC | SCNTL0_AAP);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, 0);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3, sc->clock_div);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DIEN, 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SIEN0,
	    0xff & ~(SIEN0_CMP | SIEN0_SEL | SIEN0_RSL));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SIEN1,
	    0xff & ~(SIEN1_HTH | SIEN1_GEN));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST2, STEST2_EXT);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3, STEST3_TE);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STIME0,
	    (0xb << STIME0_SEL_SHIFT));
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCID,
	    sc->sc_link.scsipi_scsi.adapter_target | SCID_RRE);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_RESPID0,
	    1 << sc->sc_link.scsipi_scsi.adapter_target);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_DCNTL, DCNTL_COM);

	/* enable clock doubler or quadruler if appropriate */
	if (sc->features & (SF_CHIP_DBLR | SF_CHIP_QUAD)) {
		stest3 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1,
		    STEST1_DBLEN);
		if (sc->features & SF_CHIP_QUAD) {
			/* wait for PPL to lock */
			while ((bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_STEST4) & STEST4_LOCK) == 0)
				delay(10);
		} else {
			/* data sheet says 20us - more won't hurt */
			delay(100);
		}
		/* halt scsi clock, select doubler/quad, restart clock */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3,
		    stest3 | STEST3_HSC);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1,
		    STEST1_DBLEN | STEST1_DBLSEL);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST3, stest3);
	} else {
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_STEST1, 0);
	}

	/* start script */
	siop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP,
	    sc->sc_scriptdma->dm_segs[0].ds_addr + Ent_reselect);
}

#if 0
#define CALL_SCRIPT(ent) do {\
	printf ("start script DSA 0x%lx DSP 0x%lx\n", \
	    siop_cmd->dsa, \
	    sc->sc_scriptdma->dm_segs[0].ds_addr + ent); \
bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP, sc->sc_scriptdma->dm_segs[0].ds_addr + ent); \
} while (0)
#else
#define CALL_SCRIPT(ent) do {\
bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP, sc->sc_scriptdma->dm_segs[0].ds_addr + ent); \
} while (0)
#endif

int
siop_intr(v)
	void *v;
{
	struct siop_softc *sc = v;
	struct siop_target *siop_target;
	struct siop_cmd *siop_cmd;
	struct scsipi_xfer *xs;
	int istat, sist0, sist1, sstat1, dstat, scntl1;
	u_int32_t irqcode;
	int need_reset = 0;
	int offset, target, lun;
	bus_addr_t dsa;

	istat = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT);
	if ((istat & (ISTAT_INTF | ISTAT_DIP | ISTAT_SIP)) == 0)
		return 0;
	INCSTAT(siop_stat_intr);
	if (istat & ISTAT_INTF) {
		printf("INTRF\n");
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_INTF);
	}
	/* use DSA to find the current siop_cmd */
	dsa = bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA);
	if (dsa >= sc->sc_scriptdma->dm_segs[0].ds_addr + CMD_OFF &&
	    dsa < sc->sc_scriptdma->dm_segs[0].ds_addr + CMD_OFF +
	       SIOP_NCMD * sizeof(struct siop_xfer)) {
		dsa -= sc->sc_scriptdma->dm_segs[0].ds_addr + CMD_OFF;
		siop_cmd = &sc->cmds[dsa / sizeof(struct siop_xfer)];
		siop_table_sync(siop_cmd,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	} else {
		printf("%s: current DSA invalid\n",
		    sc->sc_dev.dv_xname);
		siop_cmd = NULL;
	}
	if (istat & ISTAT_DIP) {
		u_int32_t *p;
		dstat = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_DSTAT);
		if (dstat & DSTAT_SSI) {
			printf("single step dsp 0x%08x dsa 0x08%x\n",
			    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
			    sc->sc_scriptdma->dm_segs[0].ds_addr),
			    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA));
			if ((dstat & ~(DSTAT_DFE | DSTAT_SSI)) == 0 &&
			    (istat & ISTAT_SIP) == 0) {
				bus_space_write_1(sc->sc_rt, sc->sc_rh,
				    SIOP_DCNTL, bus_space_read_1(sc->sc_rt,
				    sc->sc_rh, SIOP_DCNTL) | DCNTL_STD);
			}
			return 1;
		}
		if (dstat & ~(DSTAT_SIR | DSTAT_DFE | DSTAT_SSI)) {
		printf("DMA IRQ:");
		if (dstat & DSTAT_IID)
			printf(" Illegal instruction");
		if (dstat & DSTAT_ABRT)
			printf(" abort");
		if (dstat & DSTAT_BF)
			printf(" bus fault");
		if (dstat & DSTAT_MDPE)
			printf(" parity");
		if (dstat & DSTAT_DFE)
			printf(" dma fifo empty");
		printf(", DSP=0x%x DSA=0x%x: ",
		    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
		    sc->sc_scriptdma->dm_segs[0].ds_addr),
		    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA));
		p = sc->sc_script +
		    (bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
		    sc->sc_scriptdma->dm_segs[0].ds_addr - 8) / 4;
		printf("0x%x 0x%x 0x%x 0x%x\n", le32toh(p[0]), le32toh(p[1]),
		    le32toh(p[2]), le32toh(p[3]));
		if (siop_cmd)
			printf("last msg_in=0x%x status=0x%x\n",
			    siop_cmd->siop_table->msg_in[0],
			    le32toh(siop_cmd->siop_table->status));
		need_reset = 1;
		}
	}
	if (istat & ISTAT_SIP) {
		/*
		 * SCSI interrupt. If current command is not active,
		 * we don't need siop_cmd
		 */
		if (siop_cmd->status != CMDST_ACTIVE &&
		    siop_cmd->status != CMDST_SENSE_ACTIVE) {
			siop_cmd = NULL;
		}
		if (istat & ISTAT_DIP)
			delay(10);
		sist0 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SIST0);
			delay(10);
		sist1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SIST1);
		sstat1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT1);
#ifdef DEBUG_INTR
		printf("scsi interrupt, sist0=0x%x sist1=0x%x sstat1=0x%x "
		    "DSA=0x%x DSP=0x%lx\n", sist0, sist1,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT1),
		    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA),
		    (u_long)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
		    sc->sc_scriptdma->dm_segs[0].ds_addr));
#endif
		if (siop_cmd) {
			xs = siop_cmd->xs;
			siop_target = siop_cmd->siop_target;
		}
		if (sist0 & SIST0_RST) {
			siop_handle_reset(sc);
			siop_start(sc);
			/* no table to flush here */
			return 1;
		}
		if (sist0 & SIST0_SGE) {
			if (siop_cmd)
				scsi_print_addr(xs->sc_link);
			else
				printf("%s:", sc->sc_dev.dv_xname);
			printf("scsi gross error\n");
			goto reset;
		}
		if ((sist0 & SIST0_MA) && need_reset == 0) {
			if (siop_cmd) { 
				int scratcha0;
				dstat = bus_space_read_1(sc->sc_rt, sc->sc_rh,
				    SIOP_DSTAT);
				/*
				 * first restore DSA, in case we were in a S/G
				 * operation.
				 */
				bus_space_write_4(sc->sc_rt, sc->sc_rh,
				    SIOP_DSA, siop_cmd->dsa);
				scratcha0 = bus_space_read_1(sc->sc_rt,
				    sc->sc_rh, SIOP_SCRATCHA);
				switch (sstat1 & SSTAT1_PHASE_MASK) {
				case SSTAT1_PHASE_STATUS:
				/*
				 * previous phase may be aborted for any reason
				 * ( for example, the target has less data to
				 * transfer than requested). Just go to status
				 * and the command should terminate.
				 */
					INCSTAT(siop_stat_intr_shortxfer);
					CALL_SCRIPT(Ent_status);
					if ((dstat & DSTAT_DFE) == 0)
						siop_clearfifo(sc);
					/* no table to flush here */
					return 1;
				case SSTAT1_PHASE_MSGIN:
					/*
					 * target may be ready to disconnect
					 * Save data pointers just in case.
					 */
					INCSTAT(siop_stat_intr_xferdisc);
					if (scratcha0 & A_flag_data)
						siop_sdp(siop_cmd);
					else if ((dstat & DSTAT_DFE) == 0)
						siop_clearfifo(sc);
					bus_space_write_1(sc->sc_rt, sc->sc_rh,
					    SIOP_SCRATCHA,
					    scratcha0 & ~A_flag_data);
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_msgin);
					return 1;
				}
				printf("%s: unexpected phase mismatch %d\n",
				    sc->sc_dev.dv_xname,
				    sstat1 & SSTAT1_PHASE_MASK);
			} else {
				printf("%s: phase mismatch without command\n",
				    sc->sc_dev.dv_xname);
			}
			need_reset = 1;
		}
		if (sist0 & SIST0_PAR) {
			/* parity error, reset */
			if (siop_cmd)
				scsi_print_addr(xs->sc_link);
			else
				printf("%s:", sc->sc_dev.dv_xname);
			printf("parity error\n");
			need_reset = 1;
		}
		if ((sist1 & SIST1_STO) && need_reset == 0) {
			/* selection time out, assume there's no device here */
			if (siop_cmd) {
				siop_cmd->status = CMDST_DONE;
				xs->error = XS_SELTIMEOUT;
				goto end;
			} else {
				printf("%s: selection timeout without "
				    "command\n", sc->sc_dev.dv_xname);
				need_reset = 1;
			}
		}
		if (sist0 & SIST0_UDC) {
			/*
			 * unexpected disconnect. Usually the target signals
			 * a fatal condition this way. Attempt to get sense.
			 */
			 if (siop_cmd)
				goto check_sense;
			printf("%s: unexpected disconnect without "
			    "command\n", sc->sc_dev.dv_xname);
			goto reset;
		}
		/* Else it's an unhandled exeption (for now). */
		printf("%s: unhandled scsi interrupt, sist0=0x%x sist1=0x%x "
		    "sstat1=0x%x DSA=0x%x DSP=0x%x\n", sc->sc_dev.dv_xname,
		    sist0, sist1,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT1),
		    bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSA),
		    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
		    sc->sc_scriptdma->dm_segs[0].ds_addr));
		if (siop_cmd) {
			siop_cmd->status = CMDST_DONE;
			xs->error = XS_SELTIMEOUT;
			goto end;
		}
		need_reset = 1;
	}
	if (need_reset) {
reset:
		/* fatal error, reset the bus */
		scntl1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1,
		    scntl1 | SCNTL1_RST);
		/* minimum 25 us, more time won't hurt */
		delay(100);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, scntl1);
		/* no table to flush here */
		return 1;
	}


	if ((istat & ISTAT_DIP) && (dstat & DSTAT_SIR)) { /* script interrupt */
		irqcode = bus_space_read_4(sc->sc_rt, sc->sc_rh,
		    SIOP_DSPS);
#ifdef DEBUG_INTR
		printf("script interrupt 0x%x\n", irqcode);
#endif
		/*
		 * an inactive command is only valid if it's a reselect
		 * interrupt: we'll change siop_cmd to point to the rigth one
		 * just here
		 */
		if (irqcode != A_int_resel &&
		    siop_cmd->status != CMDST_ACTIVE &&
		    siop_cmd->status != CMDST_SENSE_ACTIVE) {
			printf("%s: Aie, no command (IRQ code 0x%x current "
			    "status %d) !\n", sc->sc_dev.dv_xname,
			    irqcode, siop_cmd->status);
			xs = NULL;
		} else {
			xs = siop_cmd->xs;
			siop_target = siop_cmd->siop_target;
		}
		switch(irqcode) {
		case A_int_err:
			printf("error, DSP=0x%x\n",
			    (int)(bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DSP) -
			    sc->sc_scriptdma->dm_segs[0].ds_addr));
			if (xs) {
				xs->error = XS_SELTIMEOUT;
				goto end;
			} else {
				goto reset;
			}
		case A_int_msgin:
			if (siop_cmd->siop_table->msg_in[0] ==
			    MSG_MESSAGE_REJECT) {
				int msg, extmsg;
				if (siop_cmd->siop_table->msg_out[0] & 0x80) {
					/*
					 * message was part of a identify +
					 * something else. Identify shoudl't
					 * have been rejected.
					 */
					msg = siop_cmd->siop_table->msg_out[1];
					extmsg =
					    siop_cmd->siop_table->msg_out[3];
				} else {
					msg = siop_cmd->siop_table->msg_out[0];
					extmsg =
					    siop_cmd->siop_table->msg_out[2];
				}
				if (msg == MSG_MESSAGE_REJECT) {
					/* MSG_REJECT  for a MSG_REJECT  !*/
					if (xs)
						scsi_print_addr(xs->sc_link);
					else
						printf("%s: ",
						   sc->sc_dev.dv_xname);
					printf("our reject message was "
					    "rejected\n");
					goto reset;
				}
				if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_WDTR) {
					/* WDTR rejected, initiate sync */
					printf("%s: target %d using 8bit "
					    "transfers\n", sc->sc_dev.dv_xname,
					    xs->sc_link->scsipi_scsi.target);
					siop_target->status = TARST_SYNC_NEG;
					siop_cmd->siop_table->msg_out[0] =
					    MSG_EXTENDED;
					siop_cmd->siop_table->msg_out[1] =
					    MSG_EXT_SDTR_LEN;
					siop_cmd->siop_table->msg_out[2] =
					    MSG_EXT_SDTR;
					siop_cmd->siop_table->msg_out[3] =
					    sc->minsync;
					siop_cmd->siop_table->msg_out[4] =
					    sc->maxoff;
					siop_cmd->siop_table->t_msgout.count =
					    htole32(MSG_EXT_SDTR_LEN + 2);
					siop_cmd->siop_table->t_msgout.addr =
					    htole32(siop_cmd->dsa);
					siop_table_sync(siop_cmd,
					    BUS_DMASYNC_PREREAD |
					    BUS_DMASYNC_PREWRITE);
					CALL_SCRIPT(Ent_send_msgout);
					return 1;
				} else if (msg == MSG_EXTENDED &&
				    extmsg == MSG_EXT_SDTR) {
					/* sync rejected */
					printf("%s: target %d asynchronous\n",
					    sc->sc_dev.dv_xname,
					    xs->sc_link->scsipi_scsi.target);
					siop_cmd->siop_target->status =
					    TARST_OK;
					/* no table to flush here */
					CALL_SCRIPT(Ent_msgin_ack);
					return 1;
				}
				if (xs)
					scsi_print_addr(xs->sc_link);
				else
					printf("%s: ", sc->sc_dev.dv_xname);
				if (msg == MSG_EXTENDED) {
					printf("scsi message reject, extended "
					    "message sent was 0x%x\n", extmsg);
				} else {
					printf("scsi message reject, message "
					    "sent was 0x%x\n", msg);
				}
				/* no table to flush here */
				CALL_SCRIPT(Ent_msgin_ack);
				return 1;
			}
			if (xs)
				scsi_print_addr(xs->sc_link);
			else
				printf("%s: ", sc->sc_dev.dv_xname);
			printf("unhandled message 0x%x\n",
			    siop_cmd->siop_table->msg_in[0]);
			siop_cmd->siop_table->t_msgout.count= htole32(1);
			siop_cmd->siop_table->t_msgout.addr =
			    htole32(siop_cmd->dsa);
			siop_cmd->siop_table->msg_out[0] = MSG_MESSAGE_REJECT;
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return 1;
		case A_int_extmsgin:
#ifdef DEBUG_INTR
			printf("extended message: msg 0x%x len %d\n",
			    siop_cmd->siop_table->msg_in[2], 
			    siop_cmd->siop_table->msg_in[1]);
#endif
			if (siop_cmd->siop_table->msg_in[1] > 6)
				printf("%s: extended message too big (%d)\n",
				    sc->sc_dev.dv_xname,
				    siop_cmd->siop_table->msg_in[1]);
			siop_cmd->siop_table->t_extmsgdata.count =
			    htole32(siop_cmd->siop_table->msg_in[1] - 1);
			siop_cmd->siop_table->t_extmsgdata.addr = 
			    htole32(
			    le32toh(siop_cmd->siop_table->t_extmsgin.addr)
			    + 2);
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_get_extmsgdata);
			return 1;
		case A_int_extmsgdata:
#ifdef DEBUG_INTR
			{
			int i;
			printf("extended message: 0x%x, data:",
			    siop_cmd->siop_table->msg_in[2]);
			for (i = 3; i < 2 + siop_cmd->siop_table->msg_in[1];
			    i++)
				printf(" 0x%x",
				    siop_cmd->siop_table->msg_in[i]);
			printf("\n");
			}
#endif
			if (siop_cmd->siop_table->msg_in[2] == MSG_EXT_WDTR) {
				siop_wdtr_neg(siop_cmd);
				return(1);
			}
			if (siop_cmd->siop_table->msg_in[2] == MSG_EXT_SDTR) {
				siop_sdtr_neg(siop_cmd);
				return(1);
			}
			/* send a message reject */
			siop_cmd->siop_table->t_msgout.count =
			    htole32(1);
			siop_cmd->siop_table->t_msgout.addr =
			    htole32(siop_cmd->dsa);
			siop_cmd->siop_table->msg_out[0] =
			    MSG_MESSAGE_REJECT;
			siop_table_sync(siop_cmd,
			    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return 1;
		case A_int_resel: /* reselected */
			INCSTAT(siop_stat_intr_reselect);
			if ((siop_cmd->siop_table->msg_in[0] & 0x80) == 0) {
				printf("%s: reselect without identify (%d)\n",
				    sc->sc_dev.dv_xname,
				    siop_cmd->siop_table->msg_in[0]);
				goto reset;
			}
			target = bus_space_read_1(sc->sc_rt,
			    sc->sc_rh, SIOP_SCRATCHA);
			if ((target & 0x80) == 0) {
				printf("reselect without id (%d)\n", target);
				goto reset;
			}
			target &= 0x0f;
			lun = siop_cmd->siop_table->msg_in[0] & 0x07;
#ifdef DEBUG_DR
			printf("reselected by target %d lun %d\n",
			    target, lun);
#endif
			siop_cmd =
			    sc->targets[target]->active_list[lun].tqh_first;
			if (siop_cmd == NULL) {
				printf("%s: reselected without cmd\n",
				    sc->sc_dev.dv_xname);
				goto reset;
			}
			bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSA,
			    siop_cmd->dsa);
			/* no table to flush */
			CALL_SCRIPT(Ent_selected);
			return 1;
		case A_int_disc:
			INCSTAT(siop_stat_intr_sdp);
			offset = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SCRATCHA + 1);
#ifdef DEBUG_DR
			printf("disconnect offset %d\n", offset);
#endif
			if (offset > SIOP_NSG) {
				printf("%s: bad offset for disconnect (%d)\n",
				    sc->sc_dev.dv_xname, offset);
				goto reset;
			}
			/* 
			 * offset == SIOP_NSG may be a valid condition if
			 * we get a sdp when the xfer is done.
			 * Don't call memmove in this case.
			 */
			if (offset < SIOP_NSG) {
				memmove(&siop_cmd->siop_table->data[0],
				    &siop_cmd->siop_table->data[offset],
				    (SIOP_NSG - offset) * sizeof(scr_table_t));
				siop_table_sync(siop_cmd,
				    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			}
			CALL_SCRIPT(Ent_sheduler);
			return 1;
		case A_int_resfail:
			printf("reselect failed\n");
			CALL_SCRIPT(Ent_sheduler);
			return  1;
		case A_int_done:
			if (xs == NULL) {
				printf("%s: done without command, DSA=0x%lx\n",
				    sc->sc_dev.dv_xname, (u_long)siop_cmd->dsa);
				siop_cmd->status = CMDST_FREE;
				CALL_SCRIPT(Ent_sheduler);
				siop_start(sc);
				return 1;
			}
			if (siop_target->status == TARST_PROBING)
				siop_target->status = TARST_ASYNC;
#ifdef DEBUG_INTR
			printf("done, DSA=0x%lx target id 0x%x last msg "
			    "in=0x%x status=0x%x\n", (u_long)siop_cmd->dsa,
			    le32toh(siop_cmd->siop_table->id),
			    siop_cmd->siop_table->msg_in[0],
			    le32toh(siop_cmd->siop_table->status));
#endif
			INCSTAT(siop_stat_intr_done);
			if (siop_cmd->status == CMDST_SENSE_ACTIVE)
				siop_cmd->status = CMDST_SENSE_DONE;
			else
				siop_cmd->status = CMDST_DONE;
			switch(le32toh(siop_cmd->siop_table->status)) {
			case SCSI_OK:
				xs->error = (siop_cmd->status == CMDST_DONE) ?
				    XS_NOERROR : XS_SENSE;
				break;
			case SCSI_BUSY:
				xs->error = XS_BUSY;
				break;
			case SCSI_CHECK:
check_sense:
				if (siop_cmd->status == CMDST_SENSE_DONE) {
					/* request sense on a request sense ? */
					printf("request sense failed\n");
					xs->error = XS_DRIVER_STUFFUP;
				} else {
					siop_cmd->status = CMDST_SENSE;
				}
				break;
			case 0xff:
				/*
				 * the status byte was not updated, cmd was
				 * aborted
				 */
				xs->error = XS_SELTIMEOUT;
				break;
			default:
				xs->error = XS_DRIVER_STUFFUP;
			}
			goto end;
		default:
			printf("unknown irqcode %x\n", irqcode);
			xs->error = XS_SELTIMEOUT;
			goto end;
		}
		return 1;
	}
	/* We just should't get there */
	panic("siop_intr: I shouldn't be there !");
	return 1;
end:
	bus_space_write_4(sc->sc_rt, sc->sc_rh, SIOP_DSP,
	    sc->sc_scriptdma->dm_segs[0].ds_addr + Ent_sheduler);
	lun = siop_cmd->xs->sc_link->scsipi_scsi.lun;
	siop_scsicmd_end(siop_cmd);
	if (siop_cmd->status == CMDST_FREE) {
		TAILQ_REMOVE(&siop_target->active_list[lun],
		    siop_cmd, next);
		TAILQ_INSERT_TAIL(&sc->free_list, siop_cmd, next);
	}
	siop_start(sc);
	return 1;
}

void
siop_scsicmd_end(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct scsipi_xfer *xs = siop_cmd->xs;
	struct siop_softc *sc = siop_cmd->siop_target->siop_sc;

	if (siop_cmd->status != CMDST_SENSE_DONE &&
	    xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data, 0,
		    siop_cmd->dmamap_data->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_data);
	}
	bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_cmd);
	if (siop_cmd->status == CMDST_SENSE) {
		/* issue a request sense for this target */
		int error, i;
		siop_cmd->rs_cmd.opcode = REQUEST_SENSE;
		siop_cmd->rs_cmd.byte2 = xs->sc_link->scsipi_scsi.lun << 5;
		siop_cmd->rs_cmd.unused[0] = siop_cmd->rs_cmd.unused[1] = 0;
		siop_cmd->rs_cmd.length = sizeof(struct scsipi_sense_data);
		siop_cmd->rs_cmd.control = 0;
		siop_cmd->siop_table->status = htole32(0xff); /*invalid status*/
		siop_cmd->siop_table->t_msgout.count= htole32(1);
		siop_cmd->siop_table->t_msgout.addr = htole32(siop_cmd->dsa);
		siop_cmd->siop_table->msg_out[0] =
		    MSG_IDENTIFY(xs->sc_link->scsipi_scsi.lun, 1);
		error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_cmd,
		    &siop_cmd->rs_cmd, sizeof(struct scsipi_sense),
		    NULL, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to load cmd DMA map: %d",
			    sc->sc_dev.dv_xname, error);
			xs->error = XS_DRIVER_STUFFUP;
			goto out;
		}
		siop_cmd->siop_table->cmd.count = 
		    htole32(siop_cmd->dmamap_cmd->dm_segs[0].ds_len);
		siop_cmd->siop_table->cmd.addr =
		    htole32(siop_cmd->dmamap_cmd->dm_segs[0].ds_addr);
		error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_data,
		    &xs->sense.scsi_sense, sizeof(struct  scsipi_sense_data),
		    NULL, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to load sense DMA map: %d",
			    sc->sc_dev.dv_xname, error);
			xs->error = XS_DRIVER_STUFFUP;
			bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_cmd);
			goto out;
		}
		for (i = 0; i < siop_cmd->dmamap_data->dm_nsegs; i++) {
			siop_cmd->siop_table->data[i].count =
			    htole32(siop_cmd->dmamap_data->dm_segs[i].ds_len);
			siop_cmd->siop_table->data[i].addr =
			    htole32(siop_cmd->dmamap_data->dm_segs[i].ds_addr);
		}
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data, 0,
		    siop_cmd->dmamap_data->dm_mapsize, BUS_DMASYNC_PREREAD);
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_cmd, 0,
		    siop_cmd->dmamap_cmd->dm_mapsize, BUS_DMASYNC_PREWRITE);
		siop_table_sync(siop_cmd, BUS_DMASYNC_PREWRITE);
		return;
	} else if (siop_cmd->status == CMDST_SENSE_DONE) {
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data, 0,
		    siop_cmd->dmamap_data->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_data);
	}
out:
	callout_stop(&siop_cmd->xs->xs_callout);
	siop_cmd->status = CMDST_FREE;
	xs->xs_status |= XS_STS_DONE;
	xs->resid = 0;
	scsipi_done (xs);
}

void
siop_wdtr_neg(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct siop_softc *sc = siop_cmd->siop_target->siop_sc;
	struct siop_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->sc_link->scsipi_scsi.target;

	if (siop_target->status == TARST_WIDE_NEG) {
		/* we initiated wide negotiation */
		switch (siop_cmd->siop_table->msg_in[3]) {
		case MSG_EXT_WDTR_BUS_8_BIT:
			printf("%s: target %d using 8bit transfers\n",
			    sc->sc_dev.dv_xname, target);
			siop_target->flags &= ~SF_BUS_WIDE;
			sc->targets[target]->id &= ~(SCNTL3_EWS << 24);
			break;
		case MSG_EXT_WDTR_BUS_16_BIT:
			if (sc->features & SF_BUS_WIDE) {
				printf("%s: target %d using 16bit transfers\n",
				    sc->sc_dev.dv_xname, target);
				siop_target->flags |= TARF_WIDE;
				sc->targets[target]->id |= (SCNTL3_EWS << 24);
				break;
			}
		/* FALLTHROUH */
		default:
			/*
 			 * hum, we got more than what we can handle, shoudn't
			 * happen. Reject, and stay async
			 */
			siop_target->flags &= ~TARF_WIDE;
			siop_target->status = TARST_OK;
			printf("%s: rejecting invalid wide negotiation from "
			    "target %d (%d)\n", sc->sc_dev.dv_xname, target,
			    siop_cmd->siop_table->msg_in[3]);
			siop_cmd->siop_table->t_msgout.count= htole32(1);
			siop_cmd->siop_table->t_msgout.addr =
			    htole32(siop_cmd->dsa);
			siop_cmd->siop_table->msg_out[0] = MSG_MESSAGE_REJECT;
			siop_table_sync(siop_cmd,
				BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
			CALL_SCRIPT(Ent_send_msgout);
			return;
		}
		siop_cmd->siop_table->id =
		    htole32(sc->targets[target]->id);
		bus_space_write_1(sc->sc_rt, sc->sc_rh,
		    SIOP_SCNTL3,
		    (sc->targets[target]->id >> 24) & 0xff);
		/* we now need to do sync */
		siop_target->status = TARST_SYNC_NEG;
		siop_cmd->siop_table->msg_out[0] = MSG_EXTENDED;
		siop_cmd->siop_table->msg_out[1] = MSG_EXT_SDTR_LEN;
		siop_cmd->siop_table->msg_out[2] = MSG_EXT_SDTR;
		siop_cmd->siop_table->msg_out[3] = sc->minsync;
		siop_cmd->siop_table->msg_out[4] = sc->maxoff;
		siop_cmd->siop_table->t_msgout.count =
		    htole32(MSG_EXT_SDTR_LEN + 2);
		siop_cmd->siop_table->t_msgout.addr = htole32(siop_cmd->dsa);
		siop_table_sync(siop_cmd,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		CALL_SCRIPT(Ent_send_msgout);
		return;
	} else {
		/* target initiated wide negotiation */
		if (siop_cmd->siop_table->msg_in[3] >= MSG_EXT_WDTR_BUS_16_BIT
		    && (sc->features & SF_BUS_WIDE)) {
			printf("%s: target %d using 16bit transfers\n",
			    sc->sc_dev.dv_xname, target);
			siop_target->flags |= TARF_WIDE;
			sc->targets[target]->id |= SCNTL3_EWS << 24;
			siop_cmd->siop_table->msg_out[3] =
			    MSG_EXT_WDTR_BUS_16_BIT;
		} else {
			printf("%s: target %d using 8bit transfers\n",
			    sc->sc_dev.dv_xname, target);
			siop_target->flags &= ~SF_BUS_WIDE;
			sc->targets[target]->id &= ~(SCNTL3_EWS << 24);
			siop_cmd->siop_table->msg_out[3] =
			    MSG_EXT_WDTR_BUS_8_BIT;
		}
		siop_cmd->siop_table->id =
		    htole32(sc->targets[target]->id);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
		    (sc->targets[target]->id >> 24) & 0xff);
		/*
		 * we did reset wide parameters, so fall back to async,
		 * but don't shedule a sync neg, target should initiate it
		 */
		siop_target->status = TARST_OK;
		siop_cmd->siop_table->msg_out[0] = MSG_EXTENDED;
		siop_cmd->siop_table->msg_out[1] = MSG_EXT_WDTR_LEN;
		siop_cmd->siop_table->msg_out[2] = MSG_EXT_WDTR;
		siop_cmd->siop_table->t_msgout.count=
		    htole32(MSG_EXT_WDTR_LEN + 2);
		siop_cmd->siop_table->t_msgout.addr = htole32(siop_cmd->dsa);
		siop_table_sync(siop_cmd,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		CALL_SCRIPT(Ent_send_msgout);
	}
}

void
siop_sdtr_neg(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	struct siop_softc *sc = siop_cmd->siop_target->siop_sc;
	struct siop_target *siop_target = siop_cmd->siop_target;
	int target = siop_cmd->xs->sc_link->scsipi_scsi.target;
	int sync, offset, i;
	int send_msgout = 0;

	sync = siop_cmd->siop_table->msg_in[3];
	offset = siop_cmd->siop_table->msg_in[4];

	if (siop_target->status == TARST_SYNC_NEG) {
		/* we initiated sync negotiation */
		siop_target->status = TARST_OK;
#ifdef DEBUG
		printf("sdtr: sync %d offset %d\n", sync, offset);
#endif
		if (offset > sc->maxoff || sync < sc->minsync ||
			sync > sc->maxsync)
			goto reject;
		for (i = 0; i < sizeof(scf_period) / sizeof(scf_period[0]);
		    i++) {
			if (sc->clock_period != scf_period[i].clock)
				continue;
			if (scf_period[i].period == sync) {
				/* ok, found it. we now are sync. */
				printf("%s: target %d now synchronous at "
				    "%sMhz, offset %d\n", sc->sc_dev.dv_xname,
				    target, scf_period[i].rate, offset);
				sc->targets[target]->id &=
				    ~(SCNTL3_SCF_MASK << 24);
				sc->targets[target]->id |= scf_period[i].scf
				    << (24 + SCNTL3_SCF_SHIFT);
				if (sync < 25) /* Ultra */
					sc->targets[target]->id |=
					    SCNTL3_ULTRA << 24;
				else
					sc->targets[target]->id &=
					    ~(SCNTL3_ULTRA << 24);
				sc->targets[target]->id &=
				    ~(SCXFER_MO_MASK << 8);
				sc->targets[target]->id |=
				    (offset & SCXFER_MO_MASK) << 8;
				goto end;
			}
		}
		/*
		 * we didn't find it in our table, do async and send reject
		 * msg
		 */
reject:
		send_msgout = 1;
		siop_cmd->siop_table->t_msgout.count= htole32(1);
		siop_cmd->siop_table->msg_out[0] = MSG_MESSAGE_REJECT;
		printf("%s: target %d asynchronous\n", sc->sc_dev.dv_xname,
		    target);
		sc->targets[target]->id &= ~(SCNTL3_SCF_MASK << 24);
		sc->targets[target]->id &= ~(SCNTL3_ULTRA << 24);
		sc->targets[target]->id &= ~(SCXFER_MO_MASK << 8);
	} else { /* target initiated sync neg */
#ifdef DEBUG
		printf("sdtr (target): sync %d offset %d\n", sync, offset);
#endif
		if (offset == 0 || sync > sc->maxsync) { /* async */
			goto async;
		}
		if (offset > sc->maxoff)
			offset = sc->maxoff;
		if (sync < sc->minsync)
			sync = sc->minsync;
		/* look for sync period */
		for (i = 0; i < sizeof(scf_period) / sizeof(scf_period[0]);
		    i++) {
			if (sc->clock_period != scf_period[i].clock)
				continue;
			if (scf_period[i].period == sync) {
				/* ok, found it. we now are sync. */
				printf("%s: target %d now synchronous at "
				    "%sMhz, offset %d\n", sc->sc_dev.dv_xname,
				    target, scf_period[i].rate, offset);
				sc->targets[target]->id &=
				    ~(SCNTL3_SCF_MASK << 24);
				sc->targets[target]->id |= scf_period[i].scf
				    << (24 + SCNTL3_SCF_SHIFT);
				if (sync < 25) /* Ultra */
					sc->targets[target]->id |=
					    SCNTL3_ULTRA << 24;
				else
					sc->targets[target]->id &=
					    ~(SCNTL3_ULTRA << 24);
				sc->targets[target]->id &=
				    ~(SCXFER_MO_MASK << 8);
				sc->targets[target]->id |=
				    (offset & SCXFER_MO_MASK) << 8;
				siop_cmd->siop_table->msg_out[0] = MSG_EXTENDED;
				siop_cmd->siop_table->msg_out[1] =
				    MSG_EXT_SDTR_LEN;
				siop_cmd->siop_table->msg_out[2] = MSG_EXT_SDTR;
				siop_cmd->siop_table->msg_out[3] = sync;
				siop_cmd->siop_table->msg_out[4] = offset;
				siop_cmd->siop_table->t_msgout.count=
				    htole32(MSG_EXT_SDTR_LEN + 2);
				send_msgout = 1;
				goto end;
			}
		}
async:
		printf("%s: target %d asynchronous\n",
		    sc->sc_dev.dv_xname, target);
		sc->targets[target]->id &= ~(SCNTL3_SCF_MASK << 24);
		sc->targets[target]->id &= ~(SCNTL3_ULTRA << 24);
		sc->targets[target]->id &= ~(SCXFER_MO_MASK << 8);
		siop_cmd->siop_table->msg_out[0] = MSG_EXTENDED;
		siop_cmd->siop_table->msg_out[1] = MSG_EXT_SDTR_LEN;
		siop_cmd->siop_table->msg_out[2] = MSG_EXT_SDTR;
		siop_cmd->siop_table->msg_out[3] = 0;
		siop_cmd->siop_table->msg_out[4] = 0;
		siop_cmd->siop_table->t_msgout.count=
		    htole32(MSG_EXT_SDTR_LEN + 2);
		send_msgout = 1;
	}
end:
#ifdef DEBUG
	printf("id now 0x%x\n", sc->targets[target]->id);
#endif
	siop_cmd->siop_table->id = htole32(sc->targets[target]->id);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL3,
	    (sc->targets[target]->id >> 24) & 0xff);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCXFER,
	    (sc->targets[target]->id >> 8) & 0xff);
	if (send_msgout) {
		siop_cmd->siop_table->t_msgout.addr = htole32(siop_cmd->dsa);
		siop_table_sync(siop_cmd,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		CALL_SCRIPT(Ent_send_msgout);
	} else {
		CALL_SCRIPT(Ent_msgin_ack);
	}
	return;
}

/*
 * handle a bus reset: reset chip, unqueue all active commands and report
 * loosage to upper layer.
 * As the upper layer may requeue immediatly we have to first store
 * all active commands in a temporary queue.
 */
void
siop_handle_reset(sc)
	struct siop_softc *sc;
{
	struct cmd_list reset_list;
	struct siop_cmd *siop_cmd, *next_siop_cmd;
	int target, lun;
	/*
	 * scsi bus reset. reset the chip and restart
	 * the queue. Need to clean up all active commands
	 */
	printf("%s: scsi bus reset\n", sc->sc_dev.dv_xname);
	/* stop, reset and restart the chip */
	siop_reset(sc);
	TAILQ_INIT(&reset_list);
	/* find all active commands */
	for (target = 0; target < sc->sc_link.scsipi_scsi.max_target;
	    target++) {
		if (sc->targets[target] == NULL)
			continue;
		for (lun = 0; lun < 8; lun++) {
			for (siop_cmd =
			    TAILQ_FIRST(&sc->targets[target]->active_list[lun]);
			    siop_cmd != NULL; siop_cmd = next_siop_cmd) {
				next_siop_cmd = TAILQ_NEXT(siop_cmd, next);
				if (siop_cmd->status < CMDST_ACTIVE)
					continue;
				printf("cmd %p (target %d) in reset list\n",
				    siop_cmd, target);
				TAILQ_REMOVE(
				    &sc->targets[target]->active_list[lun],
				    siop_cmd, next);
				TAILQ_INSERT_TAIL(&reset_list, siop_cmd, next);
			}
		}
		sc->targets[target]->status = TARST_ASYNC;
		sc->targets[target]->flags = ~(TARF_SYNC | TARF_WIDE);
	}
	for (siop_cmd = TAILQ_FIRST(&reset_list); siop_cmd != NULL;
	    siop_cmd = next_siop_cmd) {
		next_siop_cmd = TAILQ_NEXT(siop_cmd, next);
		siop_cmd->xs->error = (siop_cmd->flags & CMDFL_TIMEOUT) ?
		    XS_TIMEOUT : XS_RESET;
		printf("cmd %p about to be processed\n", siop_cmd);
		TAILQ_REMOVE(&reset_list, siop_cmd, next);
		siop_scsicmd_end(siop_cmd);
		TAILQ_INSERT_TAIL(&sc->free_list, siop_cmd, next);
	}
}

void
siop_minphys(bp)
	struct buf *bp;
{
	minphys(bp);
}

int
siop_ioctl(link, cmd, arg, flag, p)
	struct scsipi_link *link;
	u_long cmd;
	caddr_t arg;
	int flag;
	struct proc *p;
{
	struct siop_softc *sc = link->adapter_softc;
	u_int8_t scntl1;
	int s;

	switch (cmd) {
	case SCBUSIORESET:
		s = splbio();
		scntl1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1,
		    scntl1 | SCNTL1_RST);
		/* minimum 25 us, more time won't hurt */
		delay(100);
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, scntl1);
		splx(s);
		return (0);
	default:
		return (ENOTTY);
	}
}

int
siop_scsicmd(xs)
	struct scsipi_xfer *xs;
{
	struct siop_softc *sc = (struct siop_softc *)xs->sc_link->adapter_softc;
	struct siop_cmd *siop_cmd;
	int s, error, i;
	int target = xs->sc_link->scsipi_scsi.target;
	int lun = xs->sc_link->scsipi_scsi.lun;

	s = splbio();
#ifdef DEBUG_SHED
	printf("starting cmd for %d:%d\n", target, lun);
#endif
	siop_cmd = sc->free_list.tqh_first;
	if (siop_cmd) {
		TAILQ_REMOVE(&sc->free_list, siop_cmd, next);
	}
	splx(s);
	if (siop_cmd == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}
#ifdef DIAGNOSTIC
	if (siop_cmd->status != CMDST_FREE)
		panic("siop_scsicmd: new cmd not free");
#endif
	if (sc->targets[target] == NULL) {
		sc->targets[target] =
		    malloc(sizeof(struct siop_target), M_DEVBUF, M_NOWAIT);
		if (sc->targets[target] == NULL) {
			printf("%s: can't malloc memory for target %d\n",
			    sc->sc_dev.dv_xname, target);
			xs->error = XS_DRIVER_STUFFUP;
			return(TRY_AGAIN_LATER);
		}
		sc->targets[target]->siop_sc = sc;
		sc->targets[target]->status = TARST_PROBING;
		sc->targets[target]->flags = 0;
		sc->targets[target]->id = sc->clock_div << 24; /* scntl3 */
		sc->targets[target]->id |=  target << 16; /* id */
		sc->targets[target]->id |= 0x80 << 8; /* scxfer */
		for (i = 0; i < 8; i++) 
			TAILQ_INIT(&sc->targets[target]->active_list[i]);
	}
	siop_cmd->siop_target = sc->targets[target];
	siop_cmd->xs = xs;
	siop_cmd->siop_table->id = htole32(sc->targets[target]->id);
	siop_cmd->siop_table->t_msgout.count= htole32(1);
	siop_cmd->siop_table->t_msgout.addr = htole32(siop_cmd->dsa);
	memset(siop_cmd->siop_table->msg_out, 0, 8);
	siop_cmd->siop_table->msg_out[0] = MSG_IDENTIFY(lun, 1);
	if (sc->targets[target]->status == TARST_ASYNC) {
		if (sc->features & SF_BUS_WIDE) {
			sc->targets[target]->status = TARST_WIDE_NEG;
			siop_cmd->siop_table->msg_out[1] = MSG_EXTENDED;
			siop_cmd->siop_table->msg_out[2] = MSG_EXT_WDTR_LEN;
			siop_cmd->siop_table->msg_out[3] = MSG_EXT_WDTR;
			siop_cmd->siop_table->msg_out[4] =
			    MSG_EXT_WDTR_BUS_16_BIT;
			siop_cmd->siop_table->t_msgout.count=
			    htole32(MSG_EXT_WDTR_LEN + 2 + 1);
		} else {
			sc->targets[target]->status = TARST_SYNC_NEG;
			siop_cmd->siop_table->msg_out[1] = MSG_EXTENDED;
			siop_cmd->siop_table->msg_out[2] = MSG_EXT_SDTR_LEN;
			siop_cmd->siop_table->msg_out[3] = MSG_EXT_SDTR;
			siop_cmd->siop_table->msg_out[4] = sc->minsync;
			siop_cmd->siop_table->msg_out[5] = sc->maxoff;
			siop_cmd->siop_table->t_msgout.count=
			    htole32(MSG_EXT_SDTR_LEN + 2 +1);
		}
	}
	siop_cmd->siop_table->status = htole32(0xff); /* set invalid status */

	/* load the DMA maps */
	error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_cmd,
	    xs->cmd, xs->cmdlen, NULL, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: unable to load cmd DMA map: %d",
		    sc->sc_dev.dv_xname, error);
		xs->error = XS_DRIVER_STUFFUP;
		return(TRY_AGAIN_LATER);
	}
	siop_cmd->siop_table->cmd.count =
	    htole32(siop_cmd->dmamap_cmd->dm_segs[0].ds_len);
	siop_cmd->siop_table->cmd.addr =
	    htole32(siop_cmd->dmamap_cmd->dm_segs[0].ds_addr);
	if (xs->xs_control & (XS_CTL_DATA_IN | XS_CTL_DATA_OUT)) {
		error = bus_dmamap_load(sc->sc_dmat, siop_cmd->dmamap_data,
		    xs->data, xs->datalen, NULL, BUS_DMA_NOWAIT);
		if (error) {
			printf("%s: unable to load cmd DMA map: %d",
			    sc->sc_dev.dv_xname, error);
			xs->error = XS_DRIVER_STUFFUP;
			return(TRY_AGAIN_LATER);
			bus_dmamap_unload(sc->sc_dmat, siop_cmd->dmamap_cmd);
		}
		for (i = 0; i < siop_cmd->dmamap_data->dm_nsegs; i++) {
			siop_cmd->siop_table->data[i].count =
			    htole32(siop_cmd->dmamap_data->dm_segs[i].ds_len);
			siop_cmd->siop_table->data[i].addr =
			    htole32(siop_cmd->dmamap_data->dm_segs[i].ds_addr);
		}
		bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_data, 0,
		    siop_cmd->dmamap_data->dm_mapsize,
		    (xs->xs_control & XS_CTL_DATA_IN) ?
		    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	}
	bus_dmamap_sync(sc->sc_dmat, siop_cmd->dmamap_cmd, 0,
	    siop_cmd->dmamap_cmd->dm_mapsize, BUS_DMASYNC_PREWRITE);
	siop_table_sync(siop_cmd, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	siop_cmd->status = CMDST_READY;
	s = splbio();
	TAILQ_INSERT_TAIL(&sc->targets[target]->active_list[lun],
	    siop_cmd, next);
	siop_start(sc);
	splx(s);
	return (SUCCESSFULLY_QUEUED);
}

void
siop_start(sc)
	struct siop_softc *sc;
{
	struct siop_cmd *siop_cmd;
	u_int32_t *scr;
	u_int32_t dsa;
	int timeout;
	int target, lun, slot, maxslot, maxslot0;
	int newcmd = 0; 

	/*
	 * first make sure to read valid data
	 */
	siop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * we need to treat the slots as circular queue (to give a chance
	 * to others commands to execute). So we have to remember the
	 * last used slot in sc_currshedslot.
	 */
	maxslot0 = sc->sc_currshedslot + 1;
	maxslot = sc->sc_nshedslots;
	slot = sc->sc_currshedslot + 1;
	if (slot >= sc->sc_nshedslots)
		sc->sc_currshedslot = slot = 0;
	for (target = 0; target <= sc->sc_link.scsipi_scsi.max_target;
	    target++) {
		if (sc->targets[target] == NULL)
			continue;
		for (lun = 0; lun < 8; lun++) {
			siop_cmd =
			    sc->targets[target]->active_list[lun].tqh_first;
			if (siop_cmd == NULL)
				continue;
			if (siop_cmd->status != CMDST_READY &&
			    siop_cmd->status != CMDST_SENSE)
				continue;
			/* find a free sheduler slot and load it */
retry:
			for (; slot < maxslot; slot++) {
				scr = &sc->sc_script[Ent_sheduler / 4 +
				    (Ent_nextslot / 4) * slot];
				/*
				 * if relative addr of first jump is 0 the
				 * slot isn't free
				 */
				if (scr[Ent_slot / 4 + 1] == 0)
					continue;
#ifdef DEBUG_SHED
				printf("using slot %d for DSA 0x%lx\n", slot,
				    (u_long)siop_cmd->dsa);
#endif
				/* note that we started a new command */
				newcmd = 1;
				/* mark command as active */
				if (siop_cmd->status == CMDST_READY)
					siop_cmd->status = CMDST_ACTIVE;
				else if (siop_cmd->status == CMDST_SENSE)
					siop_cmd->status = CMDST_SENSE_ACTIVE;
				else
					panic("siop_start: bad status");
				/* patch script with DSA addr */
				dsa = siop_cmd->dsa;
				/*
				 * 0x78000000 is a 'move data8 to reg'. data8
				 * is the second octet, reg offset is the third.
				 */
				scr[Ent_idsa0 / 4] =
				    htole32(0x78100000 |
				    ((dsa & 0x000000ff) <<  8));
				scr[Ent_idsa1 / 4] =
				    htole32(0x78110000 |
				    ( dsa & 0x0000ff00       ));
				scr[Ent_idsa2 / 4] =
				    htole32(0x78120000 |
				    ((dsa & 0x00ff0000) >>  8));
				scr[Ent_idsa3 / 4] =
				    htole32(0x78130000 |
				    ((dsa & 0xff000000) >> 16));
				/* handle timeout */
				if (siop_cmd->status == CMDST_ACTIVE) {
					if ((siop_cmd->xs->xs_control &
					    XS_CTL_POLL) == 0) {
						/* start exire timer */
						timeout =
						    siop_cmd->xs->timeout *
						    hz / 1000;
						if (timeout == 0)
							timeout = 1;
						callout_reset(
						    &siop_cmd->xs->xs_callout,
						    timeout, siop_timeout,
						    siop_cmd);
					}
				}
				/*
				 * Change jump offset so that this slot will be
				 * handled
				 */
				scr[Ent_slot / 4 + 1] = 0;
				break;
			}
			/* if we reached the end, restart at the beggining */
			if (slot == sc->sc_nshedslots) {
				slot = 0;
				maxslot = maxslot0;
				goto retry;
			}
			/* no more free slot, no need to continue */
			if (slot == sc->sc_currshedslot)
				goto end;
			sc->sc_currshedslot = slot;
		}
	}
end:
	/* if nothing changed no need to flush cache and wakeup script */
	if (newcmd == 0)
		return;
	/* make sure SCRIPT processor will read valid data */
	siop_script_sync(sc, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/* Signal script it has some work to do */
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_ISTAT, ISTAT_SIGP);
	/* and wait for IRQ */
	return;
}

void
siop_timeout(v)
	void *v;
{
	struct siop_cmd *siop_cmd = v;
	struct siop_softc *sc = siop_cmd->siop_target->siop_sc;
	int s;
	u_int8_t scntl1;

	scsi_print_addr(siop_cmd->xs->sc_link);
	printf("command timeout\n");

	s = splbio();
	/* reset the scsi bus */
	scntl1 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1,
	    scntl1 | SCNTL1_RST);
	/* minimum 25 us, more time won't hurt */
	delay(100);
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_SCNTL1, scntl1);

	/* desactivate callout */
	callout_stop(&siop_cmd->xs->xs_callout);
	/* mark command has being timed out; siop_intr will handle it */
	/*
	 * mark command has being timed out and just return;
	 * the bus reset will generate an interrupt,
	 * it will be handled in siop_intr()
	 */
	siop_cmd->flags |= CMDFL_TIMEOUT;
	splx(s);
	return;

}

void
siop_sdp(siop_cmd)
	struct siop_cmd *siop_cmd;
{
	/* save data pointer. Handle async only for now */
	int offset, dbc, sstat;
	struct siop_softc *sc = siop_cmd->siop_target->siop_sc;
	scr_table_t *table; /* table to patch */

	if ((siop_cmd->xs->xs_control & (XS_CTL_DATA_OUT | XS_CTL_DATA_IN))
	    == 0)
	    return; /* no data pointers to save */
	offset = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SCRATCHA + 1);
	if (offset >= SIOP_NSG) {
		printf("%s: bad offset in siop_sdp (%d)\n",
		    sc->sc_dev.dv_xname, offset);
		return;
	}
	table = &siop_cmd->siop_table->data[offset];
#ifdef DEBUG_DR
	printf("sdp: offset %d count=%d addr=0x%x ", offset,
	    table->count, table->addr);
#endif
	dbc = bus_space_read_4(sc->sc_rt, sc->sc_rh, SIOP_DBC) & 0x00ffffff;
	if (siop_cmd->xs->xs_control & XS_CTL_DATA_OUT) {
		/* need to account stale data in FIFO */
		/* XXX check for large fifo */
		dbc += (bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_DFIFO) -
		    (dbc & 0x7f)) & 0x7f;
		sstat = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_SSTAT0);
		if (sstat & SSTAT0_OLF)
			dbc++;
		if (sstat & SSTAT0_ORF)
			dbc++;
		if (siop_cmd->siop_target->flags & TARF_WIDE) {
			sstat = bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_SSTAT2);
			if (sstat & SSTAT2_OLF1)
				dbc++;
			if (sstat & SSTAT2_ORF1)
				dbc++;
		}
		/* clear the FIFO */
		bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
		    bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3) |
		    CTEST3_CLF);
	}
	table->addr =
	    htole32(le32toh(table->addr) + le32toh(table->count) - dbc);
	table->count = htole32(dbc);
#ifdef DEBUG_DR
	printf("now count=%d addr=0x%x\n", table->count, table->addr);
#endif
}

void
siop_clearfifo(sc)
	struct siop_softc *sc;
{
	int timeout = 0;
	int ctest3 = bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3);

#ifdef DEBUG_INTR
	printf("DMA fifo not empty !\n");
#endif
	bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
	    ctest3 | CTEST3_CLF);
	while ((bus_space_read_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3) &
	    CTEST3_CLF) != 0) {
		delay(1);
		if (++timeout > 1000) {
			printf("clear fifo failed\n");
			bus_space_write_1(sc->sc_rt, sc->sc_rh, SIOP_CTEST3,
			    bus_space_read_1(sc->sc_rt, sc->sc_rh,
			    SIOP_CTEST3) & ~CTEST3_CLF);
			return;
		}
	}
}

void
siop_dump_script(sc)
	struct siop_softc *sc;
{
	int i;
	siop_script_sync(sc, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	for (i = 0; i < CMD_OFF / 4; i += 2) {
		printf("0x%04x: 0x%08x 0x%08x", i * 4,
		    le32toh(sc->sc_script[i]), le32toh(sc->sc_script[i+1]));
		if ((le32toh(sc->sc_script[i]) & 0xe0000000) == 0xc0000000) {
			i++;
			printf(" 0x%08x", le32toh(sc->sc_script[i+1]));
		}
		printf("\n");
	}
}

#ifdef SIOP_STATS
void
siop_printstats()
{
	printf("siop_stat_intr %d\n", siop_stat_intr);
	printf("siop_stat_intr_shortxfer %d\n", siop_stat_intr_shortxfer);
	printf("siop_stat_intr_xferdisc %d\n", siop_stat_intr_xferdisc);
	printf("siop_stat_intr_sdp %d\n", siop_stat_intr_sdp);
	printf("siop_stat_intr_reselect %d\n", siop_stat_intr_reselect);
	printf("siop_stat_intr_done %d\n", siop_stat_intr_done);
}
#endif
