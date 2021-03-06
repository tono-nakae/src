/* $Id: common.h,v 1.3 2013/02/23 16:22:39 jkunz Exp $ */

/*
 * Copyright (c) 2012 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Petri Laakso.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#ifndef _BOOTIMX23_COMMON_
#define _BOOTIMX23_COMMON_

#define REG_RD(reg) *(volatile uint32_t *)(reg)
#define REG_WR(reg, val)						\
do {									\
	*(volatile uint32_t *)((reg)) = val;				\
} while (0)
#define REG_RD_HW(reg) *(volatile uint16_t *)(reg)
#define REG_WR_HW(reg, val)						\
do {									\
	*(volatile uint16_t *)((reg)) = val;				\
} while (0)
#define REG_RD_BYTE(reg) *(volatile uint8_t *)(reg)
#define REG_WR_BYTE(reg, val)						\
do {									\
	*(volatile uint8_t *)((reg)) = val;				\
} while (0)

int clock_prep(void);
int emi_prep(void);
int pinctrl_prep(void);
int power_prep(void);
int args_prep(void);
void delay(unsigned int);
void putchar(int);
int getchar(void);

#endif /* !_BOOTIMX23_COMMON_ */
