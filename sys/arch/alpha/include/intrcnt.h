/* $NetBSD: intrcnt.h,v 1.15 1998/06/26 21:55:09 ross Exp $ */

/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#define	INTRNAMES_DEFINITION						\
/* 0x00 */	ASCIZ "clock";						\
		ASCIZ "isa irq 0";					\
		ASCIZ "isa irq 1";					\
		ASCIZ "isa irq 2";					\
		ASCIZ "isa irq 3";					\
		ASCIZ "isa irq 4";					\
		ASCIZ "isa irq 5";					\
		ASCIZ "isa irq 6";					\
		ASCIZ "isa irq 7";					\
		ASCIZ "isa irq 8";					\
		ASCIZ "isa irq 9";					\
		ASCIZ "isa irq 10";					\
		ASCIZ "isa irq 11";					\
		ASCIZ "isa irq 12";					\
		ASCIZ "isa irq 13";					\
		ASCIZ "isa irq 14";					\
/* 0x10 */	ASCIZ "isa irq 15";					\
		ASCIZ "kn20aa irq 0";					\
		ASCIZ "kn20aa irq 1";					\
		ASCIZ "kn20aa irq 2";					\
		ASCIZ "kn20aa irq 3";					\
		ASCIZ "kn20aa irq 4";					\
		ASCIZ "kn20aa irq 5";					\
		ASCIZ "kn20aa irq 6";					\
		ASCIZ "kn20aa irq 7";					\
		ASCIZ "kn20aa irq 8";					\
		ASCIZ "kn20aa irq 9";					\
		ASCIZ "kn20aa irq 10";					\
		ASCIZ "kn20aa irq 11";					\
		ASCIZ "kn20aa irq 12";					\
		ASCIZ "kn20aa irq 13";					\
		ASCIZ "kn20aa irq 14";					\
/* 0x20 */	ASCIZ "kn20aa irq 15";					\
		ASCIZ "kn20aa irq 16";					\
		ASCIZ "kn20aa irq 17";					\
		ASCIZ "kn20aa irq 18";					\
		ASCIZ "kn20aa irq 19";					\
		ASCIZ "kn20aa irq 20";					\
		ASCIZ "kn20aa irq 21";					\
		ASCIZ "kn20aa irq 22";					\
		ASCIZ "kn20aa irq 23";					\
		ASCIZ "kn20aa irq 24";					\
		ASCIZ "kn20aa irq 25";					\
		ASCIZ "kn20aa irq 26";					\
		ASCIZ "kn20aa irq 27";					\
		ASCIZ "kn20aa irq 28";					\
		ASCIZ "kn20aa irq 29";					\
		ASCIZ "kn20aa irq 30";					\
/* 0x30 */	ASCIZ "kn20aa irq 31";					\
		ASCIZ "kn15 tc slot 0";					\
		ASCIZ "kn15 tc slot 1";					\
		ASCIZ "kn15 tc slot 2";					\
		ASCIZ "kn15 tc slot 3";					\
		ASCIZ "kn15 tc slot 4";					\
		ASCIZ "kn15 tc slot 5";					\
		ASCIZ "kn15 tcds";					\
		ASCIZ "kn15 ioasic";					\
		ASCIZ "kn15 sfb";					\
		ASCIZ "kn16 tc slot 0";					\
		ASCIZ "kn16 tc slot 1";					\
		ASCIZ "kn16 tcds";					\
		ASCIZ "kn16 ioasic";					\
		ASCIZ "kn16 sfb";					\
		ASCIZ "tcds esp 0";					\
/* 0x40 */	ASCIZ "tcds esp 1";					\
		ASCIZ "ioasic le";					\
		ASCIZ "ioasic scc 0";					\
		ASCIZ "ioasic scc 1";					\
		ASCIZ "ioasic am79c30";					\
		ASCIZ "eb164 irq 0";					\
		ASCIZ "eb164 irq 1";					\
		ASCIZ "eb164 irq 2";					\
		ASCIZ "eb164 irq 3";					\
		ASCIZ "eb164 irq 4";					\
		ASCIZ "eb164 irq 5";					\
		ASCIZ "eb164 irq 6";					\
		ASCIZ "eb164 irq 7";					\
		ASCIZ "eb164 irq 8";					\
		ASCIZ "eb164 irq 9";					\
		ASCIZ "eb164 irq 10";					\
/* 0x50 */	ASCIZ "eb164 irq 11";					\
		ASCIZ "eb164 irq 12";					\
		ASCIZ "eb164 irq 13";					\
		ASCIZ "eb164 irq 14";					\
		ASCIZ "eb164 irq 15";					\
		ASCIZ "eb164 irq 16";					\
		ASCIZ "eb164 irq 17";					\
		ASCIZ "eb164 irq 18";					\
		ASCIZ "eb164 irq 19";					\
		ASCIZ "eb164 irq 20";					\
		ASCIZ "eb164 irq 21";					\
		ASCIZ "eb164 irq 22";					\
		ASCIZ "eb164 irq 23";					\
		ASCIZ "xbar dma"; 					\
		ASCIZ "a12 unused";					\
		ASCIZ "a12  pci";					\
/* 0x60 */	ASCIZ "a12  pci serr";					\
		ASCIZ "xbar MCE";					\
		ASCIZ "xbar ECE";					\
		ASCIZ "a12  IEI";					\
		ASCIZ "xbar FORUN";					\
		ASCIZ "xbar FURUN";					\
		ASCIZ "a12  ICW";					\
		ASCIZ "kn8ae ioerr";					\
		ASCIZ "kn8ae pci";					\
		ASCIZ "kn300 irq 0";					\
		ASCIZ "kn300 irq 1";					\
		ASCIZ "kn300 irq 2";					\
		ASCIZ "kn300 irq 3";					\
		ASCIZ "kn300 irq 4";					\
		ASCIZ "kn300 irq 5";					\
		ASCIZ "kn300 irq 6";					\
/* 0x70 */	ASCIZ "kn300 irq 7";					\
		ASCIZ "kn300 irq 8";					\
		ASCIZ "kn300 irq 9";					\
		ASCIZ "kn300 irq 10";					\
		ASCIZ "kn300 irq 11";					\
		ASCIZ "kn300 irq 12";					\
		ASCIZ "kn300 irq 13";					\
		ASCIZ "kn300 irq 14";					\
		ASCIZ "kn300 irq 15";					\
		ASCIZ "kn300 ncr810";					\
		ASCIZ "kn300 i2c ctrl";					\
		ASCIZ "kn300 i2c bus";					\
		ASCIZ "eb64+ irq 0";					\
		ASCIZ "eb64+ irq 1";					\
		ASCIZ "eb64+ irq 2";					\
		ASCIZ "eb64+ irq 3";					\
/* 0x80 */	ASCIZ "eb64+ irq 4";					\
		ASCIZ "eb64+ irq 5";					\
		ASCIZ "eb64+ irq 6";					\
		ASCIZ "eb64+ irq 7";					\
		ASCIZ "eb64+ irq 8";					\
		ASCIZ "eb64+ irq 9";					\
		ASCIZ "eb64+ irq 10";					\
		ASCIZ "eb64+ irq 11";					\
		ASCIZ "eb64+ irq 12";					\
		ASCIZ "eb64+ irq 13";					\
		ASCIZ "eb64+ irq 14";					\
		ASCIZ "eb64+ irq 15";					\
		ASCIZ "eb64+ irq 16";					\
		ASCIZ "eb64+ irq 17";					\
		ASCIZ "eb64+ irq 18";					\
		ASCIZ "eb64+ irq 19";					\
/* 0x90 */	ASCIZ "eb64+ irq 20";					\
		ASCIZ "eb64+ irq 21";					\
		ASCIZ "eb64+ irq 22";					\
		ASCIZ "eb64+ irq 23";					\
		ASCIZ "eb64+ irq 24";					\
		ASCIZ "eb64+ irq 25";					\
		ASCIZ "eb64+ irq 26";					\
		ASCIZ "eb64+ irq 27";					\
		ASCIZ "eb64+ irq 28";					\
		ASCIZ "eb64+ irq 29";					\
		ASCIZ "eb64+ irq 30";					\
		ASCIZ "eb64+ irq 31";					\
		ASCIZ "dec 550 irq 0";					\
		ASCIZ "dec 550 irq 1";					\
		ASCIZ "dec 550 irq 2";					\
		ASCIZ "dec 550 irq 3";					\
/* 0x100 */	ASCIZ "dec 550 irq 4";					\
		ASCIZ "dec 550 irq 5";					\
		ASCIZ "dec 550 irq 6";					\
		ASCIZ "dec 550 irq 7";					\
		ASCIZ "dec 550 irq 8";					\
		ASCIZ "dec 550 irq 9";					\
		ASCIZ "dec 550 irq 10";					\
		ASCIZ "dec 550 irq 11";					\
		ASCIZ "dec 550 irq 12";					\
		ASCIZ "dec 550 irq 13";					\
		ASCIZ "dec 550 irq 14";					\
		ASCIZ "dec 550 irq 15";					\
		ASCIZ "dec 550 irq 16";					\
		ASCIZ "dec 550 irq 17";					\
		ASCIZ "dec 550 irq 18";					\
		ASCIZ "dec 550 irq 19";					\
/* 0x110 */	ASCIZ "dec 550 irq 20";					\
		ASCIZ "dec 550 irq 21";					\
		ASCIZ "dec 550 irq 22";					\
		ASCIZ "dec 550 irq 23";					\
		ASCIZ "dec 550 irq 24";					\
		ASCIZ "dec 550 irq 25";					\
		ASCIZ "dec 550 irq 26";					\
		ASCIZ "dec 550 irq 27";					\
		ASCIZ "dec 550 irq 28";					\
		ASCIZ "dec 550 irq 29";					\
		ASCIZ "dec 550 irq 30";					\
		ASCIZ "dec 550 irq 31";					\
		ASCIZ "dec 550 irq 32";					\
		ASCIZ "dec 550 irq 33";					\
		ASCIZ "dec 550 irq 34";					\
		ASCIZ "dec 550 irq 35";					\
/* 0x120 */	ASCIZ "dec 550 irq 36";					\
		ASCIZ "dec 550 irq 37";					\
		ASCIZ "dec 550 irq 38";					\
		ASCIZ "dec 550 irq 39";					\
		ASCIZ "dec 550 irq 40";					\
		ASCIZ "dec 550 irq 41";					\
		ASCIZ "dec 550 irq 42";					\
		ASCIZ "dec 550 irq 43";					\
		ASCIZ "dec 550 irq 44";					\
		ASCIZ "dec 550 irq 45";					\
		ASCIZ "dec 550 irq 46";					\
		ASCIZ "dec 550 irq 47";					\
		ASCIZ "dec 1000a irq 0";				\
		ASCIZ "dec 1000a irq 1";				\
		ASCIZ "dec 1000a irq 2";				\
		ASCIZ "dec 1000a irq 3";				\
/* 0x130 */	ASCIZ "dec 1000a irq 4";				\
		ASCIZ "dec 1000a irq 5";				\
		ASCIZ "dec 1000a irq 6";				\
		ASCIZ "dec 1000a irq 7";				\
		ASCIZ "dec 1000a irq 8";				\
		ASCIZ "dec 1000a irq 9";				\
		ASCIZ "dec 1000a irq 10";				\
		ASCIZ "dec 1000a irq 11";				\
		ASCIZ "dec 1000a irq 12";				\
		ASCIZ "dec 1000a irq 13";				\
		ASCIZ "dec 1000a irq 14";				\
		ASCIZ "dec 1000a irq 15";				\
		ASCIZ "dec 1000a irq 16";				\
		ASCIZ "dec 1000a irq 17";				\
		ASCIZ "dec 1000a irq 18";				\
		ASCIZ "dec 1000a irq 19";				\
/* 0x140 */	ASCIZ "dec 1000a irq 20";				\
		ASCIZ "dec 1000a irq 21";				\
		ASCIZ "dec 1000a irq 22";				\
		ASCIZ "dec 1000a irq 23";				\
		ASCIZ "dec 1000a irq 24";				\
		ASCIZ "dec 1000a irq 25";				\
		ASCIZ "dec 1000a irq 26";				\
		ASCIZ "dec 1000a irq 27";				\
		ASCIZ "dec 1000a irq 28";				\
		ASCIZ "dec 1000a irq 29";				\
		ASCIZ "dec 1000a irq 30";				\
/* 0x14b*/	ASCIZ "dec 1000a irq 31";				\
		ASCIZ "dec 1000 irq 0";					\
		ASCIZ "dec 1000 irq 1";					\
		ASCIZ "dec 1000 irq 2";					\
		ASCIZ "dec 1000 irq 3";					\
/* 0x150 */	ASCIZ "dec 1000 irq 4";					\
		ASCIZ "dec 1000 irq 5";					\
		ASCIZ "dec 1000 irq 6";					\
		ASCIZ "dec 1000 irq 7";					\
		ASCIZ "dec 1000 irq 8";					\
		ASCIZ "dec 1000 irq 9";					\
		ASCIZ "dec 1000 irq 10";				\
		ASCIZ "dec 1000 irq 11";				\
		ASCIZ "dec 1000 irq 12";				\
		ASCIZ "dec 1000 irq 13";				\
		ASCIZ "dec 1000 irq 14";				\
/* 0x15b */	ASCIZ "dec 1000 irq 15";

#define INTRCNT_DEFINITION						\
/* 0x00 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x10 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x20 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x30 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x40 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x50 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x60 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x70 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x80 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x90 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x100 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x110 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x120 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x130 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x140 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\
/* 0x150 */	.quad 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0;	\

#define	INTRCNT_CLOCK		0
#define	INTRCNT_ISA_IRQ		(INTRCNT_CLOCK + 1)
#define	INTRCNT_ISA_IRQ_LEN	16
#define	INTRCNT_KN20AA_IRQ	(INTRCNT_ISA_IRQ + INTRCNT_ISA_IRQ_LEN)
#define	INTRCNT_KN20AA_IRQ_LEN	32
#define	INTRCNT_KN15		(INTRCNT_KN20AA_IRQ + INTRCNT_KN20AA_IRQ_LEN)
#define	INTRCNT_KN15_LEN	9
#define	INTRCNT_KN16		(INTRCNT_KN15 + INTRCNT_KN15_LEN)
#define	INTRCNT_KN16_LEN	5
#define	INTRCNT_TCDS		(INTRCNT_KN16 + INTRCNT_KN16_LEN)
#define	INTRCNT_TCDS_LEN	2
#define	INTRCNT_IOASIC		(INTRCNT_TCDS + INTRCNT_TCDS_LEN)
#define	INTRCNT_IOASIC_LEN	4
#define	INTRCNT_EB164_IRQ	(INTRCNT_IOASIC + INTRCNT_IOASIC_LEN)
#define	INTRCNT_EB164_IRQ_LEN	24
#define	INTRCNT_A12_IRQ		(INTRCNT_EB164_IRQ + INTRCNT_EB164_IRQ_LEN)
#define	INTRCNT_A12_IRQ_LEN	10
#define	INTRCNT_KN8AE_IRQ	(INTRCNT_A12_IRQ + INTRCNT_A12_IRQ_LEN)
#define	INTRCNT_KN8AE_IRQ_LEN	2
#define	INTRCNT_KN300_IRQ	(INTRCNT_KN8AE_IRQ + INTRCNT_KN8AE_IRQ_LEN)
#	define	INTRCNT_KN300_NCR810	INTRCNT_KN300_IRQ + 16
#	define	INTRCNT_KN300_I2C_CTRL	INTRCNT_KN300_IRQ + 17
#	define	INTRCNT_KN300_I2C_BUS	INTRCNT_KN300_IRQ + 18
#define	INTRCNT_KN300_LEN	19
#define	INTRCNT_EB64PLUS_IRQ	(INTRCNT_KN300_IRQ + INTRCNT_KN300_LEN)
#define	INTRCNT_EB64PLUS_IRQ_LEN 32
#define	INTRCNT_DEC_550_IRQ	(INTRCNT_EB64PLUS_IRQ + \
				    INTRCNT_EB64PLUS_IRQ_LEN)
#define	INTRCNT_DEC_550_IRQ_LEN	48

#define	INTRCNT_DEC_1000A_IRQ	(INTRCNT_DEC_550_IRQ + INTRCNT_DEC_550_IRQ_LEN)
#define	INTRCNT_DEC_1000A_IRQ_LEN	32

#define	INTRCNT_DEC_1000_IRQ	(INTRCNT_DEC_1000A_IRQ +	\
					INTRCNT_DEC_1000A_IRQ_LEN)
#define	INTRCNT_DEC_1000_IRQ_LEN	16

#ifndef _LOCORE
extern volatile long intrcnt[];
#endif
