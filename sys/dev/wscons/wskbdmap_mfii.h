/*	$NetBSD: wskbdmap_mfii.h,v 1.2 1998/04/18 09:51:20 drochner Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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

#define KC(n)		(0xe000 | (n))  /* see wsksymvar.h */

static const keysym_t pckbd_keydesc_us[] = {
/*  pos      command		normal		shifted */
    KC(1),   KS_Cmd_Debugger,	KS_Escape,
    KC(2),  			KS_1,		KS_exclam,
    KC(3),  			KS_2,		KS_at,
    KC(4),  			KS_3,		KS_numbersign,
    KC(5),  			KS_4,		KS_dollar,
    KC(6),  			KS_5,		KS_percent,
    KC(7),  			KS_6,		KS_asciicircum,
    KC(8),  			KS_7,		KS_ampersand,
    KC(9),  			KS_8,		KS_asterisk,
    KC(10), 			KS_9,		KS_parenleft,
    KC(11), 			KS_0,		KS_parenright,
    KC(12), 			KS_minus,	KS_underscore,
    KC(13), 			KS_equal,	KS_plus,
    KC(14), 			KS_BackSpace,
    KC(15), 			KS_Tab,
    KC(16), 			KS_q,
    KC(17), 			KS_w,
    KC(18), 			KS_e,
    KC(19), 			KS_r,
    KC(20), 			KS_t,
    KC(21), 			KS_y,
    KC(22), 			KS_u,
    KC(23), 			KS_i,
    KC(24), 			KS_o,
    KC(25), 			KS_p,
    KC(26), 			KS_bracketleft,	KS_braceleft,
    KC(27), 			KS_bracketright, KS_braceright,
    KC(28), 			KS_Return,
    KC(29),  KS_Cmd1,		KS_Control_L,
    KC(30), 			KS_a,
    KC(31), 			KS_s,
    KC(32), 			KS_d,
    KC(33), 			KS_f,
    KC(34), 			KS_g,
    KC(35), 			KS_h,
    KC(36), 			KS_j,
    KC(37), 			KS_k,
    KC(38), 			KS_l,
    KC(39), 			KS_semicolon,	KS_colon,
    KC(40), 			KS_apostrophe,	KS_quotedbl,
    KC(41), 			KS_grave,	KS_asciitilde,
    KC(42), 			KS_Shift_L,
    KC(43), 			KS_backslash,	KS_bar,
    KC(44), 			KS_z,
    KC(45), 			KS_x,
    KC(46), 			KS_c,
    KC(47), 			KS_v,
    KC(48), 			KS_b,
    KC(49), 			KS_n,
    KC(50), 			KS_m,
    KC(51), 			KS_comma,	KS_less,
    KC(52), 			KS_period,	KS_greater,
    KC(53), 			KS_slash,	KS_question,
    KC(54), 			KS_Shift_R,
    KC(55), 			KS_KP_Multiply,
    KC(56),  KS_Cmd2,		KS_Alt_L,
    KC(57), 			KS_space,
    KC(58), 			KS_Caps_Lock,
    KC(59),  KS_Cmd_Screen0,	KS_f1,
    KC(60),  KS_Cmd_Screen1,	KS_f2,
    KC(61),  KS_Cmd_Screen2,	KS_f3,
    KC(62),  KS_Cmd_Screen3,	KS_f4,
    KC(63),  KS_Cmd_Screen4,	KS_f5,
    KC(64),  KS_Cmd_Screen5,	KS_f6,
    KC(65),  KS_Cmd_Screen6,	KS_f7,
    KC(66),  KS_Cmd_Screen7,	KS_f8,
    KC(67),  KS_Cmd_Screen8,	KS_f9,
    KC(68),  KS_Cmd_Screen9,	KS_f10,
    KC(69), 			KS_Num_Lock,
    KC(70), 			KS_Hold_Screen,
    KC(71), 			KS_KP_Home,	KS_KP_7,
    KC(72), 			KS_KP_Up,	KS_KP_8,
    KC(73), 			KS_KP_Prior,	KS_KP_9,
    KC(74), 			KS_KP_Subtract,
    KC(75), 			KS_KP_Left,	KS_KP_4,
    KC(76), 			KS_KP_Begin,	KS_KP_5,
    KC(77), 			KS_KP_Right,	KS_KP_6,
    KC(78), 			KS_KP_Add,
    KC(79), 			KS_KP_End,	KS_KP_1,
    KC(80), 			KS_KP_Down,	KS_KP_2,
    KC(81), 			KS_KP_Next,	KS_KP_3,
    KC(82), 			KS_KP_Insert,	KS_KP_0,
    KC(83), 			KS_KP_Delete,	KS_KP_Decimal,
    KC(87), 			KS_f11,
    KC(88), 			KS_f12,
    /*  127, break, */
    KC(156),			KS_KP_Enter,
    KC(157),			KS_Control_R,
    /*  170, print screen, */
    KC(181),			KS_KP_Divide,
    /*  183, print screen, */
    KC(184),			KS_Alt_R,	KS_Multi_key,
    KC(199),			KS_Home,
    KC(200),			KS_Up,
    KC(201),			KS_Prior,
    KC(203),			KS_Left,
    KC(205),			KS_Right,
    KC(207),			KS_End,
    KC(208),			KS_Down,
    KC(209),			KS_Next,
    KC(210),			KS_Insert,
    KC(211),			KS_Delete,
    /*  219, left win, */
    /*  220, right win, */
    /*  221, menu, */
};

static const keysym_t pckbd_keydesc_de[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(3),   KS_2,		KS_quotedbl,	KS_twosuperior,
    KC(4),   KS_3,		KS_section,	KS_threesuperior,
    KC(7),   KS_6,		KS_ampersand,
    KC(8),   KS_7,		KS_slash,	KS_braceleft,
    KC(9),   KS_8,		KS_parenleft,	KS_bracketleft,
    KC(10),  KS_9,		KS_parenright,	KS_bracketright,
    KC(11),  KS_0,		KS_equal,	KS_braceright,
    KC(12),  KS_ssharp,		KS_question,	KS_backslash,
    KC(13),  KS_dead_acute,	KS_dead_grave,
    KC(16),  KS_q,		KS_Q,		KS_at,
    KC(21),  KS_z,
    KC(26),  KS_udiaeresis,
    KC(27),  KS_plus,		KS_asterisk,	KS_dead_tilde,
    KC(39),  KS_odiaeresis,
    KC(40),  KS_adiaeresis,
    KC(41),  KS_dead_circumflex,KS_dead_abovering,
    KC(43),  KS_numbersign,	KS_apostrophe,
    KC(44),  KS_y,
    KC(50),  KS_m,		KS_M,		KS_mu,
    KC(51),  KS_comma,		KS_semicolon,
    KC(52),  KS_period,		KS_colon,
    KC(53),  KS_minus,		KS_underscore,
    KC(86),  KS_less,		KS_greater,	KS_bar,		KS_brokenbar,
    KC(184), KS_Mode_switch,	KS_Multi_key,
};

static const keysym_t pckbd_keydesc_de_nodead[] = {
/*  pos      normal		shifted		altgr		shift-altgr */
    KC(13),  KS_apostrophe,	KS_grave,
    KC(27),  KS_plus,		KS_asterisk,	KS_asciitilde,
    KC(41),  KS_asciicircum,	KS_degree,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

static const struct wscons_keydesc pckbd_keydesctab[] = {
	KBD_MAP(KB_US,			0,	pckbd_keydesc_us),
	KBD_MAP(KB_DE,			KB_US,	pckbd_keydesc_de),
	KBD_MAP(KB_DE | KB_NODEAD,	KB_DE,	pckbd_keydesc_de_nodead),
};

#undef KBD_MAP
#undef KC
