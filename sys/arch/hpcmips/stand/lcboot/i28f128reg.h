/* $NetBSD: i28f128reg.h,v 1.1 2003/05/01 07:02:02 igy Exp $ */

/*
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Naoto Shimazaki of YOKOGAWA Electric Corporation.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*
 * Flash Memory Writer
 */

#define	I28F128_BLOCK_SIZE	0x20000		/* 128Kbyte */
#define	I28F128_BLOCK_MASK	0x1ffff		/* 128Kbyte */

#define	I28F128_MANUFACT	0x89
#define	I28F128_DEVCODE		0x18
#define	I28F128_PRIM_COMM0	0x01
#define	I28F128_PRIM_COMM1	0x00
#define	I28F128_PRIM_EXT_TBL0	0x31
#define	I28F128_PRIM_EXT_TBL1	0x00

#define	I28F128_RESET		0xff
#define	I28F128_READ_ARRAY	I28F128_RESET
#define	I28F128_READ_ID		0x90
#define	I28F128_READ_STATUS	0x70
#define	I28F128_CLEAR_STATUS	0x50

#define	I28F128_BLK_ERASE_1ST	0x20
#define	I28F128_BLK_ERASE_2ND	0xd0
#define	I28F128_WORDBYTE_PROG	0x40

#define	I28F128_S_READY		0x80
#define	I28F128_S_ERASE_SUSPEND	0x40
#define	I28F128_S_ERASE_ERROR	0x20
#define	I28F128_S_PROG_ERROR	0x10
#define	I28F128_S_LOW_VOLTAGE	0x08
#define	I28F128_S_PROG_SUSPEND	0x04
#define	I28F128_S_BLOCK_LOCKED	0x02
