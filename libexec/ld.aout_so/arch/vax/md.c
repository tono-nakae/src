/*	$NetBSD: md.c,v 1.7 1998/08/26 14:37:41 matt Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
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

#include <sys/param.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <err.h>
#include <fcntl.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>

#include "ld.h"

/*
 * Get relocation addend corresponding to relocation record RP
 * from address ADDR
 */
long
md_get_addend(rp, addr)
struct relocation_info	*rp;
unsigned char		*addr;
{
	switch (RELOC_TARGET_SIZE(rp)) {
	case 0:
		return get_byte(addr);
	case 1:
		return get_short(addr);
	case 2:
		return get_long(addr);
	default:
		errx(1, "Unsupported relocation size: %x",
		    RELOC_TARGET_SIZE(rp));
	}
}

/*
 * Put RELOCATION at ADDR according to relocation record RP.
 */
void
md_relocate(rp, relocation, addr, relocatable_output)
struct relocation_info	*rp;
long			relocation;
unsigned char		*addr;
int			relocatable_output;
{
	/*
	 * 
	 */
	if (rp->r_baserel && rp->r_pcrel && !relocatable_output)
		relocation += got_symbol->value - (rp->r_address + 4);

	switch (RELOC_TARGET_SIZE(rp)) {
	case 0:
		put_byte(addr, relocation);
		break;
	case 1:
		put_short(addr, relocation);
		break;
	case 2:
		put_long(addr, relocation);
		break;
	default:
		errx(1, "Unsupported relocation size: %x",
		    RELOC_TARGET_SIZE(rp));
	}
}

/*
 * Machine dependent part of claim_rrs_reloc().
 * Set RRS relocation type.
 */
int
md_make_reloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	/* Relocation size */
	r->r_length = rp->r_length;

	if (RELOC_PCREL_P(rp))
		r->r_pcrel = 1;

	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;

	if (type & RELTYPE_COPY)
		r->r_copy = 1;

	return 0;
}

/*
 * Set up a transfer from jmpslot at OFFSET (relative to the PLT table)
 * to the binder slot (which is at offset 0 of the PLT).
 */
void
md_make_jmpslot(sp, offset, index)
jmpslot_t	*sp;
long		offset;
long		index;
{
	/*
	 * On VAX a branch offset given in immediate mode is relative to
	 * the end of the address itself.
	 */
	u_long fudge = -(offset + 8 - 2 /* skip mask */);

	if (offset == 0) {
		sp->mask = 0x0101;		/* NOP NOP */
	} else {
		sp->mask - 0x0000;
	}
	sp->insn[0] = 0x16;			/* jsb */
	sp->insn[1] = 0xef;			/* L^(pc) */
	sp->insn[2] = (fudge >>  0) & 0xff;
	sp->insn[3] = (fudge >>  8) & 0xff;
	sp->insn[4] = (fudge >> 16) & 0xff;
	sp->insn[5] = (fudge >> 24) & 0xff;
	sp->reloc_index = index;
}

/*
 * Set up a "direct" transfer (ie. not through the run-time binder) from
 * jmpslot at OFFSET to ADDR. Used by `ld' when the SYMBOLIC flag is on,
 * and by `ld.so' after resolving the symbol.
 * On the VAX, we use a PC relative JMP instruction, so no
 * further RRS relocations will be necessary for such a jmpslot.
 */
void
md_fix_jmpslot(sp, offset, addr)
jmpslot_t	*sp;
long		offset;
u_long		addr;
{
	u_long fudge = addr - (offset + 9);

	if (offset == 0) {
		sp->mask = 0x0101;		/* NOP NOP */
		sp->insn[0] = 0x01;		/* nop */
		sp->insn[1] = 0x17;		/* jmp */
	} else {
		sp->mask - 0x0000;
		sp->insn[0] = 0xfa;		/* callg */
		sp->insn[1] = 0x6c;		/* (ap) */
	}
	sp->insn[2] = 0xef;			/* L^(pc) */
	sp->insn[2] = (fudge >>  0) & 0xff;
	sp->insn[3] = (fudge >>  8) & 0xff;
	sp->insn[4] = (fudge >> 16) & 0xff;
	sp->insn[5] = (fudge >> 24) & 0xff;
}

/*
 * Update the relocation record for a RRS jmpslot.
 */
void
md_make_jmpreloc(rp, r, type)
struct relocation_info	*rp, *r;
int			type;
{
	jmpslot_t	*sp;

	/*
	 * Fix relocation address to point to the correct
	 * location within this jmpslot.
	 */
	r->r_address += sizeof(sp->mask);

	/* Relocation size */
	r->r_length = 2;	/* 4 bytes */

	/* Set relocation type */
	r->r_jmptable = 1;
	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;
}

/*
 * Set relocation type for a RRS GOT relocation.
 */
void
md_make_gotreloc(rp, r, type, gotp)
struct relocation_info	*rp, *r;
int			type;
got_t			*gotp;
{
	/*
	 * this is a fixup from text space.
	 *    consider that addend is really -pc_offset + addend.
	 *    so remove -pc_offset from addend (which is stored in
	 *    the got).
	 *    movl l^datum+4, r0 --> movl @_datum@GOT, r0
	 *			     _datum@GOT: .long 4
	 */
	if (rp->r_baserel && rp->r_pcrel)
		*gotp += (rp->r_address + 4);

	r->r_baserel = 1;
	if (type & RELTYPE_RELATIVE)
		r->r_relative = 1;

	/* Relocation size */
	r->r_length = 2;	/* 4 bytes */
}

/*
 * Set relocation type for a RRS copy operation.
 */
void
md_make_cpyreloc(rp, r)
struct relocation_info	*rp, *r;
{
	/* Relocation size */
	r->r_length = 2;	/* 4 bytes */

	r->r_copy = 1;
}

void
md_set_breakpoint(where, savep)
long	where;
long	*savep;
{
	*savep = *(long *)where;
	*(char *)where = BPT;		/* !!! fixit !!! */
}

#ifndef RTLD

#ifdef FreeBSD
int	netzmagic;
#endif

/*
 * Initialize (output) exec header such that useful values are
 * obtained from subsequent N_*() macro evaluations.
 */
void
md_init_header(hp, magic, flags)
struct exec	*hp;
int		magic, flags;
{
#ifdef NetBSD
	if (oldmagic || magic == QMAGIC)
		hp->a_midmag = magic;
	else
		N_SETMAGIC((*hp), magic, MID_VAX, flags);
#endif
#ifdef FreeBSD
	if (oldmagic)
		hp->a_midmag = magic;
	else if (netzmagic)
		N_SETMAGIC_NET((*hp), magic, MID_VAX, flags);
	else
		N_SETMAGIC((*hp), magic, MID_VAX, flags);
#endif

	/* TEXT_START depends on the value of outheader.a_entry.  */
	if (!(link_mode & SHAREABLE))
		hp->a_entry = PAGSIZ;
}
#endif /* RTLD */


#ifdef NEED_SWAP
/*
 * Byte swap routines for cross-linking.
 */

void
md_swapin_exec_hdr(h)
struct exec *h;
{
	int skip = 0;

	if (!N_BADMAG(*h))
		skip = 1;

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}

void
md_swapout_exec_hdr(h)
struct exec *h;
{
	/* NetBSD: Always leave magic alone */
	int skip = 1;
#if 0
	if (N_GETMAGIC(*h) == OMAGIC)
		skip = 0;
#endif

	swap_longs((long *)h + skip, sizeof(*h)/sizeof(long) - skip);
}


void
md_swapin_reloc(r, n)
struct relocation_info *r;
int n;
{
	int	bits;

	for (; n; n--, r++) {
		r->r_address = md_swap_long(r->r_address);
		bits = ((int *)r)[1];
		r->r_symbolnum = md_swap_long(bits) & 0x00ffffff;
		r->r_pcrel = (bits & 1);
		r->r_length = (bits >> 1) & 3;
		r->r_extern = (bits >> 3) & 1;
		r->r_baserel = (bits >> 4) & 1;
		r->r_jmptable = (bits >> 5) & 1;
		r->r_relative = (bits >> 6) & 1;
#ifdef N_SIZE
		r->r_copy = (bits >> 7) & 1;
#endif
	}
}

void
md_swapout_reloc(r, n)
struct relocation_info *r;
int n;
{
	int	bits;

	for (; n; n--, r++) {
		r->r_address = md_swap_long(r->r_address);
		bits = md_swap_long(r->r_symbolnum) & 0xffffff00;
		bits |= (r->r_pcrel & 1);
		bits |= (r->r_length & 3) << 1;
		bits |= (r->r_extern & 1) << 3;
		bits |= (r->r_baserel & 1) << 4;
		bits |= (r->r_jmptable & 1) << 5;
		bits |= (r->r_relative & 1) << 6;
#ifdef N_SIZE
		bits |= (r->r_copy & 1) << 7;
#endif
		((int *)r)[1] = bits;
	}
}

void
md_swapout_jmpslot(j, n)
jmpslot_t	*j;
int		n;
{
	for (; n; n--, j++) {
		j->opcode = md_swap_short(j->opcode);
		j->addr[0] = md_swap_short(j->addr[0]);
		j->addr[1] = md_swap_short(j->addr[1]);
		j->reloc_index = md_swap_short(j->reloc_index);
	}
}

#endif /* NEED_SWAP */
