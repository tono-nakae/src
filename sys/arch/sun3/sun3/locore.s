#include "assym.s"
#include "../include/asm.h"
#include "syscall.h"
					| remember this is a fun project :)


.text
.globl start; .globl _start

start: _start:
/*
 * First we need to set it up so we can access the sun MMU, and be otherwise
 * undisturbed.  Until otherwise noted, all code must be position independent
 * as the boot loader put us low in memory, but we are linked high.
 */
	movw #PSL_HIGHIPL, sr		| no interrupts
	moveq #FC_CONTROL, d0		| make movs get us to the control
	movc d0, dfc			| space where the sun3 designers
	movc d0, sfc			| put all the "useful" stuff
	moveq #CONTEXT_0, d0
	movsb d0, CONTEXT_REG		| now in context 0

/*
 * In order to "move" the kernel to high memory, we are going to copy
 * the first 8 Mb of pmegs such that we will be mapped at the linked address.
 * This is all done by playing with the segment map, and then propigating it
 * to the other contexts.
 * We will unscramble which pmegs we actually need later.
 *
 */

percontext:					|loop among the contexts
	clrl d1					
	movl #(SEGMAP_BASE+KERNBASE), a0	| base index into seg map

perpmeg:

	movsb d1, a0@				| establish mapping
	addql #1, d1			
	addl #NBSG, a0
	cmpl #(MAINMEM_MONMAP / NBSG), d1	| are we done
	bne perpmeg


	addql #1, d0				| next context ....
	cmpl #CONTEXT_NUM, d0
	bne percontext

	clrl d0					
	movsb d0, CONTEXT_REG			| back to context 0
	
	jmp mapped_right:l			| things are now mapped right:)



mapped_right:

	movl #_edata, a0
	movl #_end, a1
bsszero: clrl a0@
	addql #4, a0
	cmpl a0, a1
	bne bsszero
	movl #start, sp
	jsr _sun3_bootstrap
	jsr _main
	rts					|should never get here
.text
/*
 * Icode is copied out to process 1 to exec init.
 * If the exec fails, process 1 exits.
 */
	.globl	_icode,_szicode
	.text
_icode:
	clrl	sp@-
	pea	pc@((argv-.)+2)
	pea	pc@((init-.)+2)
	clrl	sp@-
	moveq	#SYS_execve,d0
	trap	#0
	moveq	#SYS_exit,d0
	trap	#0
init:
	.asciz	"/sbin/init"
	.even
argv:
	.long	init+6-_icode		| argv[0] = "init" ("/sbin/init" + 6)
	.long	eicode-_icode		| argv[1] follows icode after copyout
	.long	0
eicode:

_szicode:
	.long	_szicode-_icode

#include "lib.s"
#include "copy.s"
#include "m68k.s"
#include "signal.s"
#include "process.s"
#include "softint.s"

