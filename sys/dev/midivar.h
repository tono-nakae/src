/*	$NetBSD: midivar.h,v 1.1 1998/08/07 00:00:58 augustss Exp $	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson
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

#define MIDI_BUFSIZE 4096

struct midi_buffer {
	u_char	*inp;
	u_char	*outp;
	u_char	*end;
	int	used;
	int	usedhigh;
	u_char	start[MIDI_BUFSIZE];
};

#define MIDI_MAX_WRITE 32	/* max bytes written with busy wait */
#define MIDI_WAIT 10000		/* microseconds to wait after busy wait */

struct midi_softc {
	struct	device dev;
	void	*hw_hdl;	/* Hardware driver handle */
	struct	midi_hw_if *hw_if; /* Hardware interface */
	struct	device *sc_dev;	/* Hardware device struct */
	int	isopen;		/* Open indicator */
	int	flags;		/* Open flags */
	struct	midi_buffer outbuf;
	struct	midi_buffer inbuf;
	int	props;
	int	rchan, wchan;
	int	pbus;
	struct	selinfo wsel;	/* write selector */
	struct	selinfo rsel;	/* read selector */
	struct	proc *async;	/* process who wants audio SIGIO */

	/* MIDI input state machine */
	int	in_state;
#define MIDI_IN_START 0
#define MIDI_IN_DATA 1
#define MIDI_IN_SYSEX 2
	u_char	in_msg[3];
	u_char	in_status;
	u_int	in_left;
	u_int	in_pos;

#if NSEQUENCER > 0
	/* Synthesizer emulation stuff */
	int	seqopen;
#endif
};

#define MIDIUNIT(d) ((d) & 0xff)

int midi_unit_count __P((void));
void midi_getinfo __P((dev_t, struct midi_info *));
