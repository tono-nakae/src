/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * from: Utah $Hdr: locore.s 1.58 91/04/22$
 *
 *	@(#)locore.s	7.11 (Berkeley) 5/9/91
 *
 * Original (hp300) Author: unknown, maybe Mike Hibler?
 * Amiga author: Markus Wild
 * Other contributors: Bryan Ford (kernel reload stuff)
 */

#include "assym.s"
#include "vectors.s"
#include "custom.h"
#include "zeusscsi.h"	/* needed for level 6 interrupt */

#define CIAAADDR(ar)	movl	_CIAAbase,ar
#define CIABADDR(ar)	movl	_CIABbase,ar
#define CUSTOMADDR(ar)	movl	_CUSTOMbase,ar
#define INTREQRADDR(ar)	movl	_INTREQRaddr,ar
#define INTREQWADDR(ar)	movl	_INTREQWaddr,ar

	.text
/*
 * This is where we wind up if the kernel jumps to location 0.
 * (i.e. a bogus PC)  This is known to immediately follow the vector
 * table and is hence at 0x400 (see reset vector in vectors.s).
 */
	.globl	_panic
	pea	Ljmp0panic
	jbsr	_panic
	/* NOTREACHED */
Ljmp0panic:
	.asciz	"kernel jump to zero"
	.even

/*
 * Do a dump.
 * Called by auto-restart.
 */
	.globl	_dumpsys
	.globl	_doadump
_doadump:
	jbsr	_dumpsys
	jbsr	_doboot
	/*NOTREACHED*/

/*
 * Trap/interrupt vector routines
 */

	.globl	_trap, _nofault, _longjmp
_buserr:
	tstl	_nofault		| device probe?
	jeq	_addrerr		| no, handle as usual
	movl	_nofault,sp@-		| yes,
	jbsr	_longjmp		|  longjmp(nofault)
_addrerr:
	clrw	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(60)		|   in the savearea
	lea	sp@(64),a1		| grab base of HW berr frame
	tstl	_cpu040
	jeq	Lbe030
	movl	a1@(10),sp@-		| V = exception address
	clrl	sp@-			| dummy code
	moveq	#0,d0
	movw	a1@(8),d0		| get vector offset
	andw	#0x0fff,d0
	cmpw	#12,d0			| is it address error
	jeq	Lisaerr
	movl	a1@(22),sp@(4)		| get fault address
	moveq	#0,d0
	movw	a1@(14),d0		| get SSW
	movl	d0,sp@			| pass as code
	btst	#10,d0			| test ATC
	jeq	Lisberr			| it's a bus error
	jra	Lismerr
Lbe030:
	moveq	#0,d0
	movw	a1@(12),d0		| grab SSW for fault processing
	btst	#12,d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,d0			| yes, must set FB
	movw	d0,a1@(12)		| for hardware too
LbeX0:
	btst	#13,d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,d0			| yes, must set FC
	movw	d0,a1@(12)		| for hardware too
LbeX1:
	btst	#8,d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	a1@(18),d1		| fault address is as given in frame
	jra	Lbe10			| thats it
Lbe0:
	btst	#4,a1@(8)		| long (type B) stack frame?
	jne	Lbe4			| yes, go handle
	movl	a1@(4),d1		| no, can use save PC
	btst	#14,d0			| FB set?
	jeq	Lbe3			| no, try FC
	addql	#4,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe3:
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, done
	addql	#2,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe4:
	movl	a1@(38),d1		| long format, use stage B address
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, all done
	subql	#2,d1			| yes, adjust address
Lbe10:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	movw	a1@(8),d0		| get frame format/vector offset
	andw	#0x0FFF,d0		| clear out frame format
	cmpw	#12,d0			| address error vector?
	jeq	Lisaerr			| yes, go to it
	movl	d1,a0			| fault address
	ptestr	#1,a0@,#7		| do a table search
	pmove	psr,sp@			| save result
	btst	#7,sp@			| bus error bit set?
	jeq	Lismerr			| no, must be MMU fault
	clrw	sp@			| yes, re-clear pad word
	jra	Lisberr			| and process as normal bus error
Lismerr:
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	Ltrapnstkadj		| and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	Ltrapnstkadj		| and deal with it
Lisberr:
	movl	#T_BUSERR,sp@-		| mark bus error
Ltrapnstkadj:
	jbsr	_trap			| handle the error
	lea	sp@(12),sp		| pop value args
	movl	sp@(60),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(64),d0		| need to adjust stack?
	jne	Lstkadj			| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#6,sp			| toss SSP and pad
	jra	rei			| all done
Lstkadj:
	lea	sp@(66),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(60)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * FP exceptions.
 */
_fpfline:
| check for unimplemented floating point instruction and emulate it
	.globl	fpsp_unimp
	btst	#5,sp@(6)		| is format == 2?
	jne	fpsp_unimp
	jra	_illinst

_fpunsupp:
	jra	_illinst

| FPSP entry points and support routines
	.globl	real_fline,real_bsun,real_unfl,real_operr,real_ovfl,real_snan
	.globl	real_unsupp,real_inex
	.globl	fpsp_done,fpsp_fmt_error,mem_read,mem_write,real_trace
real_fline:
	jra	_illinst
real_trace:
fpsp_done:
	rte
fpsp_fmt_error:
	pea	LFP1
	jbsr	_panic
mem_read:
	btst	#5,a6@(4)
	jeq	user_read
super_read:
	movb	a0@+,a1@+
	subl	#1,d0
	jne	super_read
	rts
user_read:
	movl	d1,sp@-
	movl	d0,sp@-		| len
	movl	a1,sp@-		| to
	movl	a0,sp@-		| from
	jsr	_copyin
	addw	#12,sp
	movl	sp@+,d1
	rts
mem_write:
	btst	#5,a6@(4)
	jeq	user_write
super_write:
	movb	a0@+,a1@+
	subl	#1,d0
	jne	super_write
	rts
user_write:
	movl	d1,sp@-
	movl	d0,sp@-		| len
	movl	a1,sp@-		| to
	movl	a0,sp@-		| from
	jsr	_copyout
	addw	#12,sp
	movl	sp@+,d1
	rts
LFP1:	.asciz	"FPSP format error"
	.even
real_unsupp:
	jra	_illinst

real_bsun:
real_inex:
real_dz:
real_unfl:
real_operr:
real_ovfl:
real_snan:
| Fall through into FP coprocessor exceptions

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
_fpfault:
#ifdef FPCOPROC
	clrw	sp@-		| pad SR to longword
	moveml	#0xFFFF,sp@-	| save user registers
	movl	usp,a0		| and save
	movl	a0,sp@(60)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	movl	_curpcb,a0	| current pcb
	lea	a0@(PCB_FPCTX),a0 | address of FP savearea
	fsave	a0@		| save state
	tstb	a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	d0		| no, need to tweak BIU
	movb	a0@(1),d0	| get frame size
	bset	#3,a0@(0,d0:w)	| set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	| push fpsr as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	Ltrapnstkadj	| call trap and deal with stack cleanup
#else
	jra	_badtrap	| treat as an unexpected trap
#endif

/*
 * Coprocessor and format errors can generate mid-instruction stack
 * frames and cause signal delivery hence we need to check for potential
 * stack adjustment.
 */
_coperr:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(60)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_COPERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

_fmterr:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(60)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_FMTERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */
_illinst:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ILLINST,d0
	jra	fault

_zerodiv:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ZERODIV,d0
	jra	fault

_chkinst:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_CHKINST,d0
	jra	fault

_trapvinst:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_TRAPVINST,d0
	jra	fault

_privinst:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_PRIVINST,d0
	jra	fault

	.globl	fault
fault:
	movl	usp,a0			| get and save
	movl	a0,sp@(60)		|   the user stack pointer
	clrl	sp@-			| no VA arg
	clrl	sp@-			| or code arg
	movl	d0,sp@-			| push trap type
	jbsr	_trap			| handle trap
	lea	sp@(12),sp		| pop value args
	movl	sp@(60),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most user regs
	addql	#6,sp			| pop SP and pad word
	jra	rei			| all done

	.globl	_straytrap
_badtrap:
	clrw	sp@-			| pad SR
	moveml	#0xC0C0,sp@-		| save scratch regs
	movw	sp@(24),sp@-		| push exception vector info
	clrw	sp@-
	movl	sp@(24),sp@-		| and PC
	jbsr	_straytrap		| report
	addql	#8,sp			| pop args
	moveml	sp@+,#0x0303		| restore regs
	addql	#2,sp			| pop padding
	jra	rei			| all done

	.globl	_syscall
_trap0:
	clrw	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(60)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall arg
	movl	sp@(60),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#6,sp			| pop SSP and align word
	jra	rei			| all done

/*
 * Our native 4.3 implementation uses trap 1 as sigreturn() and trap 2
 * as a breakpoint trap.
 */
_trap1:
	jra	sigreturn

_trap2:
	jra	_trace

/*
 * Trap 12 is the entry point for the cachectl "syscall"
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
	.globl	_cachectl
_trap12:
	movl	d1,sp@-			| push length
	movl	a1,sp@-			| push addr
	movl	d0,sp@-			| push command
	jbsr	_cachectl		| do it
	lea	sp@(12),sp		| pop args
	jra	rei			| all done

/*
 * Trap 15 is used for:
 *	- KGDB traps
 *	- trace traps for SUN binaries (not fully supported yet)
 * We just pass it on and let trap() sort it all out
 */
_trap15:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
#ifdef KGDB
	moveq	#T_TRAP15,d0
	movl	sp@(64),d1		| from user mode?
	andl	#PSL_S,d1
	jeq	fault
	movl	d0,sp@-
	.globl	_kgdb_trap_glue
	jbsr	_kgdb_trap_glue		| returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRAP15,d0
	jra	fault

/*
 * Hit a breakpoint (trap 1 or 2) instruction.
 * Push the code and treat as a normal fault.
 */
_trace:
	clrw	sp@-
	moveml	#0xFFFF,sp@-
#ifdef KGDB
	moveq	#T_TRACE,d0
	movl	sp@(64),d1		| from user mode?
	andl	#PSL_S,d1
	jeq	fault
	movl	d0,sp@-
	jbsr	_kgdb_trap_glue		| returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRACE,d0
	jra	fault

/*
 * The sigreturn() syscall comes here.  It requires special handling
 * because we must open a hole in the stack to fill in the (possibly much
 * larger) original stack frame.
 */
sigreturn:
	lea	sp@(-84),sp		| leave enough space for largest frame
	movl	sp@(84),sp@		| move up current 8 byte frame
	movl	sp@(88),sp@(4)
	movw	#84,sp@-		| default: adjust by 84 bytes
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(60)		|   in the savearea
	movl	#SYS_sigreturn,sp@-	| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall#
	movl	sp@(60),a0		| grab and restore
	movl	a0,usp			|   user SP
	lea	sp@(64),a1		| pointer to HW frame
	movw	a1@+,d0			| do we need to adjust the stack?
	jeq	Lsigr1			| no, just continue
	moveq	#92,d1			| total size
	subw	d0,d1			|  - hole size = frame size
	lea	a1@(92),a0		| destination
	addw	d1,a1			| source
	lsrw	#1,d1			| convert to word count
	subqw	#1,d1			| minus 1 for dbf
Lsigrlp:
	movw	a1@-,a0@-		| copy a word
	dbf	d1,Lsigrlp		| continue
	movl	a0,a1			| new HW frame base
Lsigr1:
	movl	a1,sp@(60)		| new SP value
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * Interrupt handlers.
 *
 *	Level 0:	Spurious: ignored.
 *	Level 1:	builtin-RS232 TBE, softint (not used yet)
 *	Level 2:	keyboard (CIA-A) + DMA + SCSI
 *	Level 3:	VBL
 *	Level 4:	not used
 *	Level 5:	builtin-RS232 RBF
 *	Level 6:	Clock (CIA-B-Timers)
 *	Level 7:	Non-maskable: shouldn't be possible. ignore.
 */

/* Provide a generic interrupt dispatcher, only handle hardclock (int6)
 * specially, to improve performance
 */

	.globl	_intrhand, _hardclock

_spurintr:
	addql	#1,_intrcnt+0
	addql	#1,_cnt+V_INTR
	jra	rei

_lev5intr:
	addql	#1,_intrcnt+20
	addql	#1,_cnt+V_INTR
	moveml	d0/d1/a0/a1,sp@-
	jsr	_ser_fastint
	moveml	sp@+,d0/d1/a0/a1
	rte
	
	
_lev1intr:
_lev2intr:
_lev3intr:
_lev4intr:
	clrw	sp@-
	moveml	#0xC0C0,sp@-
Lnotdma:
	lea	_intrcnt,a0
	movw	sp@(24),d0		| use vector offset
	andw	#0xfff,d0		|   sans frame type
	addql	#1,a0@(-0x60,d0:w)	|     to increment apropos counter
	movw	sr,sp@-			| push current SR value
	clrw	sp@-			|    padded to longword
	jbsr	_intrhand		| handle interrupt
	addql	#4,sp			| pop SR
	moveml	sp@+,#0x0303
	addql	#2,sp
	addql	#1,_cnt+V_INTR
	jra	rei

_lev6intr:
	clrw	sp@-
	moveml	#0xC0C0,sp@-
#if 0
#ifdef DEBUG
	.globl	_panicstr, _regdump, _panic
	tstl	timebomb		| set to go off?
	jeq	Lnobomb			| no, skip it
	subql	#1,timebomb		| decrement
	jne	Lnobomb			| not ready to go off
	moveml	sp@+,#0x0303		| temporarily restore regs
	jra	Lbomb			| go die
Lnobomb:
	cmpl	#_kstack+NBPG,sp	| are we still in stack pages?
	jcc	Lstackok		| yes, continue normally
	tstl	_curproc		| if !curproc could have swtch_exit'ed,
	jeq	Lstackok		|     might be on tmpstk
	tstl	_panicstr		| have we paniced?
	jne	Lstackok		| yes, do not re-panic
	lea	tmpstk,sp		| no, switch to tmpstk
	moveml	#0xFFFF,sp@-		| push all registers
	movl	#Lstkrip,sp@-		| push panic message
	jbsr	_printf			| preview
	addql	#4,sp
	movl	sp,a0			| remember this spot
	movl	#256,sp@-		| longword count
	movl	a0,sp@-			| and reg pointer
	jbsr	_regdump		| dump core
	addql	#8,sp			| pop params
	movl	#Lstkrip,sp@-		| push panic message
	jbsr	_panic			| ES and D
Lbomb:
	moveml	#0xFFFF,sp@-		| push all registers
	movl	sp,a0			| remember this spot
	movl	#256,sp@-		| longword count
	movl	a0,sp@-			| and reg pointer
	jbsr	_regdump		| dump core
	addql	#8,sp			| pop params
	movl	#Lbomrip,sp@-		| push panic message
	jbsr	_panic			| ES and D
Lstkrip:
	.asciz	"k-stack overflow"
Lbomrip:
	.asciz	"timebomb"
	.even
Lstackok:
#endif	/* DEBUG */
#endif
#if NZEUSSCSI > 0
	jbsr	_siopintr6		| check for siop (53C710) interrupt
	tstl	d0
	jne	Lskipciab		| XXX skip CIAB processing
#endif
	CIABADDR(a0)
	movb	a0@(CIAICR),d0		| read irc register (clears ints!)
	INTREQWADDR(a0)
	movew	#INTF_EXTER,a0@		| clear EXTER interrupt in intreq
#ifdef PROFTIMER
	.globl  _profon
	tstb	_profon			| profile clock on?
	jeq     LtimerA			| no, then must be timerA interrupt
	btst	#1,d0			| timerB interrupt?
	jeq     LtimerA			| no, must be timerA
	lea	sp@(16),a1		| get pointer to PS
#ifdef GPROF
	.globl	_profclock
	movl	d0,sp@-			| save status so jsr will not clobber
	movl	a1@,sp@-		| push padded PS
	movl	a1@(4),sp@-		| push PC
	jbsr	_profclock		| profclock(pc, ps)
	addql	#8,sp			| pop params
#else
	btst	#5,a1@(2)		| saved PS in user mode?
	jne	LttimerA		| no, go check timerA
	movl	_curpcb,a0		| current pcb
	tstl	a0@(U_PROFSCALE)	| process being profiled?
	jeq	LttimerA		| no, go check timerA
	movl	d0,sp@-			| save status so jsr will not clobber
	movl	#1,sp@-
	pea	a0@(U_PROF)
	movl	a1@(4),sp@-
	jbsr    _addupc			| addupc(pc, &u.u_prof, 1)
	lea	sp@(12),sp		| pop params
#endif
	addql	#1,_intrcnt+32		| add another profile clock interrupt
	movl	sp@+,d0			| get saved clock status
LttimerA:
	btst	#0,d0			| timerA interrupt?
	jeq     Ltimend		        | no, check state of kernel profiling
LtimerA:
#endif
	lea	sp@(16),a1		| get pointer to PS
	movl	a1@,sp@-		| push padded PS
	movl	a1@(4),sp@-		| push PC
	jbsr	_hardclock		| call generic clock int routine
	addql	#8,sp			| pop params
	addql	#1,_intrcnt+28		| add another system clock interrupt
#ifdef PROFTIMER
Ltimend:
#ifdef GPROF
	.globl	_profiling, _startprofclock
	tstl	_profiling		| kernel profiling desired?
	jne	Ltimdone		| no, all done
	bset	#7,_profon		| mark continuous timing
	jne	Ltimdone		| was already enabled, all done
	jbsr	_startprofclock		| else turn it on
Ltimdone:
#endif
#endif
Lskipciab:
	moveml	sp@+,#0x0303		| restore scratch regs
	addql	#2,sp			| pop pad word
	addql	#1,_cnt+V_INTR		| chalk up another interrupt
	jra	rei			| all done

_lev7intr:
#ifdef PROFTIMER
	addql	#1,_intrcnt+36
#else
	addql	#1,_intrcnt+32
#endif

#if 0
	clrw	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save registers
	movl	usp,a0			| and save
	movl	a0,sp@(60)		|   the user stack pointer
	jbsr	_nmihand		| call handler
	movl	sp@(60),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and remaining registers
	addql	#6,sp			| pop SSP and align word
#endif
	/* some amiga zorro2 boards seem to generate spurious NMIs. Best
	 * thing to do is to return as quick as possible. That's the
	 * reason why I do RTE here instead of jra rei.
	 */
	rte				| all done



/*
 * Emulation of VAX REI instruction.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.  A cleanup should only be needed at this
 * point for coprocessor mid-instruction frames (type 9), but we also test
 * for bus error frames (type 10 and 11).
 */
	.comm	_ssir,1
	.globl	_astpending
rei:
#ifdef DEBUG
	tstl	_panicstr		| have we paniced?
	jne	Ldorte			| yes, do not make matters worse
#endif
	tstl	_astpending		| AST pending?
	jeq	Lchksir			| no, go check for SIR
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Lchksir			| no, go check for SIR
	clrw	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(60)		|    the users SP
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_ASTFLT,sp@-		| type == async system trap
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(60),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and all remaining registers
	addql	#4,sp			| toss SSP
	tstw	sp@+			| do we need to clean up stack?
	jeq	Ldorte			| no, just continue
	btst	#7,sp@(6)		| type 9/10/11 frame?
	jeq	Ldorte			| no, nothing to do
	btst	#5,sp@(6)		| type 9?
	jne	Last1			| no, skip
	movw	sp@,sp@(12)		| yes, push down SR
	movl	sp@(2),sp@(14)		| and PC
	clrw	sp@(18)			| and mark as type 0 frame
	lea	sp@(12),sp		| clean the excess
	jra	Ldorte			| all done
Last1:
	btst	#4,sp@(6)		| type 10?
	jne	Last2			| no, skip
	movw	sp@,sp@(24)		| yes, push down SR
	movl	sp@(2),sp@(26)		| and PC
	clrw	sp@(30)			| and mark as type 0 frame
	lea	sp@(24),sp		| clean the excess
	jra	Ldorte			| all done
Last2:
	movw	sp@,sp@(84)		| type 11, push down SR
	movl	sp@(2),sp@(86)		| and PC
	clrw	sp@(90)			| and mark as type 0 frame
	lea	sp@(84),sp		| clean the excess
	jra	Ldorte			| all done
Lchksir:
	tstb	_ssir			| SIR pending?
	jeq	Ldorte			| no, all done
	movl	d0,sp@-			| need a scratch register
	movw	sp@(4),d0		| get SR
	andw	#PSL_IPL7,d0		| mask all but IPL
	jne	Lnosir			| came from interrupt, no can do
	movl	sp@+,d0			| restore scratch register
Lgotsir:
	movw	#SPL1,sr		| prevent others from servicing int
	tstb	_ssir			| too late?
	jeq	Ldorte			| yes, oh well...
	clrw	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(60)		|    the users SP
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_SSIR,sp@-		| type == software interrupt
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(60),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and all remaining registers
	addql	#6,sp			| pop SSP and align word
	rte
Lnosir:
	movl	sp@+,d0			| restore scratch register
Ldorte:
	rte				| real return

/*
 * Kernel access to the current processes kernel stack is via a fixed
 * virtual address.  It is at the same address as in the users VA space.
 * Umap contains the KVA of the first of UPAGES PTEs mapping VA _kstack.
 */
	.data
	.set	_kstack,-(UPAGES*NBPG)
_Umap:	.long	0
	.globl	_kstack, _Umap


/*
 * Initialization
 *
 * A5 contains physical load point from boot
 * exceptions vector thru our table, that's bad.. just hope nothing exceptional
 * happens till we had time to initialize ourselves..
 */
	.comm	_lowram,4

	.text
	.globl	_edata
	.globl	_etext,_end
	.globl	start
	.word	0x0001			| loadbsd version required
					| XXX should be a symbol?
start:
	movw	#PSL_HIGHIPL,sr		| no interrupts
	lea	tmpstk,sp		| give ourselves a temporary stack

	| save the passed parameters. `prepass' them on the stack for
	| later catch by _start_c
	movel	d1,sp@-
	movel	d0,sp@-
	movel	a0,sp@-			| pass fastmem_start and _len and chipmem-len
	movel	d5,sp@-			| pass machine id

	movl	#CACHE_OFF,d0		| 68020/030 cache
	movl	#AMIGA_68040,d1
	andl	d1,d5
	movl	d5,_cpu040		| set 68040 CPU flag
	jeq	Lstartnot040		| it's not 68040
	.word	0xf4f8		| cpusha bc - push and invalidate caches
	lea	Lvectab+0xc0,a0		| set up 68040 floating point
	movl	#fpsp_bsun,a0@+		|  exception vectors
	movl	#real_inex,a0@+
	movl	#real_dz,a0@+
	movl	#fpsp_unfl,a0@+
	movl	#fpsp_operr,a0@+
	movl	#fpsp_ovfl,a0@+
	movl	#fpsp_snan,a0@+
	movl	#fpsp_unsupp,a0@+

	movl	#CACHE40_OFF,d0		| 68040 cache disable
Lstartnot040:
	movc	d0,cacr			| clear and disable on-chip cache(s)
	moveq	#0,d0
	movc	d0,vbr

#if 1
	| WHY THE @#$@#$@ DOESN'T THIS WORK????????

	| add code to determine MMU. This should be passed from
	| AmigaOS really...
	movl	#0x200,d0		| data freeze bit
	movc	d0,cacr			|   only exists on 68030
	movc	cacr,d0			| read it back
	tstl	d0			| zero?
	jeq	Lis68020		| yes, we have 68020
	| movl	#-1,_mmutype		| no, we have 68030
	jra	Lskip
Lis68020:
	| movl	#1,_mmutype		| hope we have 68851...
Lskip:
	movl	#CACHE_OFF,d0
	tstl	d5			| running on 68040?
	jeq	Lcacheoff		| no
	movl	#CACHE40_OFF,d0		| 68040 cache enable
Lcacheoff:
	movc	d0,cacr
#endif

/* initialize source/destination control registers for movs */
	moveq	#FC_USERD,d0		| user space
	movc	d0,sfc			|   as source
	movc	d0,dfc			|   and destination of transfers

/* let the C function initialize everything and enable the MMU */
	jsr	_start_c
	addl	#16,sp

/* set kernel stack, user SP, and initial pcb */
	lea	_kstack,a1		| proc0 kernel stack
	lea	a1@(UPAGES*NBPG-4),sp	| set kernel stack to end of area
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP
	movl	_proc0paddr,a1		| get proc0 pcb addr
	movl	a1,_curpcb		| proc0 is running
	clrw	a1@(PCB_FLAGS)		| clear flags
#ifdef FPCOPROC
	clrl	a1@(PCB_FPCTX)		| ensure null FP context
|WRONG!	movl	a1,sp@-
	pea	a1@(PCB_FPCTX)
	jbsr	_m68881_restore		| restore it (does not kill a1)
	addql	#4,sp
#endif
/* flush TLB and turn on caches */
	jbsr	_TBIA			| invalidate TLB
	movl	#CACHE_ON,d0
	tstl	d5
	jeq	Lcacheon
| is this needed? MLH
	.word	0xf4f8		| cpusha bc - push & invalidate caches
	movl	#CACHE40_ON,d0
Lcacheon:
	movc	d0,cacr			| clear cache(s)
/* final setup for C code */


	movw	#PSL_LOWIPL,sr		| lower SPL

	movl	d7,_boothowto		| save reboot flags
/*
	movl	d6,_bootdev		|   and boot device
*/
	jbsr	_main			| call main()

/* proc[1] == init now running here;
 * create a null exception frame and return to user mode in icode
 */
	tstl	d5
	jeq	Lstartinit
| icode was copied to location 0, so need to push caches
	.word	0xf4f8		| cpusha bc
Lstartinit:
	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| return to icode location 0
	movw	#PSL_USER,sp@-		| in user mode
	rte

/*
 * Signal "trampoline" code (18 bytes).  Invoked from RTE setup by sendsig().
 *
 * Stack looks like:
 *
 *	sp+0 ->	signal number
 *	sp+4	signal specific code
 *	sp+8	pointer to signal context frame (scp)
 *	sp+12	address of handler
 *	sp+16	saved hardware state
 *			.
 *			.
 *	scp+0->	beginning of signal context frame
 */
	.globl	_sigcode, _esigcode
	.data
_sigcode:
	movl	sp@(12),a0		| signal handler addr	(4 bytes)
	jsr	a0@			| call signal handler	(2 bytes)
	addql	#4,sp			| pop signo		(2 bytes)
	trap	#1			| special syscall entry	(2 bytes)
	movl	d0,sp@(4)		| save errno		(4 bytes)
	moveq	#1,d0			| syscall == exit	(2 bytes)
	trap	#0			| exit(errno)		(2 bytes)
	.align	2
_esigcode:

/*
 * Icode is copied out to process 1 to exec init.
 * If the exec fails, process 1 exits.
 */
	.globl	_icode,_szicode
	.text
_icode:
	jra	st1
init:
	.asciz	"/sbin/init"
	.byte	0			| GNU ``as'' bug won't
					| allow an .even directive 
					| here, says something about
					| non constant, which is crap.
argv:
	.long	init+6-_icode	| argv[0] = "init" ("/sbin/init" + 6)
	.long	eicode-_icode	| argv[1] follows icode after copyout
	.long	0

st1:	clrl	sp@-
	.set	argvrpc,argv-.-2	| avoids PCREL bugs in ``as''
					| otherwise it assebles different
					| depending on your version of
					| GNU ``as''.  Markus' as has a 
					| nice one:
					| 	a:	pea pc@(0) 
					|is equivelent to:
					|	a:	pea pc@(a-.)
					|is equivelent to!: 
					|	a:	pea pc@(b-.)
					|	b:	....
					|---
					| Mine (2.2.1) doesn't work right 
					| for #3 mine puts out
					|	a:	pea pc@(2)
					| and it should be pc@(4). 
					| cest la vie.
	pea	pc@(argvrpc)
	.set	initrpc,init-.-2	| avoids PCREL bugs in ``as''
					| see above comment	
	pea	pc@(initrpc)
	clrl	sp@-
	moveq	#SYS_execve,d0
	trap	#0
	moveq	#SYS_exit,d0
	trap	#0
eicode:

_szicode:
	.long	_szicode-_icode

/*
 * Primitives
 */

#ifdef GPROF
#define	ENTRY(name) \
	.globl _/**/name; _/**/name: link a6,#0; jbsr mcount; unlk a6
#define ALTENTRY(name, rname) \
	ENTRY(name); jra rname+12
#else
#define	ENTRY(name) \
	.globl _/**/name; _/**/name:
#define ALTENTRY(name, rname) \
	.globl _/**/name; _/**/name:
#endif

/*
 * update profiling information for the user
 * addupc(pc, &u.u_prof, ticks)
 */
ENTRY(addupc)
	movl	a2,sp@-			| scratch register
	movl	sp@(12),a2		| get &u.u_prof
	movl	sp@(8),d0		| get user pc
	subl	a2@(8),d0		| pc -= pr->pr_off
	jlt	Lauexit			| less than 0, skip it
	movl	a2@(12),d1		| get pr->pr_scale
	lsrl	#1,d0			| pc /= 2
	lsrl	#1,d1			| scale /= 2
	mulul	d1,d0			| pc /= scale
	moveq	#14,d1
	lsrl	d1,d0			| pc >>= 14
	bclr	#0,d0			| pc &= ~1
	cmpl	a2@(4),d0		| too big for buffer?
	jge	Lauexit			| yes, screw it
	addl	a2@,d0			| no, add base
	movl	d0,sp@-			| push address
	jbsr	_fusword		| grab old value
	movl	sp@+,a0			| grab address back
	cmpl	#-1,d0			| access ok
	jeq	Lauerror		| no, skip out
	addw	sp@(18),d0		| add tick to current value
	movl	d0,sp@-			| push value
	movl	a0,sp@-			| push address
	jbsr	_susword		| write back new value
	addql	#8,sp			| pop params
	tstl	d0			| fault?
	jeq	Lauexit			| no, all done
Lauerror:
	clrl	a2@(12)			| clear scale (turn off prof)
Lauexit:
	movl	sp@+,a2			| restore scratch reg
	rts

/*
 * non-local gotos
 */
ENTRY(setjmp)
	movl	sp@(4),a0	| savearea pointer
	moveml	#0xFCFC,a0@	| save d2-d7/a2-a7
	movl	sp@,a0@(48)	| and return address
	moveq	#0,d0		| return 0
	rts

ENTRY(qsetjmp)
	movl	sp@(4),a0	| savearea pointer
	lea	a0@(40),a0	| skip regs we do not save
	movl	a6,a0@+		| save FP
	movl	sp,a0@+		| save SP
	movl	sp@,a0@		| and return address
	moveq	#0,d0		| return 0
	rts

ENTRY(longjmp)
	movl	sp@(4),a0
	moveml	a0@+,#0xFCFC
	movl	a0@,sp@
	moveq	#1,d0
	rts

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs
 * have processes in them.  Setrq puts processes into queues, Remrq
 * removes them from queues.  The running process is on no queue,
 * other processes are on a queue related to p->p_pri, divided by 4
 * actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */

	.globl	_whichqs,_qs,_cnt,_panic
	.globl	_curproc
	.comm	_want_resched,4

/*
 * Setrq(p)
 *
 * Call should be made at spl6(), and p->p_stat should be SRUN
 */
ENTRY(setrq)
	movl	sp@(4),a0
	tstl	a0@(P_RLINK)
	jeq	Lset1
	movl	#Lset2,sp@-
	jbsr	_panic
Lset1:
	clrl	d0
	movb	a0@(P_PRI),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
	lslb	#3,d0
	addl	#_qs,d0
	movl	d0,a0@(P_LINK)
	movl	d0,a1
	movl	a1@(P_RLINK),a0@(P_RLINK)
	movl	a0,a1@(P_RLINK)
	movl	a0@(P_RLINK),a1
	movl	a0,a1@(P_LINK)
	rts

Lset2:
	.asciz	"setrq"
	.even

/*
 * Remrq(p)
 *
 * Call should be made at spl6().
 */
ENTRY(remrq)
	movl	sp@(4),a0
	clrl	d0
	movb	a0@(P_PRI),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bclr	d0,d1
	jne	Lrem1
	movl	#Lrem3,sp@-
	jbsr	_panic
Lrem1:
	movl	d1,_whichqs
	movl	a0@(P_LINK),a1
	movl	a0@(P_RLINK),a1@(P_RLINK)
	movl	a0@(P_RLINK),a1
	movl	a0@(P_LINK),a1@(P_LINK)
	movl	#_qs,a1
	movl	d0,d1
	lslb	#3,d1
	addl	d1,a1
	cmpl	a1@(P_LINK),a1
	jeq	Lrem2
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
Lrem2:
	clrl	a0@(P_RLINK)
	rts

Lrem3:
	.asciz	"remrq"
Lsw0:
	.asciz	"swtch"
	.even

	.globl	_curpcb
	.globl	_masterpaddr	| XXX compatibility (debuggers)
	.data
_masterpaddr:			| XXX compatibility (debuggers)
_curpcb:
	.long	0
pcbflag:
	.byte	0		| copy of pcb_flags low byte
	.align	2
	.comm	nullpcb,SIZEOF_PCB
	.text

/*
 * At exit of a process, do a swtch for the last time.
 * The mapping of the pcb at p->p_addr has already been deleted,
 * and the memory for the pcb+stack has been freed.
 * The ipl is high enough to prevent the memory from being reallocated.
 */
ENTRY(swtch_exit)
	movl	#nullpcb,_curpcb	| save state into garbage pcb
	lea	tmpstk,sp		| goto a tmp stack
	jra	_swtch

/*
 * When no processes are on the runq, Swtch branches to idle
 * to wait for something to come ready.
 */
	.globl	Idle
Lidle:
	stop	#PSL_LOWIPL
Idle:
idle:
	movw	#PSL_HIGHIPL,sr
	tstl	_whichqs
	jeq	Lidle
	movw	#PSL_LOWIPL,sr
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * Swtch()
 *
 * NOTE: On the mc68851 (318/319/330) we attempt to avoid flushing the
 * entire ATC.  The effort involved in selective flushing may not be
 * worth it, maybe we should just flush the whole thing?
 *
 * NOTE 2: With the new VM layout we now no longer know if an inactive
 * user's PTEs have been changed (formerly denoted by the SPTECHG p_flag
 * bit).  For now, we just always flush the full ATC.
 */
ENTRY(swtch)
	movl	_curpcb,a0		| current pcb
	movw	sr,a0@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		| remember last proc running
#endif
	clrl	_curproc
	addql	#1,_cnt+V_SWTCH
Lsw1:
	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	clrl	d0
	lea	_whichqs,a0
	movl	a0@,d1
Lswchk:
	btst	d0,d1
	jne	Lswfnd
	addqb	#1,d0
	cmpb	#32,d0
	jne	Lswchk
	jra	idle
Lswfnd:
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	movl	a0@,d1			| and check again...
	bclr	d0,d1
	jeq	Lsw1			| proc moved, rescan
	movl	d1,a0@			| update whichqs
	moveq	#1,d1			| double check for higher priority
	lsll	d0,d1			| process (which may have snuck in
	subql	#1,d1			| while we were finding this one)
	andl	a0@,d1
	jeq	Lswok			| no one got in, continue
	movl	a0@,d1
	bset	d0,d1			| otherwise put this one back
	movl	d1,a0@
	jra	Lsw1			| and rescan
Lswok:
	movl	d0,d1
	lslb	#3,d1			| convert queue number to index
	addl	#_qs,d1			| locate queue (q)
	movl	d1,a1
	cmpl	a1@(P_LINK),a1		| anyone on queue?
	jeq	Lbadsw			| no, panic
	movl	a1@(P_LINK),a0			| p = q->p_link
	movl	a0@(P_LINK),a1@(P_LINK)		| q->p_link = p->p_link
	movl	a0@(P_LINK),a1			| q = p->p_link
	movl	a0@(P_RLINK),a1@(P_RLINK)	| q->p_rlink = p->p_rlink
	cmpl	a0@(P_LINK),d1		| anyone left on queue?
	jeq	Lsw2			| no, skip
	movl	_whichqs,d1
	bset	d0,d1			| yes, reset bit
	movl	d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1
	cmpl	a0,a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif
	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_curpcb,a1
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	usp,a2			| grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		| and save it
	movl	_CMAP2,a1@(PCB_CMAP2)	| save temporary map PTE
#ifdef FPCOPROC
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a2@(312)	| save FP control registers
Lswnofpsave:
#endif

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_RLINK)		| clear back link
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_curpcb
	movb	a1@(PCB_FLAGS+1),pcbflag | copy of pcb_flags low byte

	/* see if pmap_activate needs to be called; should remove this */
	movl	a0@(P_VMSPACE),a0	| vmspace = p->p_vmspace
#ifdef DIAGNOSTIC
	tstl	a0			| map == VM_MAP_NULL?
	jeq	Lbadsw			| panic
#endif
	lea	a0@(VM_PMAP),a0		| pmap = &vmspace.vm_pmap
	tstl	a0@(PM_STCHG)		| pmap->st_changed?
	jeq	Lswnochg		| no, skip
	pea	a1@			| push pcb (at p_addr)
	pea	a0@			| push pmap
	jbsr	_pmap_activate		| pmap_activate(pmap, pcb)
	addql	#8,sp
	movl	_curpcb,a1		| restore p_addr
Lswnochg:

#ifdef PROFTIMER
#ifdef notdef
	movw	#SPL6,sr		| protect against clock interrupts
#endif
	bclr	#0,_profon		| clear user profiling bit, was set?
	jeq	Lskipoff		| no, clock off or doing kernel only
#ifdef GPROF
	tstb	_profon			| kernel profiling also enabled?
	jlt	Lskipoff		| yes, nothing more to do
#endif
	CIABADDR(a0)
	moveb	a0@(CIACRB),d0		| turn off timer B in CIAB
	andb	#0xc0,d0
	moveb	d0,a0@(CIACRB)

Lskipoff:
#endif
	movl	#PGSHIFT,d1
	movl	a1,d0
	lsrl	d1,d0			| convert p_addr to page number
	lsll	#2,d0			| and now to Systab offset
	addl	_Sysmap,d0		| add Systab base to get PTE addr
#ifdef notdef
	movw	#PSL_HIGHIPL,sr		| go crit while changing PTEs
#endif
	lea	tmpstk,sp		| now goto a tmp stack for NMI
	movl	d0,a0			| address of new context
	movl	_Umap,a2		| address of PTEs for kstack
	moveq	#UPAGES-1,d0		| sizeof kstack
Lres1:
	movl	a0@+,d1			| get PTE
	andl	#~PG_PROT,d1		| mask out old protection
	orl	#PG_RW+PG_V,d1		| ensure valid and writable
	movl	d1,a2@+			| load it up
	dbf	d0,Lres1		| til done
	tstl	_cpu040
	jne	Lres2
	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	pflusha				| flush entire TLB
	jra	Lres3
Lres2:
	.word	0xf4f8		| cpusha bc
	.word	0xf518		| pflusha (68040)
	movl	#CACHE40_ON,d0
	movc	d0,cacr			| invalidate cache(s)
Lres3:
	movl	a1@(PCB_USTP),d0	| get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	tstl	_cpu040
	jne	Lres4
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load new user root pointer
	jra	Lres5
Lres4:
	.word	0x4e7b,0x0806	| movc d0,URP
Lres5:
	movl	a1@(PCB_CMAP2),_CMAP2	| reload tmp map
	moveml	a1@(PCB_REGS),#0xFCFC	| and registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP
#ifdef PROFTIMER
	tstl	a1@(U_PROFSCALE)	| process being profiled?
	jeq	Lskipon			| no, do nothing
	orb	#1,_profon		| turn on user profiling bit
#ifdef GPROF
	jlt	Lskipon			| already profiling kernel, all done
#endif
	CIABADDR(a0)
	movl	_profint,d1		| profiling interval
	moveb	d1,a0@(CIATBLO)
	lsr	#8,d1
	moveb	d1,a0@(CIATBHI)		| set interval
	moveb	#(1<<7)|(1<<1),a0@(CIAICR) | enable interrupts for timer B
	moveb	a0@(CIACRB),d0
	andb	#0xc0,d0
	bset	#0,d0
	moveb	d0,a0@(CIACRB)		| start timer B in continuous shot mode
Lskipon:
#endif
#ifdef FPCOPROC
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	tstb	a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lresfprest:
	frestore a0@			| restore state
#endif
	movw	a1@(PCB_PS),sr		| no, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts

/*
 * savectx(pcb, altreturn)
 * Update pcb, saving current processor state and arranging
 * for alternate return ala longjmp in swtch if altreturn is true.
 */
ENTRY(savectx)
	movl	sp@(4),a1
	movw	sr,a1@(PCB_PS)
	movl	usp,a0			| grab USP
	movl	a0,a1@(PCB_USP)		| and save it
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	_CMAP2,a1@(PCB_CMAP2)	| save temporary map PTE
#ifdef FPCOPROC
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	fsave	a0@			| save FP state
	tstb	a0@			| null state frame?
	jeq	Lsvnofpsave		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lsvnofpsave:
#endif
	tstl	sp@(8)			| altreturn?
	jeq	Lsavedone
	movl	sp,d0			| relocate current sp relative to a1
	subl	#_kstack,d0		|   (sp is relative to kstack):
	addl	d0,a1			|   a1 += sp - kstack;
	movl	sp@,a1@			| write return pc at (relocated) sp@
Lsavedone:
	moveq	#0,d0			| return 0
	rts

/*
 * Copy 1 relocation unit (NBPG bytes)
 * from user virtual address to physical address
 */
ENTRY(copyseg)
	movl	_curpcb,a1			| current pcb
	movl	#Lcpydone,a1@(PCB_ONFAULT)	| where to return to on a fault
	movl	sp@(8),d0			| destination page number
	moveq	#PGSHIFT,d1
	lsll	d1,d0				| convert to address
	orl	#PG_CI+PG_RW+PG_V,d0		| make sure valid and writable
	movl	_CMAP2,a0
	movl	_CADDR2,sp@-			| destination kernel VA
	movl	d0,a0@				| load in page table
	jbsr	_TBIS				| invalidate any old mapping
	addql	#4,sp
	movl	_CADDR2,a1			| destination addr
	movl	sp@(4),a0			| source addr
	movl	#NBPG/4-1,d0			| count
Lcpyloop:
	movsl	a0@+,d1				| read longword
	movl	d1,a1@+				| write longword
	dbf	d0,Lcpyloop			| continue until done
Lcpydone:
	movl	_curpcb,a1			| current pcb
	clrl	a1@(PCB_ONFAULT) 		| clear error catch
	rts

/*
 * Copy 1 relocation unit (NBPG bytes)
 * from physical address to physical address
 */
ENTRY(physcopyseg)
	movl	sp@(4),d0			| source page number
	moveq	#PGSHIFT,d1
	lsll	d1,d0				| convert to address
	orl	#PG_CI+PG_RW+PG_V,d0		| make sure valid and writable
	movl	_CMAP1,a0
	movl	d0,a0@				| load in page table
	movl	_CADDR1,sp@-			| destination kernel VA
	jbsr	_TBIS				| invalidate any old mapping
	addql	#4,sp

	movl	sp@(8),d0			| destination page number
	moveq	#PGSHIFT,d1
	lsll	d1,d0				| convert to address
	orl	#PG_CI+PG_RW+PG_V,d0		| make sure valid and writable
	movl	_CMAP2,a0
	movl	d0,a0@				| load in page table
	movl	_CADDR2,sp@-			| destination kernel VA
	jbsr	_TBIS				| invalidate any old mapping
	addql	#4,sp

	movl	_CADDR1,a0			| source addr
	movl	_CADDR2,a1			| destination addr
	movl	#NBPG/4-1,d0			| count
Lpcpy:
	movl	a0@+,a1@+			| copy longword
	dbf	d0,Lpcpy			| continue until done
	rts

/*
 * zero out physical memory
 * specified in relocation units (NBPG bytes)
 */
ENTRY(clearseg)
	movl	sp@(4),d0			| destination page number
	moveq	#PGSHIFT,d1
	lsll	d1,d0				| convert to address
	orl	#PG_CI+PG_RW+PG_V,d0		| make sure valid and writable
	movl	_CMAP1,a0
	movl	_CADDR1,sp@-			| destination kernel VA
	movl	d0,a0@				| load in page map
	jbsr	_TBIS				| invalidate any old mapping
	addql	#4,sp
	movl	_CADDR1,a1			| destination addr
	movl	#NBPG/4-1,d0			| count
/* simple clear loop is fastest on 68020 */
Lclrloop:
	clrl	a1@+				| clear a longword
	dbf	d0,Lclrloop			| continue til done
	rts

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
__TBIA:
	tstl	_cpu040
	jne	Ltbia040
	pflusha				| flush entire TLB
	tstl	_mmutype
	jpl	Lmc68851a		| 68851 implies no d-cache
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
Lmc68851a:
	rts
Ltbia040:
	.word	0xf518		| pflusha
	.word	0xf478		| cpush dc [cinv or cpush ??]
	rts

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush entire TLB
#endif
	movl	sp@(4),a0		| get addr to flush
	tstl	_cpu040
	jne	Ltbis040
	tstl	_mmutype
	jpl	Lmc68851b		| is 68851?
	pflush	#0,#0,a0@		| flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip data cache
	rts
Lmc68851b:
	pflushs	#0,#0,a0@		| flush address from both sides
	rts
Ltbis040:
	moveq	#FC_SUPERD,d0		| select supervisor
	movc	d0,dfc
	.word	0xf508		| pflush a0@
	moveq	#FC_USERD,d0		| select user
	movc	d0,dfc
	.word	0xf508		| pflush a0@
	.word	0xf478		| cpusha dc [cinv or cpush ??]
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
	tstl	_cpu040
	jne	Ltbias040
	tstl	_mmutype
	jpl	Lmc68851c		| 68851?
	pflush #4,#4			| flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Lmc68851c:
	pflushs #4,#4			| flush supervisor TLB entries
	rts
Ltbias040:
| 68040 can't specify supervisor/user on pflusha, so we flush all
	.word	0xf518		| pflusha
	.word	0xf478		| cpusha dc [cinv or cpush ??]
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
	tstl	_cpu040
	jne	Ltbiau040
	tstl	_mmutype
	jpl	Lmc68851d		| 68851?
	pflush	#0,#4			| flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Lmc68851d:
	pflushs	#0,#4			| flush user TLB entries
	rts
Ltbiau040:
| 68040 can't specify supervisor/user on pflusha, so we flush all
	.word	0xf518		| pflusha
	.word	0xf478		| cpusha dc [cinv or cpush ??]
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
	tstl	_cpu040
	jne	Licia040
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts
Licia040:
	.word	0xf498		| cinva ic
	rts

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
__DCIA:
	tstl	_cpu040
	jeq	Ldciax
	.word	0xf478		| cpusha dc
Ldciax:
	rts

ENTRY(DCIS)
__DCIS:
	tstl	_cpu040
	jeq	Ldcisx
	.word	0xf478		| cpusha dc
	nop
Ldcisx:
	rts

ENTRY(DCIU)
__DCIU:
	tstl	_cpu040
	jeq	Ldciux
	.word	0xf478		| cpusha dc
Ldciux:
	rts

| Invalid single cache line
ENTRY(DCIAS)
__DCIAS:
	tstl	_cpu040
	jeq	Ldciasx
	movl	sp@(4),a0
	.word	0xf468		| cpushl dc,a0@
Ldciasx:
	rts

ENTRY(PCIA)
	tstl	_cpu040
	jne	Lpcia040
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Lpcia040:
	.word	0xf478		| cpusha dc
	rts

ENTRY(ecacheon)
	rts

ENTRY(ecacheoff)
	rts

/*
 * Get callers current SP value.
 * Note that simply taking the address of a local variable in a C function
 * doesn't work because callee saved registers may be outside the stack frame
 * defined by A6 (e.g. GCC generated code).
 */
	.globl	_getsp
_getsp:
	movl	sp,d0			| get current SP
	addql	#4,d0			| compensate for return address
	rts

	.globl	_getsfc, _getdfc
_getsfc:
	movc	sfc,d0
	rts
_getdfc:
	movc	dfc,d0
	rts

/*
 * Check out a virtual address to see if it's okay to write to.
 *
 * probeva(va, fc)
 *
 */
ENTRY(probeva)
	movl	sp@(8),d0
	movec	d0,dfc
	movl	sp@(4),a0
	.word	0xf548		| ptestw (a0)
	moveq	#FC_USERD,d0		| restore DFC to user space
	movc	d0,dfc
	.word	0x4e7a,0x0805	| movec  MMUSR,d0
	rts

/*
 * Load a new user segment table pointer.
 */
ENTRY(loadustp)
	movl	sp@(4),d0		| new USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	tstl	_cpu040
	jne	Lldustp040
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load root pointer
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts				|   since pmove flushes TLB
Lldustp040:
	.word	0xf478		| cpush dc
	.word	0x4e7b,0x0806	| movec d0,URP
	rts

/*
 * Flush any hardware context associated with given USTP.
 * Only does something for HP330 where we must flush RPT
 * and ATC entries in PMMU.
 */
ENTRY(flushustp)
	tstl	_cpu040
	jne	Lnot68851
	tstl	_mmutype		| 68851 PMMU?
	jle	Lnot68851		| no, nothing to do
	movl	sp@(4),d0		| get USTP to flush
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to address
	movl	d0,_protorp+4		| stash USTP
	pflushr	_protorp		| flush RPT/TLB entries
Lnot68851:
	rts

ENTRY(ploadw)
	movl	sp@(4),a0		| address to load
	tstl	_cpu040
	jne	Lploadw040
	ploadw	#1,a0@			| pre-load translation
Lploadw040:			| should 68040 do a ptest?
	rts

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, spl0 requires special handling
 * as we need to check for our emulated software interrupts.
 */

ENTRY(spl0)
	moveq	#0,d0
	movw	sr,d0			| get old SR for return
	movw	#PSL_LOWIPL,sr		| restore new SR
	tstb	_ssir			| software interrupt pending?
	jeq	Lspldone		| no, all done
	subql	#4,sp			| make room for RTE frame
	movl	sp@(4),sp@(2)		| position return address
	clrw	sp@(6)			| set frame type 0
	movw	#PSL_LOWIPL,sp@		| and new SR
	jra	Lgotsir			| go handle it
Lspldone:
	rts

ENTRY(_insque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		| atomic
	movl	sp@(8),a0		| where to insert (after)
	movl	sp@(4),a1		| element to insert (e)
	movl	a0@,a1@			| e->next = after->next
	movl	a0,a1@(4)		| e->prev = after
	movl	a1,a0@			| after->next = e
	movl	a1@,a0
	movl	a1,a0@(4)		| e->next->prev = e
	movw	d0,sr
	rts

ENTRY(_remque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		| atomic
	movl	sp@(4),a0		| element to remove (e)
	movl	a0@,a1
	movl	a0@(4),a0
	movl	a0,a1@(4)		| e->next->prev = e->prev
	movl	a1,a0@			| e->prev->next = e->next
	movw	d0,sr
	rts

/*
 * bzero(addr, count)
 */
ALTENTRY(blkclr, _bzero)
ENTRY(bzero)
	movl	sp@(4),a0	| address
	movl	sp@(8),d0	| count
	jeq	Lbzdone		| if zero, nothing to do
	movl	a0,d1
	btst	#0,d1		| address odd?
	jeq	Lbzeven		| no, can copy words
	clrb	a0@+		| yes, zero byte to get to even boundary
	subql	#1,d0		| decrement count
	jeq	Lbzdone		| none left, all done
Lbzeven:
	movl	d0,d1
	andl	#31,d0
	lsrl	#5,d1		| convert count to 8*longword count
	jeq	Lbzbyte		| no such blocks, zero byte at a time
Lbzloop:
	clrl	a0@+; clrl	a0@+; clrl	a0@+; clrl	a0@+;
	clrl	a0@+; clrl	a0@+; clrl	a0@+; clrl	a0@+;
	subql	#1,d1		| one more block zeroed
	jne	Lbzloop		| more to go, do it
	tstl	d0		| partial block left?
	jeq	Lbzdone		| no, all done
Lbzbyte:
	clrb	a0@+
	subql	#1,d0		| one more byte cleared
	jne	Lbzbyte		| more to go, do it
Lbzdone:
	rts

/*
 * strlen(str)
 */
ENTRY(strlen)
	moveq	#-1,d0
	movl	sp@(4),a0	| string
Lslloop:
	addql	#1,d0		| increment count
	tstb	a0@+		| null?
	jne	Lslloop		| no, keep going
	rts

/*
 * bcmp(s1, s2, len)
 *
 * WARNING!  This guy only works with counts up to 64K
 */
ENTRY(bcmp)
	movl	sp@(4),a0		| string 1
	movl	sp@(8),a1		| string 2
	moveq	#0,d0
	movw	sp@(14),d0		| length
	jeq	Lcmpdone		| if zero, nothing to do
	subqw	#1,d0			| set up for DBcc loop
Lcmploop:
	cmpmb	a0@+,a1@+		| equal?
	dbne	d0,Lcmploop		| yes, keep going
	addqw	#1,d0			| +1 gives zero on match
Lcmpdone:
	rts

/*
 * {ov}bcopy(from, to, len)
 *
 * Works for counts up to 128K.
 */
ALTENTRY(ovbcopy, _bcopy)
ENTRY(bcopy)
	movl	sp@(12),d0		| get count
	jeq	Lcpyexit		| if zero, return
	movl	sp@(4),a0		| src address
	movl	sp@(8),a1		| dest address
	cmpl	a1,a0			| src before dest?
	jlt	Lcpyback		| yes, copy backwards (avoids overlap)
	movl	a0,d1
	btst	#0,d1			| src address odd?
	jeq	Lcfeven			| no, go check dest
	movb	a0@+,a1@+		| yes, copy a byte
	subql	#1,d0			| update count
	jeq	Lcpyexit		| exit if done
Lcfeven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	jne	Lcfbyte			| yes, must copy by bytes
	movl	d0,d1			| no, get count
	lsrl	#2,d1			| convert to longwords
	jeq	Lcfbyte			| no longwords, copy bytes
	subql	#1,d1			| set up for dbf
Lcflloop:
	movl	a0@+,a1@+		| copy longwords
	dbf	d1,Lcflloop		| til done
	andl	#3,d0			| get remaining count
	jeq	Lcpyexit		| done if none
Lcfbyte:
	subql	#1,d0			| set up for dbf
Lcfbloop:
	movb	a0@+,a1@+		| copy bytes
	dbf	d0,Lcfbloop		| til done
Lcpyexit:
	rts
Lcpyback:
	addl	d0,a0			| add count to src
	addl	d0,a1			| add count to dest
	movl	a0,d1
	btst	#0,d1			| src address odd?
	jeq	Lcbeven			| no, go check dest
	movb	a0@-,a1@-		| yes, copy a byte
	subql	#1,d0			| update count
	jeq	Lcpyexit		| exit if done
Lcbeven:
	movl	a1,d1
	btst	#0,d1			| dest address odd?
	jne	Lcbbyte			| yes, must copy by bytes
	movl	d0,d1			| no, get count
	lsrl	#2,d1			| convert to longwords
	jeq	Lcbbyte			| no longwords, copy bytes
	subql	#1,d1			| set up for dbf
Lcblloop:
	movl	a0@-,a1@-		| copy longwords
	dbf	d1,Lcblloop		| til done
	andl	#3,d0			| get remaining count
	jeq	Lcpyexit		| done if none
Lcbbyte:
	subql	#1,d0			| set up for dbf
Lcbbloop:
	movb	a0@-,a1@-		| copy bytes
	dbf	d0,Lcbbloop		| til done
	rts

/*
 * Emulate fancy VAX string operations:
 *	scanc(count, startc, table, mask)
 *	skpc(mask, count, startc)
 *	locc(mask, count, startc)
 */
ENTRY(scanc)
	movl	sp@(4),d0	| get length
	jeq	Lscdone		| nothing to do, return
	movl	sp@(8),a0	| start of scan
	movl	sp@(12),a1	| table to compare with
	movb	sp@(19),d1	| and mask to use
	movw	d2,sp@-		| need a scratch register
	clrw	d2		| clear it out
	subqw	#1,d0		| adjust for dbra
Lscloop:
	movb	a0@+,d2		| get character
	movb	a1@(0,d2:w),d2	| get table entry
	andb	d1,d2		| mask it
	dbne	d0,Lscloop	| keep going til no more or non-zero
	addqw	#1,d0		| overshot by one
	movw	sp@+,d2		| restore scratch
Lscdone:
	rts

ENTRY(skpc)
	movl	sp@(8),d0	| get length
	jeq	Lskdone		| nothing to do, return
	movb	sp@(7),d1	| mask to use
	movl	sp@(12),a0	| where to start
	subqw	#1,d0		| adjust for dbcc
Lskloop:
	cmpb	a0@+,d1		| compate with mask
	dbne	d0,Lskloop	| keep going til no more or zero
	addqw	#1,d0		| overshot by one
Lskdone:
	rts

ENTRY(locc)
	movl	sp@(8),d0	| get length
	jeq	Llcdone		| nothing to do, return
	movb	sp@(7),d1	| mask to use
	movl	sp@(12),a0	| where to start
	subqw	#1,d0		| adjust for dbcc
Llcloop:
	cmpb	a0@+,d1		| compate with mask
	dbeq	d0,Llcloop	| keep going til no more or non-zero
	addqw	#1,d0		| overshot by one
Llcdone:
	rts

/*
 * Emulate VAX FFS (find first set) instruction.
 */
ENTRY(ffs)
	moveq	#-1,d0
	movl	sp@(4),d1
	jeq	Lffsdone
Lffsloop:
	addql	#1,d0
	btst	d0,d1
	jeq	Lffsloop
Lffsdone:
	addql	#1,d0
	rts

#ifdef FPCOPROC
/*
 * Save and restore 68881 state.
 * Pretty awful looking since our assembler does not
 * recognize FP mnemonics.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		| save area pointer
	fsave	a0@			| save state
	tstb	a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem fp0-fp7,a0@(216)		| save FP general registers
	fmovem fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lm68881sdone:
	rts

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lm68881rdone:
	frestore a0@			| restore state
	rts
#endif

/*
 * Handle the nitty-gritty of rebooting the machine.
 *
 * DOES NOT YET WORK !!!!
 */
	.globl	_doboot
_doboot:
	movl	#CACHE_OFF,d0
	movl	_cpu040,d1		| load 68040 flag
	jeq	Ldoboot0
	.word	0xf4f8		| cpusha bc - push and invalidate caches
	nop
	movl	#CACHE40_OFF,d0
Ldoboot0:
	movc	d0,cacr			| disable on-chip cache(s)

	movw	#0x2700,sr		| cut off any interrupts

	| clear first 4k of CHIPMEM
	movl	_CHIPMEMADDR,a0
	movl	a0,a1
	movl	#1024,d0
Ldb1:
	clrl	a0@+
	dbra	d0,Ldb1

	| now, copy the following code over
|	lea	a1@(Ldoreboot),a0	| KVA starts at 0, CHIPMEM is phys 0
|	lea	a1@(Ldorebootend),a1
|	lea	pc@(Ldoreboot-.+2),a0
|	addl	a1,a0
|	lea	a0@(128),a1
|	lea	pc@(Ldoreboot-.+2),a2
	lea	Ldoreboot,a2
	lea	Ldorebootend,a0
	addl	a1,a0
	addl	a2,a1
	exg	a0,a1
Ldb2:
	movel	a2@+,a0@+
	cmpl	a1,a0
	jle	Ldb2

	| ok, turn off MMU..
Ldoreboot:
	tstl	d1
	jne	Lmmuoff040
	lea	zero,a0
	pmove	a0@,tc			| Turn off MMU
	lea	nullrp,a0
	pmove	a0@,crp			| Turn off MMU some more
	pmove	a0@,srp			| Really, really, turn off MMU
	jra	Ldoboot1
Lmmuoff040:
	movl	#0,d0
	.word	0x4e7b,0x0003	| movc d0,TC
	.word	0x4e7b,0x0806	| movc d0,URP
	.word	0x4e7b,0x0807	| movc d0,SRP
Ldoboot1:

	| this weird code is the OFFICIAL way to reboot an Amiga ! ..
	lea	0x1000000,a0
	subl	a0@(-0x14),a0
	movl	a0@(4),a0
	subl	#2,a0
	cmpw	#0x4e70,a0@		| 68040 kludge: if ROM entry is not
	jne	Ldoreset		| a reset, do the reset here
	jmp	a0@			| otherwise, jump to the ROM to reset
	| reset needs to be on longword boundary
	nop
	.align	2
Ldoreset:
	| reset unconfigures all memory!
	reset
	| now rely on prefetch for next jmp
	jmp	a0@
	| NOT REACHED


/*
 * Reboot directly into a new kernel image.
 * kernel_reload(image, image_size, entry,
 *		 fastram_start, fastram_size, chipram_start)
 */
	.globl	_kernel_reload
_kernel_reload:
	CUSTOMADDR(a5)

	movew	#(1<<9),a5@(0x096)	| disable DMA (before clobbering chipmem)

	movl	#CACHE_OFF,d0
	tstl	_cpu040
	jeq	Lreload1
	.word	0xf4f8		| cpusha bc - push and invalidate caches
	nop
	movl	#CACHE40_OFF,d0
Lreload1:
	movc	d0,cacr			| disable on-chip cache(s)

	movw	#0x2700,sr		| cut off any interrupts
	movel	_boothowto,d7		| save boothowto
	movel	_machineid,d5		| (and machineid)

	movel	sp@(16),a0		| load memory parameters
	movel	sp@(20),d0
	movel	sp@(24),d1

	movel	sp@(12),a4		| find entrypoint (a4)

	movel	sp@(4),a2		| copy kernel to low chip memory
	movel	sp@(8),d2
	movl	_CHIPMEMADDR,a3
Lreload_copy:
	movel	a2@+,a3@+
	subl	#4,d2
	jcc	Lreload_copy

	| ok, turn off MMU..
	tstl	_cpu040
	jne	Lreload040
	lea	zero,a3
	pmove	a3@,tc			| Turn off MMU
	lea	nullrp,a3
	pmove	a3@,crp			| Turn off MMU some more
	pmove	a3@,srp			| Really, really, turn off MMU
	jra	Lreload2
Lreload040:
	movl	#0,d3
	.word	0x4e7b,0x3003	| movc d3,TC
	.word	0x4e7b,0x3806	| movc d3,URP
	.word	0x4e7b,0x3807	| movc d3,SRP
Lreload2:

	jmp	a4@			| start new kernel


| A do-nothing MMU root pointer (includes the following long as well)

nullrp:	.long	0x7fff0001
zero:	.long	0
Ldorebootend:

	.data
	.space	NBPG
tmpstk:
	.globl	_machineid
_machineid:
	.long	0		| default to 320
	.globl	_cpu040
_cpu040:
	.long	0
	.globl	_mmutype,_protorp
_mmutype:
	.long	-1		| default to 68030 MMU
_protorp:
	.long	0x80000002,0	| prototype root pointer
	.globl	_cold
_cold:
	.long	1		| cold start flag
	.globl	_proc0paddr
_proc0paddr:
	.long	0		| KVA of proc0 u-area
#ifdef DEBUG
	.globl	fulltflush, fullcflush
fulltflush:
	.long	0
fullcflush:
	.long	0
	.globl	timebomb
timebomb:
	.long	0
#endif
/* interrupt counters */
	.globl	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	.asciz	"spur"
	.asciz	"tbe"
	.asciz	"kbd/scsi"
	.asciz	"vbl"
	.asciz	"lev4"
	.asciz	"rbf"
	.asciz	"dunno"
	.asciz	"clock"
#ifdef PROFTIMER
	.asciz  "pclock"
#endif
	.asciz	"nmi"
_eintrnames:
	.even
_intrcnt:
#ifdef PROFTIMER
	.long	0,0,0,0,0,0,0,0,0,0
#else
	.long	0,0,0,0,0,0,0,0,0
#endif
_eintrcnt:
