/*	$NetBSD: label.c,v 1.31 2003/06/16 19:42:14 dsl Exp $	*/

/*
 * Copyright 1997 Jonathan Stone
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
 *      This product includes software developed for the NetBSD Project by
 *      Jonathan Stone.
 * 4. The name of Jonathan Stone may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JONATHAN STONE ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PIERMONT INFORMATION SYSTEMS INC. BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#if defined(LIBC_SCCS) && !defined(lint)
__RCSID("$NetBSD: label.c,v 1.31 2003/06/16 19:42:14 dsl Exp $");
#endif

#include <sys/types.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <util.h>
#include <unistd.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>

#include "defs.h"
#include "msg_defs.h"
#include "menu_defs.h"

struct ptn_menu_info {
	int	menu_no;
	int	flags;
#define PIF_SHOW_UNUSED		1
	char	texts[MAXPARTITIONS][70];
};

/*
 * local prototypes
 */
static int boringpart(partinfo *, int, int, int);

int	checklabel(partinfo *, int, int, int, int *, int *);
void	translate_partinfo(partinfo *lp, struct partition *pp);
void	atofsb(const char *, int *, int *);


/*
 * Return 1 if partition i in lp should be ignored when checking
 * for overlapping partitions.
 */
static int
boringpart(partinfo *lp, int i, int rawpart, int bsdpart)
{

	if (i == rawpart || i == bsdpart ||
	    lp[i].pi_fstype == FS_UNUSED || lp[i].pi_size == 0)
		return 1;
	return 0;
}



/*
 * Check a sysinst label structure for overlapping partitions.
 * Returns 0 if no overlapping partition found, nonzero otherwise.
 * Sets reference arguments ovly1 and ovly2 to the indices of
 * overlapping partitions if any are found.
 */
int
checklabel(partinfo *lp, int nparts, int rawpart, int bsdpart,
	int *ovly1, int *ovly2)
{
	int i;
	int j;

	*ovly1 = -1;
	*ovly2 = -1;

	for (i = 0; i < nparts - 1; i ++ ) {
		partinfo *ip = &lp[i];
		int istart, istop;

		/* skip unused or reserved partitions */
		if (boringpart(lp, i, rawpart, bsdpart))
			continue;

		/*
		 * check succeding partitions for overlap.
		 * O(n^2), but n is small (currently <= 16).
		 */
		istart = ip->pi_offset;
		istop = istart + ip->pi_size;

		for (j = i+1; j < nparts; j++) {
			partinfo *jp = &lp[j];
			int jstart, jstop;

			/* skip unused or reserved partitions */
			if (boringpart(lp, j, rawpart, bsdpart))
				continue;

			jstart = jp->pi_offset;
			jstop = jstart + jp->pi_size;

			/* overlap? */
			if ((istart <= jstart && jstart < istop) ||
			    (jstart <= istart && istart < jstop)) {
				*ovly1 = i;
				*ovly2 = j;
				return (1);
			}
		}
	}

	return (0);
}

static int
edit_fs_start(menudesc *m, menu_ent *e, void *arg)
{
	partinfo *p = arg;
	int start, size;

	start = getpartoff(p->pi_offset);
	size = p->pi_size;
	if (size != 0) {
		/* Try to keep end in the same place */
		size += p->pi_offset - start;
		if (size < 0)
			size = 0;
		p->pi_size = size;
	}
	p->pi_offset = start;
	return 0;
}

static int
edit_fs_size(menudesc *m, menu_ent *e, void *arg)
{
	partinfo *p = arg;
	int size;

	size = getpartsize(p->pi_offset, p->pi_size);
	if (size == -1)
		size = dlsize - p->pi_offset;
	p->pi_size = size;
	return 0;
}

int
set_bsize(partinfo *p, int size)
{

	if (!PI_ISBSDFS(p))
		size = 0;

	p->pi_bsize = size;
	p->pi_fsize = size / 8;

	return 0;
}

int
set_fsize(partinfo *p, int nfrag)
{

	p->pi_fsize = p->pi_bsize / nfrag;
	return 0;
}

static int
edit_fs_preserve(menudesc *m, menu_ent *e, void *arg)
{
	partinfo *p = arg;

	p->pi_newfs ^= 1;
	return 0;
}

static int
edit_fs_mountpt(menudesc *m, menu_ent *e, void *arg)
{
	partinfo *p = arg;

	if (PI_ISBSDFS(p) || p->pi_fstype == FS_MSDOS) {
		msg_prompt_add(MSG_mountpoint,
			p->pi_mount,
			p->pi_mount,
			sizeof p->pi_mount);
		if (strcmp(p->pi_mount, "none") == 0)
			p->pi_mount[0] = '\0';
	} else {
		msg_display(MSG_nomount, 'a' + p - bsdlabel);
		process_menu(MENU_ok, NULL);
	}

	return 0;
}

static int
edit_restore(menudesc *m, menu_ent *e, void *arg)
{
	partinfo *p = arg;

	p->pi_reset = 1;
	return 1;
}

static void
set_ptn_labels(menudesc *m, void *arg)
{
	static char l[8][70];
	partinfo *p = arg;
	menu_ent *opt;
	int i;
	int ms = MEG / sectorsize;
	int s;

	msg_clear();
	msg_table_add(MSG_edfspart, 'a' + (p - bsdlabel));

	snprintf(l[0], sizeof l[0], msg_string(MSG_fstype_fmt),
		fstypenames[p->pi_fstype]);
	s = p->pi_offset;
	snprintf(l[1], sizeof l[0], msg_string(MSG_start_fmt),
		s / ms, s / dlcylsize, s );
	s = p->pi_size;
	snprintf(l[2], sizeof l[0], msg_string(MSG_size_fmt),
		s / ms, s / dlcylsize, s );
	s = p->pi_offset + p->pi_size;
	snprintf(l[3], sizeof l[0], msg_string(MSG_end_fmt),
		s / ms, s / dlcylsize, s );

	if (PI_ISBSDFS(p)) {
		m->opts[4].opt_menu = MENU_selbsize;
		m->opts[5].opt_menu = MENU_selfsize;
		snprintf(l[4], sizeof l[0],
			msg_string(MSG_bsize_fmt), p->pi_bsize);
		snprintf(l[5], sizeof l[0],
			msg_string(MSG_fsize_fmt), p->pi_fsize);
	} else {
		m->opts[4].opt_menu = OPT_NOMENU;
		m->opts[5].opt_menu = OPT_NOMENU;
		strcpy(l[4], "      -");
		strcpy(l[5], "      -");
	}

	snprintf(l[6], sizeof l[0], msg_string(MSG_preserve_fmt),
		msg_string(p->pi_newfs ? MSG_no : MSG_yes));
	snprintf(l[7], sizeof l[0], msg_string(MSG_mount_fmt), p->pi_mount);

	for (opt = m->opts, i = 0; i < nelem(l); i++, opt++)
		opt->opt_name = l[i];
}

static int
edit_ptn(menudesc *menu, menu_ent *opt, void *arg)
{
	static menu_ent fs_fields[] = {
	    {"", MENU_selfskind, OPT_SUB, NULL},
	    {"", OPT_NOMENU, 0, edit_fs_start},
	    {"", OPT_NOMENU, 0, edit_fs_size},
	    {"", OPT_NOMENU, 0, NULL},
	    {"", MENU_selbsize, OPT_SUB, NULL},
	    {"", MENU_selfsize, OPT_SUB, NULL},
	    {"", OPT_NOMENU, 0, edit_fs_preserve},
	    {"", OPT_NOMENU, 0, edit_fs_mountpt},
	    {MSG_askunits, MENU_sizechoice, OPT_SUB, NULL},
	    {MSG_restore, OPT_NOMENU, 0, edit_restore},
	};
	static int fspart_menu = -1;
	partinfo *p, p_save;

	if (fspart_menu == -1) {
		if (!check_lfs_progs())
			set_menu_numopts(MENU_selfskind, 4);
		fspart_menu = new_menu(NULL, fs_fields, nelem(fs_fields),
			0, 7, 0, 70,
			MC_NOBOX | MC_NOCLEAR | MC_SCROLL,
			set_ptn_labels, NULL,
			NULL, MSG_partition_sizes_ok);
	}

	p = bsdlabel + (opt - menu->opts);
	p->pi_reset = 0;
	p_save = *p;
	for (;;) {
		process_menu(fspart_menu, p);
		if (!p->pi_reset)
			break;
		*p = p_save;
	}

	return 0;
}

static int
show_all_unused(menudesc *m, menu_ent *opt, void *arg)
{
	struct ptn_menu_info *pi = arg;

	pi->flags |= PIF_SHOW_UNUSED;
	return 0;
}

static void
set_label_texts(menudesc *menu, void *arg)
{
	struct ptn_menu_info *pi = arg;
	menu_ent *m;
	int ptn, last_used_ptn;
	int rawptn = getrawpartition();
	int maxpart = getmaxpartitions();

	msg_display(MSG_fspart, multname);
	msg_table_add(MSG_fspart_header);

	for (last_used_ptn = 0, ptn = 0; ptn < maxpart; ptn++) {
		if (bsdlabel[ptn].pi_fstype != FS_UNUSED)
			last_used_ptn = ptn;
		m = &menu->opts[ptn];
		m->opt_menu = OPT_NOMENU;
		m->opt_flags = 0;
		m->opt_name = pi->texts[ptn];
		if (ptn == rawptn
#ifdef PART_BOOT
		    || ptn == PART_BOOT
#endif
		    || ptn == C) {
			m->opt_action = NULL;
		} else
			m->opt_action = edit_ptn;
		fmt_fspart(pi->texts[ptn], sizeof pi->texts[0], ptn);
	}

	if (!(pi->flags & PIF_SHOW_UNUSED) && ptn != ++last_used_ptn) {
		ptn = last_used_ptn;
		m = &menu->opts[ptn];
		m->opt_name = MSG_show_all_unused_partitions;
		m->opt_action = show_all_unused;
		ptn++;
	}

	m = &menu->opts[ptn];
	m->opt_menu = MENU_sizechoice; 
	m->opt_flags = OPT_SUB; 
	m->opt_action = NULL;
	m->opt_name = MSG_askunits;

	set_menu_numopts(pi->menu_no, ptn + 1);
}

/*
 * Check a disklabel.
 * If there are overlapping active parititons,
 * Ask the user if they want to edit the parittion or give up.
 */
int
edit_and_check_label(partinfo *lp, int nparts, int rawpart, int bsdpart)
{
	static struct menu_ent *menu;
	static struct ptn_menu_info *pi;
	int maxpart = getmaxpartitions();

	if (menu == NULL) {
		menu = malloc((maxpart + 1) * sizeof *menu);
		pi = malloc(offsetof(struct ptn_menu_info, texts[maxpart]) );
		if (!menu || !pi)
			return 1;

		pi->flags = 0;
		current_cylsize = dlcylsize;

		pi->menu_no = new_menu(NULL, menu, maxpart + 1,
			0, 6, maxpart + 2, 74,
			MC_SCROLL | MC_NOBOX | MC_DFLTEXIT,
			set_label_texts, NULL, NULL,
			MSG_partition_sizes_ok);
	}

	if (pi->menu_no < 0)
		return 1;

	for (;;) {
		int i, j;

		/* first give the user the option to edit the label... */
		process_menu(pi->menu_no, pi);

		/* User thinks the label is OK. check for overlaps */
		if (checklabel(lp, nparts, rawpart, bsdpart, &i, &j) == 0) {
			/* partitions are OK */
			return (1);
		}
		 
		/* partitions overlap */
		msg_display(MSG_partitions_overlap, 'a' + i, 'a' + j);
		/*XXX*/
		msg_display_add(MSG_edit_partitions_again);
		process_menu(MENU_yesno, NULL);
		if (!yesno)
			return(0);
	}

	/*NOTREACHED*/
}

	
void
emptylabel(partinfo *lp)
{
	register int i, maxpart;

	maxpart = getmaxpartitions();

	for (i = 0; i < maxpart; i++) {
		lp[i].pi_fstype = FS_UNUSED;
		lp[i].pi_offset = 0;
		lp[i].pi_size = 0;
		lp[i].pi_bsize = 0;
		lp[i].pi_fsize = 0;
	}
}

/*
 * XXX MSDOS?
 */
void
translate_partinfo(partinfo *lp, struct partition *pp)
{

	lp->pi_fstype = pp->p_fstype;

	switch (pp->p_fstype) {

	case FS_UNUSED:				/* XXX */
	case FS_SWAP:
		break;

	case FS_BSDLFS:
	case FS_BSDFFS:
		(*lp).pi_offset = 0;
		(*lp).pi_size = 0;
		(*lp).pi_bsize = pp->p_fsize * pp->p_frag;
		(*lp).pi_fsize = pp->p_fsize;
		break;

	case FS_EX2FS:
		(*lp).pi_fstype = FS_UNUSED;	/* XXX why? */
		(*lp).pi_bsize = pp->p_fsize * pp->p_frag;
		(*lp).pi_fsize = pp->p_fsize;
		break;

	default:
		(*lp).pi_fstype = FS_UNUSED;
		break;
	}
}

/*
 * Read a label from disk into a sysist label structure.
 */
int
incorelabel(const char *dkname, partinfo *lp)
{
	struct disklabel lab;
	int fd;
	int i, maxpart;
	struct partition *pp;
	char nambuf[STRSIZE];

	fd = opendisk(dkname, O_RDONLY,  nambuf, STRSIZE, 0);

	if (ioctl(fd, DIOCGDINFO, &lab) < 0) {
		/*XXX err(4, "ioctl DIOCGDINFO");*/
		return(errno);
	}
	close(fd);
	touchwin(stdscr);
	maxpart = getmaxpartitions();
	if (maxpart > 16)
		maxpart = 16;


	/* XXX set globals used by MD code to compute disk size? */
	
	pp = &lab.d_partitions[0];
	emptylabel(lp);
	for (i = 0; i < maxpart; i++) {
		translate_partinfo(lp+i, pp+i);
	}

	return (0);
}

/* Ask for a partition offset, check bounds and do the needed roundups */
int
getpartoff(int defpartstart)
{
	char defsize[20], isize[20], maxpartc;
	int i, localsizemult, partn;
	const char *errmsg = "\n";

	maxpartc = 'a' + getmaxpartitions() - 1;
	for (;;) {
		snprintf(defsize, sizeof defsize, "%d", defpartstart/sizemult);
		msg_prompt_win(MSG_label_offset, -1, 13, 70, 9,
		    (defpartstart > 0) ? defsize : NULL, isize, sizeof isize,
		    errmsg, maxpartc, maxpartc, multname);
		if (strcmp(defsize, isize) == 0)
			/* Don't do rounding if default accepted */
			return defpartstart;
		if (isize[1] == '\0' && isize[0] >= 'a' &&
		    isize[0] <= maxpartc) {
			partn = isize[0] - 'a';
			i = bsdlabel[partn].pi_size + bsdlabel[partn].pi_offset;
			localsizemult = 1;
		} else if (atoi(isize) == -1) {
			i = ptstart;
			localsizemult = 1;
		} else
			atofsb(isize, &i, &localsizemult);
		if (i < 0) {
			errmsg = msg_string(MSG_invalid_sector_number);
			continue;
		}
		/* round to cylinder size if localsizemult != 1 */
		i = NUMSEC(i/localsizemult, localsizemult, dlcylsize);
		/* Adjust to start of slice if needed */
		if ((i < ptstart && (ptstart - i) < localsizemult) ||
		    (i > ptstart && (i - ptstart) < localsizemult)) {
			i = ptstart;
		}
		if (i <= dlsize)
			break;
		errmsg = msg_string(MSG_startoutsidedisk);
	}
	return i;
}


/* Ask for a partition size, check bounds and does the needed roundups */
int
getpartsize(int partstart, int defpartsize)
{
	char dsize[20], isize[20], maxpartc;
	const char *errmsg = "\n";
	int i, partend, localsizemult;
	int fsptend = ptstart + ptsize;
	int partn;

	maxpartc = 'a' + getmaxpartitions() - 1;
	for (;;) {
		snprintf(dsize, sizeof dsize, "%d", defpartsize/sizemult);
		msg_prompt_win(MSG_label_size, -1, 12, 70, 9,
		    (defpartsize != 0) ? dsize : 0, isize, sizeof isize,
		    errmsg, maxpartc, multname);
		if (strcmp(isize, dsize) == 0)
			return defpartsize;
		if (isize[1] == '\0' && isize[0] >= 'a' &&
		    isize[0] <= maxpartc) {
			partn = isize[0] - 'a';
			i = bsdlabel[partn].pi_offset - partstart;
			localsizemult = 1;
		} else if (atoi(isize) == -1) {
			i = fsptend - partstart;
			localsizemult = 1;
		} else
			atofsb(isize, &i, &localsizemult);
		if (i < 0) {
			errmsg = msg_string(MSG_invalid_sector_number);
			continue;
		}
		/*
		 * partend is aligned to a cylinder if localsizemult
		 * is not 1 sector
		 */
		partend = NUMSEC((partstart + i) / localsizemult,
		    localsizemult, dlcylsize);
		/* Align to end-of-disk or end-of-slice if close enough */
		i = dlsize - partend;
		if (i > -localsizemult && i < localsizemult)
			partend = dlsize;
		i = fsptend - partend;
		if (i > -localsizemult && i < localsizemult)
			partend = fsptend;
		/* sanity checks */
		if (partend > dlsize) {
			partend = dlsize;
			msg_prompt_win(MSG_endoutsidedisk, -1, 13, 70, 6,
			    NULL, isize, 1,
			    (partend - partstart) / sizemult, multname);
		}
		/* return value */
		return (partend - partstart);
	}
	/* NOTREACHED */
}

/*
 * convert a string to a number of sectors, with a possible unit
 * 150M = 150 Megabytes
 * 2000c = 2000 cylinders
 * 150256s = 150256 sectors
 * Without units, use the default (sizemult)
 * returns the number of sectors, and the unit used (for roundups).
 */

void
atofsb(const char *str, int *p_val, int *localsizemult)
{
	int i;
	int val;

	*localsizemult = sizemult;
	if (str[0] == '\0') {
		*p_val = -1;
		return;
	}
	val = 0;
	for (i = 0; str[i] != '\0'; i++) {
		if (str[i] >= '0' && str[i] <= '9') {
			val = val * 10 + str[i] - '0';
			continue;
		}
		if (str[i + 1] != '\0') {
			/* A non-digit caracter, not at the end */
			*p_val = -1;
			return;
		}
		if (str[i] == 'G' || str[i] == 'g') {
			val *= 1024;
			*localsizemult = MEG / sectorsize;
			break;
		}
		if (str[i] == 'M' || str[i] == 'm') {
			*localsizemult = MEG / sectorsize;
			break;
		}
		if (str[i] == 'c') {
			*localsizemult = dlcylsize;
			break;
		}
		if (str[i] == 's') {
			*localsizemult = 1;
			break;
		}
		/* not a known unit */
		*p_val = -1;
		return;
	}
	*p_val = val * (*localsizemult);
	return;
}
