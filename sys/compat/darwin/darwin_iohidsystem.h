/*	$NetBSD: darwin_iohidsystem.h,v 1.3 2003/04/29 22:16:39 manu Exp $ */

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Emmanuel Dreyfus
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

#ifndef	_DARWIN_IOHIDSYSTEM_H_
#define	_DARWIN_IOHIDSYSTEM_H_

extern struct mach_iokit_devclass darwin_iohidsystem_devclass;

typedef volatile int darwin_ev_lock_data_t; /* aka IOSharedLockData */

typedef struct {
	int16_t x;
	int16_t y;
} darwin_iogpoint;

typedef struct {
	int16_t width;
	int16_t height;
} darwin_iogsize;

typedef struct {
	int16_t minx;
	int16_t maxx;
	int16_t miny;
	int16_t maxy;
} darwin_iogbounds;

/* Events and event queue */

typedef struct {
	uint16_t tabletid;
	uint16_t pointerid;
	uint16_t deviceid;
	uint16_t system_tabletid;
	uint16_t vendor_pointertype;
	uint32_t pointer_serialnum;
	uint64_t uniqueid;
	uint32_t cap_mask;
	uint8_t ptrtype;
	uint8_t enter_proximity;
	int16_t reserved1;
} darwin_iohidsystem_tabletproxymity;

typedef struct {
	int32_t x;
	int32_t y;
	int32_t z;
	uint16_t buttons;
	uint16_t pressure;
	struct {
		int16_t x;
		int16_t y;
	} tilt;
	uint16_t rotation;
	uint16_t tanpressure;
	uint16_t devid;
	uint16_t vendor1;
	uint16_t vendor2;
	uint16_t vendor3;
} darwin_iohidsystem_tabletpoint;

typedef union {
	struct {
		uint8_t subx;
		uint8_t suby;
		int16_t buttonid;
		int32_t click;
		uint8_t pressure;
		uint8_t reserved1;
		uint8_t subtype;
		uint8_t reserved2;
		uint32_t reserved3;
		union {
			darwin_iohidsystem_tabletpoint point;	
			darwin_iohidsystem_tabletproxymity proximity;
		} tablet;
	} mouse;
	struct {
		int32_t dx;
		int32_t dy;
		uint8_t subx;
		uint8_t suby;
		uint8_t subtype;
		uint8_t reserved1;
		int32_t reserved2;
		union {
			darwin_iohidsystem_tabletpoint point;	
			darwin_iohidsystem_tabletproxymity proximity;
		} tablet;
	} mouse_move;
	struct {
		uint16_t origi_charset;
		int16_t repeat;
		uint16_t charset;
		uint16_t charcode;
		uint16_t keycode;
		uint16_t orig_charcode;
		int32_t reserved1;
		int32_t keyboardtype;
		int32_t reserved2[7];
	} key;
	struct {
		int16_t reserved;
		int16_t eventnum;
		int32_t trackingnum;
		int32_t userdata;
		int32_t reserved5[9];
	} tracking;
	struct {
		int16_t da1;
		int16_t da2;
		int16_t da3;
		int16_t reserved1;
		int32_t reserved2[10];
	} scrollwheel;
	struct {
		int16_t reserved;
		int16_t subtype;
		union {
			float	F[11];
			long	L[11];
			short	S[22];
			char	C[44];
		} misc;
	} compound;
	struct {
		int32_t x;
		int32_t y;
		int32_t z;
		uint16_t buttons;
		uint16_t pressure;
		struct {
			int16_t x;
			int16_t y;
		} tilt;
		uint16_t rotation;
		int16_t tanpressure;
		uint16_t devid;
		int16_t vendor1;
		int16_t vendor2;
		int16_t vendor3;
		int32_t reserved[4];
	} tablet;
	struct {
		uint16_t vendorid;
		uint16_t tabletid;
		uint16_t pointerid;
		uint16_t deviceid;
		uint16_t systemtabletid;
		uint16_t vendor_pointertype;
		uint32_t pointer_serialnum;
		uint64_t uniqueid;
		uint32_t cap_mask;
		uint8_t ptr_type;
		uint8_t enter_proximity;
		int16_t reserved[9];
	} proximity;
} darwin_iohidsystem_event_data;

typedef volatile struct {
	int die_type;
	int die_location_x;
	int die_location_y;
	unsigned long die_time_hi;
	unsigned long die_time_lo;
	int die_flags;
	unsigned int die_window;
	darwin_iohidsystem_event_data die_data;
} darwin_iohidsystem_event;

typedef struct {
	int diei_next;
	darwin_ev_lock_data_t diei_sem;
	/* 
	 * Avoid automatic alignement, this should be 
	 * darwin_iohidsystem_event, we declare it as a char array.
	 */
	unsigned char diei_event[76];
} darwin_iohidsystem_event_item;

#define DARWIN_IOHIDSYSTEM_EVENTQUEUE_LEN 240

struct darwin_iohidsystem_evglobals {
	darwin_ev_lock_data_t die_cursor_sem;
	int die_event_head;
	int die_event_tail;
	int die_event_last;
	int die_uniq_mouseid;
	int die_buttons;
	int die_event_flags;
	int die_event_time;
	darwin_iogpoint die_cursor_loc;
	int die_cursor_frame;
	darwin_iogbounds die_all_screen;
	darwin_iogbounds die_mouse_rect;
	int die_version;
	int die_struct_size;
	int die_last_frame;
	unsigned int die_reserved1[31];
	unsigned die_reserved2:27;
	unsigned die_want_pressure:1;
	unsigned die_want_precision:1;
	unsigned die_dontwant_coalesce:1;
	unsigned die_dont_coalesce:1;
	unsigned die_mouserect_valid:1;
	int die_moved_mask;
	int die_lastevent_sent;
	int die_lastevent_consumed;
	darwin_ev_lock_data_t die_waitcursor_sem;
	int die_waitcursor;
	char die_waitcursor_timeout;
	char die_waitcursor_enabled;
	char die_globalwaitcursor_enabled;
	int die_waitcursor_threshold;
	darwin_iohidsystem_event_item 
	    die_evqueue[DARWIN_IOHIDSYSTEM_EVENTQUEUE_LEN];
};

/* Shared memory between the IOHIDSystem driver and userland */
struct  darwin_iohidsystem_shmem {
	int dis_global_offset;	/* Offset to global zone, usually 8 */
	int dis_private_offset;	/* Offset to privete memory. Don't care. */
	struct darwin_iohidsystem_evglobals dis_evglobals; 
};

int darwin_iohidsystem_connect_method_scalari_scalaro(struct mach_trap_args *);
int darwin_iohidsystem_connect_map_memory(struct mach_trap_args *);

#endif /* _DARWIN_IOHIDSYSTEM_H_ */
