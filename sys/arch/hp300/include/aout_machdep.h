/*
 * Copyright (c) 1993 Christopher G. Demetriou
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
 *
 *	$Id: aout_machdep.h,v 1.4 1993/08/19 14:34:25 mycroft Exp $
 */

#ifndef _HP300_EXEC_H_
#define _HP300_EXEC_H_

#ifdef KERNEL

#ifdef HPUXCOMPAT
#include "user.h"			/* for pcb */
#include "hp300/hpux/hpux_exec.h"
#endif

/*
 * the following, if defined, prepares a set of vmspace commands for
 * a given exectable package defined by epp.
 * The standard executable formats are taken care of automatically;
 * machine-specific ones can be defined using this function.
 */
int cpu_exec_makecmds __P((struct proc *p, struct exec_package *epp));

/*
 * the following function/macro checks to see if a given machine
 * type (a_mid) field is valid for this architecture
 * a non-zero return value indicates that the machine type is correct.
 */
#ifdef HPUXCOMPAT
#define cpu_exec_checkmid(mid) ((mid == MID_HP200) || (mid == MID_HP300) || \
				(mid == MID_HPUX))
#else
#define cpu_exec_checkmid(mid) ((mid == MID_HP200) || (mid == MID_HP300)))
#endif

#endif /* KERNEL */

#endif  /* _HP300_EXEC_H_ */
