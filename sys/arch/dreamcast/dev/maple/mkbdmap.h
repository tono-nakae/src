/*	$NetBSD: mkbdmap.h,v 1.1 2001/01/16 00:33:01 marcus Exp $	*/

/*
 * Copyright (c) 2001 Marcus Comstedt
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

#define KC(n) KS_KEYCODE(n)

static const keysym_t mkbd_keydesc_jp[] = {
/*  pos		normal		shifted */
    KC(4),	KS_a,
    KC(5),	KS_b,
    KC(6),	KS_c,
    KC(7),	KS_d,
    KC(8),	KS_e,
    KC(9),	KS_f,
    KC(10),	KS_g,
    KC(11),	KS_h,
    KC(12),	KS_i,
    KC(13),	KS_j,
    KC(14),	KS_k,
    KC(15),	KS_l,
    KC(16),	KS_m,
    KC(17),	KS_n,
    KC(18),	KS_o,
    KC(19),	KS_p,
    KC(20),	KS_q,
    KC(21),	KS_r,
    KC(22),	KS_s,
    KC(23),	KS_t,
    KC(24),	KS_u,
    KC(25),	KS_v,
    KC(26),	KS_w,
    KC(27),	KS_x,
    KC(28),	KS_y,
    KC(29),	KS_z,

    KC(30),	KS_1,		KS_exclam,
    KC(31),	KS_2,		KS_quotedbl,
    KC(32),	KS_3,		KS_numbersign,
    KC(33),	KS_4,		KS_dollar,
    KC(34),	KS_5,		KS_percent,
    KC(35),	KS_6,		KS_ampersand,
    KC(36),	KS_7,		KS_apostrophe,
    KC(37),	KS_8,		KS_parenleft,
    KC(38),	KS_9,		KS_parenright,
    KC(39),	KS_0,		KS_asciitilde,

    KC(40),	KS_Return,
    KC(41),	KS_Escape,
    KC(42),	KS_BackSpace,
    KC(43),	KS_Tab,
    KC(44),	KS_space,

    KC(45),	KS_minus,	KS_equal,
    KC(46),	KS_asciicircum,	KS_macron,
    KC(47),	KS_at,		KS_grave,
    KC(48),	KS_bracketleft,	KS_braceleft,
    /* no 49 */
    KC(50),	KS_bracketright,KS_braceright,
    KC(51),	KS_semicolon,	KS_plus,
    KC(52),	KS_colon,	KS_asterisk,
    KC(53),	KS_Zenkaku_Hankaku,
    KC(54),	KS_comma,	KS_less,
    KC(55),	KS_period,	KS_greater,
    KC(56),	KS_slash,	KS_question,

    KC(57),	KS_Caps_Lock,

    KC(58),	KS_f1,
    KC(59),	KS_f2,
    KC(60),	KS_f3,
    KC(61),	KS_f4,
    KC(62),	KS_f5,
    KC(63),	KS_f6,
    KC(64),	KS_f7,
    KC(65),	KS_f8,
    KC(66),	KS_f9,
    KC(67),	KS_f10,
    KC(68),	KS_f11,
    KC(69),	KS_f12,

    KC(70),	KS_Print_Screen,
    KC(71),	KS_Hold_Screen,
    KC(72),	KS_Pause,
    KC(73),	KS_Insert,
    KC(74),	KS_Home,
    KC(75),	KS_Prior,
    KC(76),	KS_Delete,
    KC(77),	KS_End,
    KC(78),	KS_Next,
    KC(79),	KS_Right,
    KC(80),	KS_Left,
    KC(81),	KS_Down,
    KC(82),	KS_Up,

    /* 83-99 is numeric keypad */

    /* 100 is european keyboard only */

    KC(101),	KS_Help,

    /* no 102-134 */

    KC(135),	KS_backslash,	KS_underscore,
    KC(136),	KS_Hiragana_Katakana,
    KC(137),	KS_yen,		KS_bar,
    KC(138),	KS_Henkan_Mode,
    KC(139),	KS_Muhenkan,

    KC(0x100),	KS_Control_L,
    KC(0x101),	KS_Shift_L,
    KC(0x102),	KS_Alt_L,
    KC(0x103),	KS_Meta_L,
    KC(0x104),	KS_Control_R,
    KC(0x105),	KS_Shift_R,
    KC(0x106),	KS_Alt_R,
    KC(0x107),	KS_Meta_R,
};

#define KBD_MAP(name, base, map) \
			{ name, base, sizeof(map)/sizeof(keysym_t), map }

static const struct wscons_keydesc mkbd_keydesctab[] = {
	KBD_MAP(KB_JP,			0,	mkbd_keydesc_jp),
/*	KBD_MAP(KB_UK,		    KB_JP,	mkbd_keydesc_uk),  */
	{0, 0, 0, 0}
};

#undef KBD_MAP
#undef KC
