/*	$NetBSD: display.c,v 1.6 2004/07/29 22:29:37 jmmv Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
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

#include <sys/ioctl.h>
#include <sys/time.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <dev/wscons/wsconsio.h>

#include "wsconsctl.h"

static int border;
static int dpytype;
static struct wsdisplay_usefontdata font;
static struct wsdisplay_scroll_data scroll_l;
static int havescroll = 1;
static int msg_default_attrs, msg_default_bg, msg_default_fg;
static int msg_kernel_attrs, msg_kernel_bg, msg_kernel_fg;

struct field display_field_tab[] = {
    { "border",			&border,	FMT_COLOR,	FLG_MODIFY },
    { "type",			&dpytype,	FMT_DPYTYPE,	FLG_RDONLY },
    { "font",			&font.name,	FMT_STRING,	FLG_WRONLY },
    { "scroll.fastlines",	&scroll_l.fastlines, FMT_UINT, FLG_MODIFY },
    { "scroll.slowlines",	&scroll_l.slowlines, FMT_UINT, FLG_MODIFY },
    { "msg.default.attrs",	&msg_default_attrs, FMT_ATTRS,	FLG_MODIFY },
    { "msg.default.bg",		&msg_default_bg, FMT_COLOR,	FLG_MODIFY },
    { "msg.default.fg",		&msg_default_fg, FMT_COLOR,	FLG_MODIFY },
    { "msg.kernel.attrs",	&msg_kernel_attrs, FMT_ATTRS,	FLG_MODIFY },
    { "msg.kernel.bg",		&msg_kernel_bg, FMT_COLOR,	FLG_MODIFY },
    { "msg.kernel.fg",		&msg_kernel_fg, FMT_COLOR,	FLG_MODIFY },
};

int display_field_tab_len = sizeof(display_field_tab)/
			     sizeof(display_field_tab[0]);

static int
init_values(void)
{
	scroll_l.which = 0;

	if (field_by_value(&scroll_l.fastlines)->flags & FLG_GET)
		scroll_l.which |= WSDISPLAY_SCROLL_DOFASTLINES;
	if (field_by_value(&scroll_l.slowlines)->flags & FLG_GET)
		scroll_l.which |= WSDISPLAY_SCROLL_DOSLOWLINES;

	return scroll_l.which;

}
void
display_get_values(fd)
	int fd;
{
	if (field_by_value(&dpytype)->flags & FLG_GET)
		if (ioctl(fd, WSDISPLAYIO_GTYPE, &dpytype) < 0)
			err(1, "WSDISPLAYIO_GTYPE");
	
	if (field_by_value(&border)->flags & FLG_GET)
		if (ioctl(fd, WSDISPLAYIO_GBORDER, &border) < 0)
			warn("WSDISPLAYIO_GBORDER");
	
	if (field_by_value(&msg_default_attrs)->flags & FLG_GET ||
	    field_by_value(&msg_default_bg)->flags & FLG_GET ||
	    field_by_value(&msg_default_fg)->flags & FLG_GET ||
	    field_by_value(&msg_kernel_attrs)->flags & FLG_GET ||
	    field_by_value(&msg_kernel_bg)->flags & FLG_GET ||
	    field_by_value(&msg_kernel_fg)->flags & FLG_GET) {
		struct wsdisplay_msgattrs ma;

		if (ioctl(fd, WSDISPLAYIO_GMSGATTRS, &ma) < 0) {
			warnx("no support to change console/kernel colors");

			ma.default_attrs = -1;
			ma.default_bg = -1;
			ma.default_fg = -1;

			ma.kernel_attrs = -1;
			ma.kernel_bg = -1;
			ma.kernel_fg = -1;
		}

		msg_default_attrs = ma.default_attrs;
		if (ma.default_attrs & WSATTR_WSCOLORS) {
			msg_default_bg = ma.default_bg;
			msg_default_fg = ma.default_fg;
		} else
			msg_default_bg = msg_default_fg = -1;

		msg_kernel_attrs = ma.kernel_attrs;
		if (ma.kernel_attrs & WSATTR_WSCOLORS) {
			msg_kernel_bg = ma.kernel_bg;
			msg_kernel_fg = ma.kernel_fg;
		} else
			msg_kernel_bg = msg_kernel_fg = -1;
	}

	if (init_values() == 0 || havescroll == 0)
		return;

	if (ioctl(fd, WSDISPLAYIO_DGSCROLL, &scroll_l) < 0) {
		if (errno != ENODEV)
			err(1, "WSDISPLAYIO_GSCROLL");
		else
			havescroll = 0;
	}
}

void
display_put_values(fd)
	int fd;
{
	if (field_by_value(&font.name)->flags & FLG_SET) {
		if (ioctl(fd, WSDISPLAYIO_SFONT, &font) < 0)
			err(1, "WSDISPLAYIO_SFONT");
		pr_field(field_by_value(&font.name), " -> ");
	}

	if (field_by_value(&border)->flags & FLG_SET) {
		if (ioctl(fd, WSDISPLAYIO_SBORDER, &border) < 0)
			err(1, "WSDISPLAYIO_SBORDER");
		pr_field(field_by_value(&border), " -> ");
	}

	if (field_by_value(&msg_default_attrs)->flags & FLG_SET ||
	    field_by_value(&msg_default_bg)->flags & FLG_SET ||
	    field_by_value(&msg_default_fg)->flags & FLG_SET ||
	    field_by_value(&msg_kernel_attrs)->flags & FLG_SET ||
	    field_by_value(&msg_kernel_bg)->flags & FLG_SET ||
	    field_by_value(&msg_kernel_fg)->flags & FLG_SET) {
		struct wsdisplay_msgattrs ma;

		if (ioctl(fd, WSDISPLAYIO_GMSGATTRS, &ma) < 0)
			err(1, "WSDISPLAYIO_GMSGATTRS");

		if (field_by_value(&msg_default_attrs)->flags & FLG_SET) {
			ma.default_attrs = msg_default_attrs;
			pr_field(field_by_value(&msg_default_attrs), " -> ");
		}
		if (ma.default_attrs & WSATTR_WSCOLORS) {
			if (field_by_value(&msg_default_bg)->flags & FLG_SET) {
				ma.default_bg = msg_default_bg;
				pr_field(field_by_value(&msg_default_bg),
				         " -> ");
			}
			if (field_by_value(&msg_default_fg)->flags & FLG_SET) {
				ma.default_fg = msg_default_fg;
				pr_field(field_by_value(&msg_default_fg),
				         " -> ");
			}
		}

		if (field_by_value(&msg_kernel_attrs)->flags & FLG_SET) {
			ma.kernel_attrs = msg_kernel_attrs;
			pr_field(field_by_value(&msg_kernel_attrs), " -> ");
		}
		if (ma.default_attrs & WSATTR_WSCOLORS) {
			if (field_by_value(&msg_kernel_bg)->flags & FLG_SET) {
				ma.kernel_bg = msg_kernel_bg;
				pr_field(field_by_value(&msg_kernel_bg),
				         " -> ");
			}
			if (field_by_value(&msg_kernel_fg)->flags & FLG_SET) {
				ma.kernel_fg = msg_kernel_fg;
				pr_field(field_by_value(&msg_kernel_fg),
				         " -> ");
			}
		}

		if (ioctl(fd, WSDISPLAYIO_SMSGATTRS, &ma) < 0)
			err(1, "WSDISPLAYIO_SMSGATTRS");
	}

	if (init_values() == 0 || havescroll == 0)
		return;

	if (scroll_l.which & WSDISPLAY_SCROLL_DOFASTLINES)
		pr_field(field_by_value(&scroll_l.fastlines), " -> ");
	if (scroll_l.which & WSDISPLAY_SCROLL_DOSLOWLINES)
		pr_field(field_by_value(&scroll_l.slowlines), " -> ");

	if (ioctl(fd, WSDISPLAYIO_DSSCROLL, &scroll_l) < 0) {
		if (errno != ENODEV)
			err (1, "WSDISPLAYIO_DSSCROLL");
		else {
			warnx("scrolling is not supported by this kernel");
			havescroll = 0;
		}
	}
}
