/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990 The Regents of the University of California.
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
 * from: Utah $Hdr: iteioctl.h 1.1 90/07/09$
 *
 *	@(#)iteioctl.h	7.2 (Berkeley) 11/4/90
 */

#define ITESWITCH	_IOW('Z',0x69, int)	/* XXX */
#define ITELOADKMAP	_IOW('Z',0x70, struct kbdmap)
#define ITEGETKMAP	_IOR('Z',0x71, struct kbdmap)

/* don't use these in new code, just included to get 
   view code to compile!! There's got to be a more generic,
   not so cc-concentrated approach! */

struct ite_window_size {
    int x;
    int y;
    int width;
    int height;
    int depth;
};

struct ite_bell_values {
    int volume;
    int period;
    int time;
};


#define ITE_GET_WINDOW_SIZE	_IOR('Z',0x72, struct ite_window_size)
#define ITE_SET_WINDOW_SIZE	_IOW('Z',0x73, struct ite_window_size)

#define ITE_DISPLAY_WINDOW	_IO('Z', 0x74)
#define ITE_REMOVE_WINDOW	_IO('Z', 0x75)

#define ITE_GET_BELL_VALUES	_IOR('Z', 0x76, struct ite_bell_values)
#define ITE_SET_BELL_VALUES	_IOW('Z', 0x77, struct ite_bell_values)


