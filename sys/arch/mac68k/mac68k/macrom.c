/*	$NetBSD: macrom.c,v 1.21 1996/04/01 04:30:34 scottr Exp $	*/

/*-
 * Copyright (C) 1994	Bradley A. Grantham
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
 *	This product includes software developed by Bradley A. Grantham.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Mac ROM Glue
 *
 * This allows MacBSD to access (in a limited fashion) routines included
 * in the Mac ROMs, like ADBReInit.
 *
 * As a (fascinating) side effect, this glue allows ROM code (or any other
 * MacOS code) to call MacBSD kernel routines, like NewPtr.
 *
 * Uncleaned-up weirdness,
 *	This doesn't work on a lot of machines.  Perhaps the IIsi stuff
 * can be generalized somewhat for others.  It looks like most machines
 * are similar to the IIsi ("Universal ROMs"?).
 */

#include <sys/param.h>
#include <sys/queue.h>

#include <vm/vm_prot.h>
#include <vm/vm_param.h>
#include <vm/lock.h>
#include <vm/pmap.h>

#include <machine/viareg.h>
#include "macrom.h"
#include <sys/malloc.h>
#include <machine/cpu.h>

#include <machine/frame.h>

	/* trap modifiers (put it macrom.h) */
#define TRAP_TOOLBOX(a)	((a) & 0x800)
#define TRAP_PASSA0(a)	((a) & 0x100)
#define TRAP_NUM(a)	(TRAP_TOOLBOX(a) ? (a) & 0x3ff : (a) & 0xff)
#define TRAP_SYS(a)	((a) & 0x400)
#define TRAP_CLEAR(a)	((a) & 0x200)


	/* Mac Rom Glue global variables */
/*
 * ADB Storage.  Is 512 bytes enough?  Too much?
 */
u_char mrg_adbstore[512];
u_char mrg_adbstore2[512];
u_char mrg_adbstore3[512];
u_char mrg_ExpandMem[512];			/* 0x1ea Bytes minimum */
u_char mrg_adbstore4[32];			/* 0x16 bytes was the largest I found yet */
u_char mrg_adbstore5[80];			/* 0x46 bytes minimum */

caddr_t	mrg_romadbintr = (caddr_t)0x40807002;	/* ROM ADB interrupt */
caddr_t	mrg_rompmintr = 0;			/* ROM PM (?) interrupt */
char *mrg_romident = NULL;			/* ident string for ROMs */
caddr_t mrg_ADBAlternateInit = 0;
caddr_t mrg_InitEgret = 0;
caddr_t	mrg_ADBIntrPtr = (caddr_t)0x0;	/* ADB interrupt taken from MacOS vector table*/

caddr_t *rsrc_handle;				/* Variables for ROM resource map */
caddr_t *rsrc_header;
u_int32_t nr_of_rsrcs;


/*
 * Last straw functions; we didn't set them up, so freak out!
 * When someone sees these called, we can finally go back and
 * bother to implement them.
 */


int
mrg_Delay()
{
#define TICK_DURATION 16625

	int result = noErr;
	u_int32_t ticks;

	asm("	movl	a0, %0"		/* get arguments */
		:
		: "g" (ticks));

#if defined(MRG_DEBUG)
	printf("mrg: mrg_Delay(%d) = %d ms\n", ticks, ticks * 60);
#endif
	delay(ticks * TICK_DURATION);
	return(ticks);	/* The number of ticks since startup should be
			 * returned here. Until someone finds a need for
			 * this, we just return the requested number
			 *  of ticks */
}


int
is_resource(caddr_t rp)
{
	/*  Heuristic to locate resources in the ROM.
	 *  rp+8 builds a linked list, but where does it start?
	 */
	if (    (0x00000000 == ( *((u_int32_t *)  rp   ) & 0x00ffffff))	/* magic numbers */
	     && (0x00000000 ==   *((u_int32_t *) (rp+0x04)) )		/* magic numbers */
	     && ( 0 == ( *((u_int32_t *) (rp+0x08)) & 0x0f ) )		/* on paragraph boundary */
	     && ( 0 == ( *((u_int32_t *) (rp+0x0c)) & 0x0f ) )		/* on paragraph boundary */
	     && ( 0 == (  ((u_int32_t) rp) & 0x0f )      )		/* on paragraph boundary */
	     && (256 > ( *((u_int32_t *) (rp+0x0c)) - ((u_int32_t)rp & 0x0fffff)  ))
									/* point to someplace near rp */
	   )
	      return 1;
	 else return 0;
}

int
count_all_resources(caddr_t rombase, u_int32_t romlen)
{
	caddr_t romptr;
	u_int32_t nr_of_rsrcs = 0; 

	for (romptr = rombase + romlen; romptr >= rombase; romptr -= 0x10)
	{
	    if (is_resource(romptr))
	    nr_of_rsrcs++;  
	}
	return nr_of_rsrcs;
}

void
w_build_resource_list(caddr_t rombase, u_int32_t romlen)
{
	caddr_t romptr;
	u_int32_t rsrc_no = 0;
#ifdef MRG_DEBUG
	char rsrc_name[5];

	printf("mrg: Resources found:\n");
#endif
	nr_of_rsrcs = count_all_resources(rombase, romlen);
	if(0 == (rsrc_header = (caddr_t *) malloc(nr_of_rsrcs * sizeof(caddr_t), M_TEMP, M_NOWAIT)))
	    panic("mrg: Can't malloc memory for rsrc_header list\n");
	if(0 == (rsrc_handle = (caddr_t *) malloc(nr_of_rsrcs * sizeof(caddr_t), M_TEMP, M_NOWAIT)))
	    panic("mrg: Can't malloc memory for rsrc_handle list\n");

	for (romptr = rombase + romlen; romptr >= rombase; romptr -= 0x10)
	{
	    if (is_resource(romptr))
		if (rsrc_no < nr_of_rsrcs)
		{
		    rsrc_header[rsrc_no] = romptr;
		    rsrc_handle[rsrc_no] = (caddr_t) (ROMBase + *((u_int32_t *)(romptr + 0x0c)));
		    rsrc_no++;
#ifdef MRG_DEBUG
		    strncpy(rsrc_name, (char *) (romptr + 0x10), 4);
		    rsrc_name[4] = '\0';
		    printf("%4s 0x%2x   ", rsrc_name, *((u_int16_t *) (romptr + 0x14)) );
#endif
		}  
	}
#ifdef MRG_DEBUG  
	printf("\n");
#endif
}

int
w_count_resources(u_int32_t rsrc_type)
{
	u_int32_t rsrc_no = 0;
	u_int32_t rsrc_count = 0;
  
#ifdef MRG_DEBUG
	char rsrc_name[5];

	strncpy(rsrc_name, (char *) (&rsrc_type), 4);
	rsrc_name[4] = '\0';
	printf("mrg: w_count_resources called for resource %4s :  ", rsrc_name);
#endif
	while (rsrc_no < nr_of_rsrcs)
	{
	    if (rsrc_type == *((u_int32_t *) (rsrc_header[rsrc_no] + 0x10)) )
	    rsrc_count++;
	    rsrc_no++;
	}
#ifdef MRG_DEBUG
	printf("found %d resources of requested type!\n", rsrc_count);
#endif
	return rsrc_count;
}

caddr_t *
w_get_ind_resource(u_int32_t rsrc_type, u_int16_t rsrc_ind)
{
	u_int32_t rsrc_no = 0;

#ifdef MRG_DEBUG
	char rsrc_name[5];

	strncpy(rsrc_name, (char *) (&rsrc_type), 4);
	rsrc_name[4] = '\0';
	printf("mrg: w_get_int_resource called for resource %4s, no. %d :  ", rsrc_name, rsrc_ind);
#endif

	while (rsrc_ind > 0)
	{
	    while (    (rsrc_no < nr_of_rsrcs)
		    && (rsrc_type != *((u_int32_t *) (rsrc_header[rsrc_no] + 0x10)) )
		  )
	    {
		rsrc_no++;
	    }
	    rsrc_ind--;
	    rsrc_no++;
	}
	rsrc_no--;
	if (rsrc_no == nr_of_rsrcs)
	{ /* Error */
#ifdef MRG_DEBUG
	    printf("not found!\n");
#endif
	    return (caddr_t *) 0;
	}
	else
	{
#ifdef MRG_DEBUG
	    printf("found at addr 0x%x -> 0x%x\n", &rsrc_handle[rsrc_no], rsrc_handle[rsrc_no]);
#endif
	    return (caddr_t *) &rsrc_handle[rsrc_no];
	}
}


void
mrg_VBLQueue()
{
#define qLink 0
#define qType 4
#define vblAddr 6
#define vblCount 10
#define vblPhase 12

	caddr_t vbltask;
	caddr_t last_vbltask;
	
	last_vbltask = (caddr_t) &VBLQueue_head;
	vbltask = VBLQueue_head;
	while (0 != vbltask)
	{
	    if ( 0 != *((u_int16_t *)(vbltask + vblPhase)) )
	    {
		*((u_int16_t *)(vbltask + vblPhase)) -= 1;
	    } else
	    {
		if ( 0 != *((u_int16_t *)(vbltask + vblCount)) )
		{
		    *((u_int16_t *)(vbltask + vblCount)) -= 1;
		} else
		{
#if defined(MRG_DEBUG)
		    printf("mrg: mrg_VBLQueue: calling VBL task at 0x%x with VBLTask block at 0x%x\n",
			   *((u_int32_t *)(vbltask + vblAddr)), vbltask);
#endif	      
		    asm("   movml	#0xfffe, sp@-   /* better save all registers! */
			    movl	%0, a0
			    movl	%1, a1
			    jbsr	a1@
			    movml	sp@+, #0x7fff"	/* better restore all registers! */
			    :
			    : "g" (vbltask), "g" (*((caddr_t)(vbltask + vblAddr))));
#if defined(MRG_DEBUG)
		    printf("mrg: mrg_VBLQueue: back from VBL task\n");
#endif	      
		    if ( 0 == *((u_int16_t *)(vbltask + vblCount)) )
		    {
#if defined(MRG_DEBUG)
			printf("mrg: mrg_VBLQueue: removing VBLTask block at 0x%x\n",
			       vbltask);
#endif	      
			*((u_int32_t *)(last_vbltask + qLink)) = *((u_int32_t *)(vbltask + qLink));
			    /* can't free memory from VBLTask block as
		             * we don't know where it came from */
			if (vbltask == VBLQueue_tail)
			{ /* last task of do{}while */
			    VBLQueue_tail = last_vbltask;
			}
		    }
		}
	    }
	    last_vbltask = vbltask;
	    vbltask = (caddr_t) *((u_int32_t *)(vbltask + qLink));
	} /* while */
}


void
mrg_init_stub_1()
{
  	asm("movml #0xffff, sp@-");
	printf("mrg: hit mrg_init_stub_1\n");
  	asm("movml sp@+, #0xffff");
}

void
mrg_init_stub_2()
{
	panic("mrg: hit mrg_init_stub_2\n");
}

void
mrg_1sec_timer_tick()
{	
	/* The timer tick from the Egret chip triggers this routine via
	 * Lvl1DT[0] (addr 0x192) once every second.
	 */
}
  
void
mrg_lvl1dtpanic()		/* Lvl1DT stopper */
{
	printf("Agh!  I was called from Lvl1DT!!!\n");
#if DDB
	Debugger();
#endif
}

void
mrg_lvl2dtpanic()		/* Lvl2DT stopper */
{
	panic("Agh!  I was called from Lvl2DT!!!\n");
}

void
mrg_jadbprocpanic()	/* JADBProc stopper */
{
	panic("Agh!  Called JADBProc!\n");
}

void
mrg_jswapmmupanic()	/* jSwapMMU stopper */
{
	panic("Agh!  Called jSwapMMU!\n");
}

void
mrg_jkybdtaskpanic()	/* JKybdTask stopper */
{
	panic("Agh!  Called JKybdTask!\n");
}

long
mrg_adbintr()	/* Call ROM ADB Interrupt */
{
	if(mrg_romadbintr != NULL)
	{
#if defined(MRG_TRACE)
		tron();
#endif

		/* Gotta load a1 with VIA address. */
		/* ADB int expects it from Mac intr routine. */
		asm("
			movml	#0xffff, sp@-	| better save all registers!
			movl	%0, a0
			movl	_VIA, a1
			jbsr	a0@
			movml	sp@+, #0xffff"	/* better restore all registers! */
			:
			: "g" (mrg_romadbintr));

#if defined(MRG_TRACE)
		troff();
#endif

	}
	return(1);
}

long
mrg_pmintr()	/* Call ROM PM Interrupt */
{
	if(mrg_rompmintr != NULL)
	{
#if defined(MRG_TRACE)
		tron();
#endif

		/* Gotta load a1 with VIA address. */
		/* ADB int expects it from Mac intr routine. */
		asm("
			movml	#0xffff, sp@-	| better save all registers!
			movl	%0, a0
			movl	_VIA, a1
			jbsr	a0@
			movml	sp@+, #0xffff"	/* better restore all registers! */
			:
			: "g" (mrg_rompmintr));

#if defined(MRG_TRACE)
		troff();
#endif
	}
	return(1);
}


void
mrg_notrap()
{
	printf("Aigh!\n");
	panic("mrg_notrap: We're doomed!\n");
}

int
myowntrap()
{
	printf("Oooo!  My Own Trap Routine!\n");
	return(50);
}


int
mrg_NewPtr()
{
	int result = noErr;
	u_int numbytes;
	u_int32_t trapword;
	caddr_t ptr;

	asm("	movl	d1, %0
		movl	d0, %1"
		: "=g" (trapword), "=g" (numbytes));

#if defined(MRG_SHOWTRAPS)
	printf("mrg: NewPtr(%d bytes, %sclear, %ssys)", numbytes,
		TRAP_SYS(trapword) ? "" : "no ",
		TRAP_CLEAR(trapword) ? "" : "no ");
#endif

		/* plus 4 for size */
	ptr = malloc(numbytes + 4 , M_DEVBUF, M_NOWAIT); /* ?? */
		/* We ignore "Sys;" where else would it come from? */
		/* plus, (I think), malloc clears block for us */

	if(ptr == NULL){
		result = memFullErr;
#if defined(MRG_SHOWTRAPS)
		printf(" failed.\n");
#endif
	}else{
#if defined(MRG_SHOWTRAPS)
		printf(" succeded = %x.\n", ptr);
#endif
		*(u_int32_t *)ptr = numbytes;
		ptr += 4;
		bzero(ptr, numbytes); /* NewPtr, Clear ! */
	}

	asm("	movl	%0, a0" :  : "g" (ptr));
	return(result);
}

int
mrg_DisposPtr()
{
	int result = noErr;
	caddr_t ptr;

	asm("	movl	a0, %0" : "=g" (ptr));

#if defined(MRG_SHOWTRAPS)
	printf("mrg: DisposPtr(%x)\n", ptr);
#endif

	if(ptr == 0){
		result = memWZErr;
	}else{
		free(ptr - 4, M_DEVBUF);
	}

	return(result);
}

int
mrg_GetPtrSize()
{
	int result = noErr;
	caddr_t ptr;

	asm("	movl	a0, %0" : "=g" (ptr));

#if defined(MRG_SHOWTRAPS)
	printf("mrg: GetPtrSize(%x)\n", ptr);
#endif

	if(ptr == 0){
		return(memWZErr);
	}else
		return(*(int *)(ptr - 4));
}

int
mrg_SetPtrSize()
{
	int result = noErr;
	caddr_t ptr;
	int newbytes;

	asm("	movl	a1, %0
		movl	d0, %1"
		: "=g" (ptr), "=g" (newbytes));

#if defined(MRG_SHOWTRAPS)
	printf("mrg: SetPtrSize(%x, %d) failed\n", ptr, newbytes);
#endif

	return(memFullErr);	/* How would I handle this, anyway? */
}

int
mrg_PostEvent()
{
	return 0;
}

void
mrg_StripAddress()
{
}

/*
 * trap jump address tables (different per machine?)
 * Can I just use the tables stored in the ROMs?
 * *Is* there a table stored in the ROMs?
 * (BTW, this table is initialized for Mac II.)
 */
caddr_t mrg_OStraps[256] = {
#ifdef __GNUC__
		/* God, I love gcc.  see GCC2 manual, section 2.17, */
		/* "labeled elements in initializers." */
	[0x1e]	(caddr_t)mrg_NewPtr,
		(caddr_t)mrg_DisposPtr,
		(caddr_t)mrg_SetPtrSize,
		(caddr_t)mrg_GetPtrSize,
	[0x2f]	(caddr_t)mrg_PostEvent,
	[0x3b]	(caddr_t)mrg_Delay,	
	[0x47]	(caddr_t)mrg_SetOSTrapAddress,
	[0x55]	(caddr_t)mrg_StripAddress,
	[0x77]	(caddr_t)0x40807778,	/* CountADBs */
		(caddr_t)0x40807792,	/* GetIndADB */
		(caddr_t)0x408077be,	/* GetADBInfo */
		(caddr_t)0x408077c4,	/* SetADBInfo */
		(caddr_t)0x40807704,	/* ADBReInit */
		(caddr_t)0x408072fa,	/* ADBOp */
	[0x85]	0,			/* PMgrOp (not on II) */
	[0x92]	(caddr_t)0x40814800,	/* Egret */
#else
#error "Using a GNU C extension."
#endif
};

caddr_t mrg_ToolBoxtraps[1024] = {
	[0x19c] (caddr_t)mrg_CountResources,
	[0x19d] (caddr_t)mrg_GetIndResource,
	[0x1a0] (caddr_t)mrg_GetResource,
	[0x1af] (caddr_t)mrg_ResError,
};

int
mrg_SetOSTrapAddress()
{
	int result = noErr;
	u_int trapnumber;
	u_int trapaddress;
	u_int32_t trapword;

	asm("	movl	d1, %0
		movl	d0, %1
		movl	a0, %2"
		: "=g" (trapword), "=g" (trapnumber), "=g" (trapaddress));

#if defined(MRG_SHOWTRAPS)
	printf("mrg: SetOSTrapAddress(Trap: 0x%x, Address: 0x%8x)", trapnumber,
		trapaddress);
#endif

	mrg_OStraps[trapnumber] = (caddr_t) trapaddress;	

	return(result);
}

/*
 * Handle a supervisor mode A-line trap.
 */
void
mrg_aline_super(struct frame *frame)
{
	caddr_t trapaddr;
	u_short trapword;
	int isOStrap;
	int trapnum;
	int a0passback;
	u_int32_t a0bucket, d0bucket;
        int danprint=0; /* This shouldn't be necessary, but seems to be.  */

#if defined(MRG_DEBUG)
	printf("mrg: a super");
#endif

	trapword = *(u_short *)frame->f_pc;

        if (trapword == 0xa71e)
          danprint = 1;

#if defined(MRG_DEBUG)
	printf(" wd 0x%x", trapword);
#endif
	isOStrap = ! TRAP_TOOLBOX(trapword);
	trapnum = TRAP_NUM(trapword);

        if (danprint)
          {
	   /* Without these print statements, ADBReInit fails on IIsi */
            printf(""); printf("");
          }

#if defined(MRG_DEBUG)
	printf(" %s # 0x%x", isOStrap? "OS" :
		"ToolBox", trapnum);
#endif

	/* Only OS Traps come to us; _alinetrap takes care of ToolBox
	  traps, which are a horrible Frankenstein-esque abomination. */

	trapaddr = mrg_OStraps[trapnum];
#if defined(MRG_DEBUG)
	printf(" addr 0x%x\n", trapaddr);
 	printf("    got:    d0 = 0x%8x,  a0 = 0x%8x, called from: 0x%8x\n",
		frame->f_regs[0], frame->f_regs[8], frame->f_pc	);
#endif
	if(trapaddr == NULL){
		printf("unknown %s trap 0x%x, no trap address available\n",
			isOStrap ? "OS" : "ToolBox", trapword);
		panic("mrg_aline_super()");
	}
	a0passback = TRAP_PASSA0(trapword);

#if defined(MRG_TRACE)
	tron();
#endif

/* 	put trapword in d1 */
/* 	put trapaddr in a1 */
/* 	put a0 in a0 */
/* 	put d0 in d0 */
/* save a6 */
/* 	call the damn routine */
/* restore a6 */
/* 	store d0 in d0bucket */
/* 	store a0 in d0bucket */
/* This will change a1,d1,d0,a0 and possibly a6 */

	asm("
		movl	%2, a1
		movw	%3, d1
		movl	%4, d0
		movl	%5, a0
		jbsr	a1@
		movl	a0, %0
		movl	d0, %1"

		: "=g" (a0bucket), "=g" (d0bucket)

		: "g" (trapaddr), "g" (trapword),
			"m" (frame->f_regs[0]), "m" (frame->f_regs[8])

		: "d0", "d1", "a0", "a1", "a6"

	);

#if defined(MRG_TRACE)
	troff();
#endif
#if defined(MRG_DEBUG)
	printf("    result: d0 = 0x%8x,  a0 = 0x%8x\n",
		d0bucket, a0bucket );
 	printf(" bk");
#endif

	frame->f_regs[0] = d0bucket;
	if(a0passback)
		frame->f_regs[8] = a0bucket;

	frame->f_pc += 2;	/* skip offending instruction */

#if defined(MRG_DEBUG)
	printf(" exit\n");
#endif
}

	/* handle a user mode A-line trap */
void
mrg_aline_user()
{
#if 1
	/* send process a SIGILL; aline traps are illegal as yet */
#else /* how to handle real Mac App A-lines */
	/* ignore for now */
	I have no idea!
	maybe pass SIGALINE?
	maybe put global information about aline trap?
#endif
}

extern u_int32_t traceloopstart[];
extern u_int32_t traceloopend;
extern u_int32_t *traceloopptr;

void
dumptrace()
{
#if defined(MRG_TRACE)
	u_int32_t *traceindex;

	printf("instruction trace:\n");
	traceindex = traceloopptr + 1;
	while(traceindex != traceloopptr)
	{
		printf("    %08x\n", *traceindex++);
		if(traceindex == &traceloopend)
			traceindex = &traceloopstart[0];
	}
#else
	printf("mrg: no trace functionality enabled\n");
#endif
}

	/* Set ROM Vectors */
void
mrg_setvectors(rom)
	romvec_t *rom;
{
	if (rom == NULL)
		return;		/* whoops!  ROM vectors not defined! */

	mrg_romident = rom->romident;

	if (0 != mrg_ADBIntrPtr) {
		mrg_romadbintr = mrg_ADBIntrPtr;
		printf("mrg_setvectors: using ADBIntrPtr from booter: 0x%08x\n",
			mrg_ADBIntrPtr);
	} else
 		mrg_romadbintr = rom->adbintr;
	mrg_rompmintr = rom->pmintr;
	mrg_ADBAlternateInit = rom->ADBAlternateInit;
	mrg_InitEgret = rom->InitEgret;

	/*
	 * mrg_adbstore becomes ADBBase
	 */
	*((u_int32_t *)(mrg_adbstore + 0x130)) = (u_int32_t) rom->adb130intr;

	jEgret = (void (*))0x40814800;

	mrg_OStraps[0x77] = rom->CountADBs;
	mrg_OStraps[0x78] = rom->GetIndADB;
	mrg_OStraps[0x79] = rom->GetADBInfo;
	mrg_OStraps[0x7a] = rom->SetADBInfo;
	mrg_OStraps[0x7b] = rom->ADBReInit;
	mrg_OStraps[0x7c] = rom->ADBOp;
	mrg_OStraps[0x85] = rom->PMgrOp;
	mrg_OStraps[0x51] = rom->ReadXPRam;

	mrg_OStraps[0x38] = rom->WriteParam;	/* WriteParam */
	mrg_OStraps[0x3a] = rom->SetDateTime;	/* SetDateTime */
	mrg_OStraps[0x3f] = rom->InitUtil;	/* InitUtil */
	mrg_OStraps[0x51] = rom->ReadXPRam;	/* ReadXPRam */
	mrg_OStraps[0x52] = rom->WriteXPRam;	/* WriteXPRam */
        jClkNoMem = (void (*)) rom->jClkNoMem;

	if (0 == jClkNoMem) {
		printf("WARNING: don't have a value for jClkNoMem, please contact:  walter@ghpc8.ihf.rwth-aachen.de\n");
		printf("Can't read RTC without it. Using MacOS boot time.\n");
	}

	mrg_ToolBoxtraps[0x04d] = rom->FixDiv;
	mrg_ToolBoxtraps[0x068] = rom->FixMul;


#if defined(MRG_DEBUG)
	printf("mrg: ROM adbintr 0x%08x\n", mrg_romadbintr);
	printf("mrg: ROM pmintr 0x%08x\n", mrg_rompmintr);
	printf("mrg: OS trap 0x77 (CountADBs) = 0x%08x\n", mrg_OStraps[0x77]);
	printf("mrg: OS trap 0x78 (GetIndADB) = 0x%08x\n", mrg_OStraps[0x78]);
	printf("mrg: OS trap 0x79 (GetADBInfo) = 0x%08x\n", mrg_OStraps[0x79]);
	printf("mrg: OS trap 0x7a (SetADBInfo) = 0x%08x\n", mrg_OStraps[0x7a]);
	printf("mrg: OS trap 0x7b (ADBReInit) = 0x%08x\n", mrg_OStraps[0x7b]);
	printf("mrg: OS trap 0x7c (ADBOp) = 0x%08x\n", mrg_OStraps[0x7c]);
	printf("mrg: OS trap 0x85 (PMgrOp) = 0x%08x\n", mrg_OStraps[0x85]);
	printf("mrg: ROM ADBAltInit 0x%08x\n", mrg_ADBAlternateInit);
	printf("mrg: ROM InitEgret  0x%08x\n", mrg_InitEgret);
#endif
}

	/* To find out if we're okay calling ROM vectors */
int
mrg_romready()
{
	return(mrg_romident != NULL);
}

extern unsigned long	IOBase;
extern volatile u_char	*sccA;

	/* initialize Mac ROM Glue */
void
mrg_init()
{
	int i;
	char *findername = "MacBSD FakeFinder";
	caddr_t ptr;
	caddr_t *handle;
	int sizeptr;
	extern short mrg_ResErr;
	
	w_build_resource_list(ROMBase, 0x00100000);	/* search one MB */
	
	VBLQueue = (u_int16_t) 0;	/* No vertical blanking routines in the queue */
	VBLQueue_head = (caddr_t) 0;	/*  Let's hope that this init happens
	VBLQueue_tail = (caddr_t) 0;	 *  before the RTC interrupts are enabled */
					 
	if(mrg_romready()){
		printf("mrg: '%s' ROM glue", mrg_romident);

#if defined(MRG_TRACE)
#if defined(MRG_FOLLOW)
		printf(", tracing on (verbose)");
#else /* ! defined (MRG_FOLLOW) */
		printf(", tracing on (silent)");
#endif /* defined(MRG_FOLLOW) */
#else /* !defined(MRG_TRACE) */
		printf(", tracing off");
#endif	/* defined(MRG_TRACE) */

#if defined(MRG_DEBUG)
		printf(", debug on");
#else /* !defined(MRG_DEBUG) */
		printf(", debug off");
#endif /* defined(MRG_DEBUG) */

#if defined(MRG_SHOWTRAPS)
		printf(", verbose traps");
#else /* !defined(MRG_SHOWTRAPS) */
		printf(", silent traps");
#endif /* defined(MRG_SHOWTRAPS) */
	}else{
		printf("mrg: kernel has no ROM vectors for this machine!\n");
		return;
	}

	printf("\n");

#if defined(MRG_DEBUG)
	printf("mrg: start init\n");
#endif
		/* expected globals */
	ExpandMem = &mrg_ExpandMem[0];
	*((u_int16_t *)(mrg_ExpandMem + 0x00) ) = 0x0123;	/* magic (word) */
	*((u_int32_t *)(mrg_ExpandMem + 0x02) ) = 0x000001ea;	/* Length of table (long) */
	*((u_int32_t *)(mrg_ExpandMem + 0x1e0)) = (u_int32_t) &mrg_adbstore4[0];

	*((u_int32_t *)(mrg_adbstore4 + 0x8)) = (u_int32_t) mrg_init_stub_1;
	*((u_int32_t *)(mrg_adbstore4 + 0xc)) = (u_int32_t) mrg_init_stub_2;
	*((u_int32_t *)(mrg_adbstore4 + 0x4)) = (u_int32_t) &mrg_adbstore5[0];

	*((u_int32_t *)(mrg_adbstore5 + 0x08)) = (u_int32_t) 0x00100000;
	*((u_int32_t *)(mrg_adbstore5 + 0x0c)) = (u_int32_t) 0x00100000;
	*((u_int32_t *)(mrg_adbstore5 + 0x16)) = (u_int32_t) 0x00480000;

	ADBBase = &mrg_adbstore[0];
	ADBState = &mrg_adbstore2[0];
	ADBYMM = &mrg_adbstore3[0];
	MinusOne = 0xffffffff;
	Lo3Bytes = 0x00ffffff;
	VIA = (caddr_t)Via1Base;
	MMU32Bit = 1; /* ?means MMU is in 32 bit mode? */
  	if(TimeDBRA == 0)
		TimeDBRA = 0xa3b;		/* BARF default is Mac II */
  	if(ROMBase == 0)
		panic("ROMBase not set in mrg_init()!\n");

	strcpy(&FinderName[1], findername);
	FinderName[0] = (u_char) strlen(findername);
#if defined(MRG_DEBUG)
	printf("After setting globals\n");
#endif

		/* Fake jump points */
	for(i = 0; i < 8; i++) /* Set up fake Lvl1DT */
		Lvl1DT[i] = mrg_lvl1dtpanic;
	for(i = 0; i < 8; i++) /* Set up fake Lvl2DT */
		Lvl2DT[i] = mrg_lvl2dtpanic;
	Lvl1DT[0] = (void (*)())mrg_1sec_timer_tick;
	Lvl1DT[2] = (void (*)())mrg_romadbintr;
	Lvl1DT[4] = (void (*)())mrg_rompmintr;
	JADBProc = mrg_jadbprocpanic; /* Fake JADBProc for the time being */
	jSwapMMU = mrg_jswapmmupanic; /* Fake jSwapMMU for the time being */
	JKybdTask = mrg_jkybdtaskpanic; /* Fake jSwapMMU for the time being */

	jADBOp = (void (*)())mrg_OStraps[0x7c];	/* probably very dangerous */
	mrg_VIA2 = (caddr_t)(Via1Base + VIA2 * 0x2000);	/* see via.h */
	SCCRd = (caddr_t)(IOBase + sccA);   /* ser.c ; we run before serinit */

	switch(mach_cputype()){
		case MACH_68020:	CPUFlag = 2;	break;
		case MACH_68030:	CPUFlag = 3;	break;
		case MACH_68040:	CPUFlag = 4;	break;
		default:
			printf("mrg: unknown CPU type; cannot set CPUFlag\n");
			break;
	}

#if defined(MRG_TEST)
	printf("Allocating a pointer...\n");
	ptr = (caddr_t)NewPtr(1024);
	printf("Result is 0x%x.\n", ptr);
	sizeptr = GetPtrSize((Ptr)ptr);
	printf("Pointer size is %d\n", sizeptr);
	printf("Freeing the pointer...\n");
	DisposPtr((Ptr)ptr);
	printf("Free'd.\n");

	for(i = 0; i < 500000; i++)
		if((i % 100000) == 0)printf(".");
	printf("\n");

	mrg_ResErr = 0xdead;	/* set an error we know */
	printf("Getting error code...\n");
	i = ResError();
	printf("Result code (0xdeadbaaf): %x\n", i);
	printf("Getting a Resource...\n");
	handle = GetResource('ADBS', 2);
	printf("Handle result from GetResource: 0x%x\n", handle);
	printf("Getting error code...\n");
	i = ResError();
	printf("Result code (-192?) : %d\n", i);

	for(i = 0; i < 500000; i++)
		if((i % 100000) == 0)printf(".");
	printf("\n");

#if defined(MRG_TRACE)
	printf("Turning on a trace\n");
	tron();
	printf("We are now tracing\n");
	troff();
	printf("Turning off trace\n");
	dumptrace();
#endif /* MRG_TRACE */

	for(i = 0; i < 500000; i++)
		if((i % 100000) == 0)printf(".");
	printf("\n");
#endif /* MRG_TEST */

#if defined(MRG_DEBUG)
	printf("after setting jump points\n");
	printf("mrg: end init\n");
#endif

	if (current_mac_model->class == MACH_CLASSII) {
		/*
		 * For the bloody Mac II ROMs, we have to map this space
		 * so that the PRam functions will work.
		 * Gee, Apple, is that a hard-coded hardware address in
		 * your code?  I think so! (_ReadXPRam + 0x0062)  We map
		 * the first 
		 */
#ifdef DIAGNOSTIC
		printf("mrg: I/O map kludge for old ROMs that use hardware %s",
			"addresses directly.\n");
#endif
		pmap_map(0x50f00000, 0x50f00000, 0x50f00000 + 0x4000,
			 VM_PROT_READ|VM_PROT_WRITE);
	}
}

void
setup_egret(void)
{
	if (0 != mrg_InitEgret){

	/* This initializes ADBState (mrg_ADBStore2) and
	   enables interrupts */
		asm("	movml	a0-a2, sp@-
			movl	%1, a0		/* ADBState, mrg_adbstore2 */
			movl	%0, a1
			jbsr	a1@
			movml	sp@+, a0-a2 "
			:
			: "g" (mrg_InitEgret), "g" (ADBState));
		jEgret = (void (*)) mrg_OStraps[0x92]; /* may have been set in asm() */
	}
	else printf("Help ...  No vector for InitEgret!!\n");
	
	printf("mrg: ADBIntrVector: 0x%8x,  mrg_ADBIntrVector: 0x%8x\n",
			(long) mrg_romadbintr,
			*((long *) 0x19a));
	printf("mrg: EgretOSTrap: 0x%8x\n",
			(long) mrg_OStraps[0x92]);

}

void
mrg_initadbintr()
{
	int i;

	if (mac68k_machine.do_graybars)
		printf("Got following HwCfgFlags: 0x%4x, 0x%8x, 0x%8x, 0x%8x\n",
				HwCfgFlags, HwCfgFlags2, HwCfgFlags3, ADBReInit_JTBL);

        if ( (HwCfgFlags == 0) && (HwCfgFlags2 == 0) && (HwCfgFlags3 == 0) ){

		printf("Caution: No HwCfgFlags from Booter, please "
			"use at least booter version 1.8.\n");

		if (current_mac_model->class == MACH_CLASSIIsi) {
			printf("     ...  Using defaults for IIsi.\n");

			/* Egret and ADBReInit look into these HwCfgFlags */
			HwCfgFlags = 0xfc00;	
			HwCfgFlags2 = 0x0000773F;
			HwCfgFlags3 = 0x000001a6;
		}

		printf("Using HwCfgFlags: 0x%4x, 0x%8x, 0x%8x\n",
			HwCfgFlags, HwCfgFlags2, HwCfgFlags3);
	}

	/*
	 * If we think there is an Egret in the machine, attempt to
	 * set it up.  If not, just enable the interrupts (only on
	 * some machines, others are already on from ADBReInit?).
	 */
	if (   ((HwCfgFlags3 & 0x0e) == 0x06 )
	    || ((HwCfgFlags3 & 0x70) == 0x20 )) {
		if (mac68k_machine.do_graybars)
			printf("mrg: setup_egret:\n");

		setup_egret();

		if (mac68k_machine.do_graybars)
			printf("mrg: setup_egret: done.\n");

	} else {

		if (mac68k_machine.do_graybars)
			printf("mrg: Not setting up egret.\n");

		via_reg(VIA1, vIFR) = 0x4;
		via_reg(VIA1, vIER) = 0x84;

		if (mac68k_machine.do_graybars)
			printf("mrg: ADB interrupts enabled.\n");
	}	
}

#define IS_ROM_ADDR(addr) (   ((u_int) (addr)) > oldbase         \
			   && ((u_int) (addr)) < oldbase + ROMLEN)

void
mrg_fixupROMBase(obase, nbase)
	caddr_t obase;
	caddr_t nbase;
{
	int       i;
	u_int32_t temp, *p, oldbase, newbase;

	oldbase = (u_int32_t) obase;
	newbase = (u_int32_t) nbase;
	for (i=0 ; i<256 ; i++)
		if (IS_ROM_ADDR(mrg_OStraps[i])) {
			temp = (u_int) mrg_OStraps[i];
			temp = (temp - oldbase) + newbase;
			mrg_OStraps[i] = (caddr_t) temp;
		}
	for (i=0 ; i<1024 ; i++)
		if (IS_ROM_ADDR(mrg_ToolBoxtraps[i])) {
			temp = (u_int) mrg_ToolBoxtraps[i];
			temp = (temp - oldbase) + newbase;
			mrg_ToolBoxtraps[i] = (caddr_t) temp;
		}
	p = (u_int32_t *) mrg_adbstore;
	for (i=0 ; i<512/4 ; i++)
		if (IS_ROM_ADDR(p[i]))
			p[i] = (p[i] - oldbase) + newbase;

	if (IS_ROM_ADDR(jEgret))
		jEgret = (void (*)) ((((u_int) jEgret) - oldbase) + newbase);

	if (IS_ROM_ADDR(mrg_romadbintr))
		mrg_romadbintr = mrg_romadbintr - oldbase + newbase;

	if (IS_ROM_ADDR(mrg_rompmintr))
		mrg_rompmintr = mrg_rompmintr - oldbase + newbase;

	if (IS_ROM_ADDR(mrg_romident))
		mrg_romident = mrg_romident - oldbase + newbase;

	if (IS_ROM_ADDR(jClkNoMem))
		jClkNoMem = jClkNoMem - oldbase + newbase;

	if (IS_ROM_ADDR(mrg_ADBAlternateInit))
		mrg_ADBAlternateInit = mrg_ADBAlternateInit - oldbase + newbase;

	if (IS_ROM_ADDR(mrg_InitEgret))
		mrg_InitEgret = mrg_InitEgret - oldbase + newbase;

	if (IS_ROM_ADDR(ADBReInit_JTBL))
		ADBReInit_JTBL = ADBReInit_JTBL - oldbase + newbase;
}

void
ADBAlternateInit(void)
{
	if (0 == mrg_ADBAlternateInit){
		ADBReInit();
	} else {
 		asm("
			movml	a0-a6/d0-d7, sp@-
			movl	%0, a1
			movl	%1, a3
			jbsr	a1@
			movml	sp@+, a0-a6/d0-d7"
	
			: 
			: "g" (mrg_ADBAlternateInit), "g" (ADBBase) );
	}
}
