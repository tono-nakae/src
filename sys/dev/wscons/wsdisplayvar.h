/* $NetBSD: wsdisplayvar.h,v 1.2 1998/05/14 20:49:57 drochner Exp $ */

/*
 * Copyright (c) 1996, 1997 Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

struct device;

/*
 * WSDISPLAY interfaces
 */

/*
 * Emulation functions, for displays that can support glass-tty terminal
 * emulations.  These are character oriented, with row and column
 * numbers starting at zero in the upper left hand corner of the
 * screen.
 *
 * These are used only when emulating a terminal.  Therefore, displays
 * drivers which cannot emulate terminals do not have to provide them.
 *
 * There is a "void *" cookie provided by the display driver associated
 * with these functions, which is passed to them when they are invoked.
 */
struct wsdisplay_emulops {
	void	(*cursor) __P((void *c, int on, int row, int col));
	void	(*putstr) __P((void *c, int row, int col,
			       char *cp, int n, long));
	void	(*copycols) __P((void *c, int row, int srccol, int dstcol,
		    int ncols));
	void	(*erasecols) __P((void *c, int row, int startcol,
		    int ncols, long));
	void	(*copyrows) __P((void *c, int srcrow, int dstrow,
		    int nrows));
	void	(*eraserows) __P((void *c, int row, int nrows, long));
	int	(*alloc_attr) __P((void *c, int fg, int bg, int flags, long *));
/* fg / bg values. Made identical to ANSI terminal color codes. */
#define WSCOL_BLACK	0
#define WSCOL_RED	1
#define WSCOL_GREEN	2
#define WSCOL_BROWN	3
#define WSCOL_BLUE	4
#define WSCOL_MAGENTA	5
#define WSCOL_CYAN	6
#define WSCOL_WHITE	7
/* flag values: */
#define WSATTR_REVERSE	1
#define WSATTR_HILIT	2
#define WSATTR_BLINK	4
#define WSATTR_UNDERLINE 8
#define WSATTR_WSCOLORS 16
	/* XXX need a free_attr() ??? */
};

struct wsscreen_descr {
	char *name;
	int ncols, nrows;
	const struct wsdisplay_emulops *textops;
	int fontwidth, fontheight;
	int capabilities;
#define WSSCREEN_WSCOLORS	1	/* minimal color capability */
#define WSSCREEN_REVERSE	2	/* can display reversed */
#define WSSCREEN_HILIT		4	/* can highlight (however) */
#define WSSCREEN_BLINK		8	/* can blink */
#define WSSCREEN_UNDERLINE	16	/* can underline */
};

/*
 * Display access functions, invoked by user-land programs which require
 * direct device access, such as X11.
 *
 * There is a "void *" cookie provided by the display driver associated
 * with these functions, which is passed to them when they are invoked.
 */
struct wsdisplay_accessops {
	int	(*ioctl) __P((void *v, u_long cmd, caddr_t data, int flag,
		    struct proc *p));
	int	(*mmap) __P((void *v, off_t off, int prot));
	int	(*alloc_screen) __P((void *, const struct wsscreen_descr *,
				     void **, int *, int *, long *));
	void	(*free_screen) __P((void *, void *));
	void	(*show_screen) __P((void *, void *));
	int	(*load_font) __P((void *, void *, int, int, int, void *));
};

/*
 * Attachment information provided by wsdisplaydev devices when attaching
 * wsdisplay units.
 */
struct wsdisplaydev_attach_args {
	const struct wsdisplay_accessops *accessops;	/* access ops */
	void	*accesscookie;				/* access cookie */
};

/* passed to wscons by the video driver to tell about its capabilities */
struct wsscreen_list {
	int nscreens;
	const struct wsscreen_descr **screens;
};

/*
 * Attachment information provided by wsemuldisplaydev devices when attaching
 * wsdisplay units.
 */
struct wsemuldisplaydev_attach_args {
	int	console;				/* is it console? */
	const struct wsscreen_list *scrdata;		/* screen cfg info */
	const struct wsdisplay_accessops *accessops;	/* access ops */
	void	*accesscookie;				/* access cookie */
};

#define	wsemuldisplaydevcf_console	cf_loc[0]	/* spec'd as console? */
#define	WSEMULDISPLAYDEVCF_CONSOLE_UNK	-1

/*
 * Autoconfiguration helper functions.
 */
void	wsdisplay_cnattach __P((const struct wsscreen_descr *, void *,
				int, int, long));
int	wsdisplaydevprint __P((void *, const char *));
int	wsemuldisplaydevprint __P((void *, const char *));

/*
 * Console interface.
 */
void	wsdisplay_cnputc __P((dev_t dev, int i));
