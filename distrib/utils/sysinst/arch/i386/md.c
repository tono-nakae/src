/*	$NetBSD: md.c,v 1.88 2003/06/14 12:58:49 dsl Exp $ */

/*
 * Copyright 1997 Piermont Information Systems Inc.
 * All rights reserved.
 *
 * Written by Philip A. Nelson for Piermont Information Systems Inc.
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
 *      Piermont Information Systems Inc.
 * 4. The name of Piermont Information Systems Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PIERMONT INFORMATION SYSTEMS INC. ``AS IS''
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

/* md.c -- Machine specific code for i386 */

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/exec.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <machine/cpu.h>
#include <stdio.h>
#include <stddef.h>
#include <util.h>
#include <dirent.h>
#include "defs.h"
#include "md.h"
#include "endian.h"
#include "msg_defs.h"
#include "menu_defs.h"
#include <sys/bootblock.h>

#ifdef NO_LBA_READS		/* for testing */
#undef BIFLAG_EXTINT13
#define BIFLAG_EXTINT13	0
#endif

mbr_sector_t mbr;
struct nativedisk_info *nativedisk;
static struct biosdisk_info *biosdisk = NULL;
int netbsd_mbr_installed = 0;

/* prototypes */

static int md_read_bootcode(const char *, mbr_sector_t *);
static int count_mbr_parts(struct mbr_partition *);
static int mbr_root_above_chs(struct mbr_partition *);
static void md_upgrade_mbrtype(void);
#if defined(__i386__)
static unsigned int get_bootmodel(void);
#endif


int
md_get_info(void)
{
	if (read_mbr(diskdev, &mbr) < 0)
		memset(&mbr, 0, sizeof mbr - 2);
	md_bios_info(diskdev);

edit:
	edit_mbr(&mbr);

	root_limit = 0;
	if (biosdisk == NULL || !(biosdisk->bi_flags & BIFLAG_EXTINT13)) {
		if (mbr_root_above_chs(part)) {
			msg_display(MSG_partabovechs);
			process_menu(MENU_noyes, NULL);
			if (!yesno)
				goto edit;
			/* The user is shooting themselves in the foot here...*/
		} else
			root_limit = bcyl * bhead * bsec;
	}

	if (count_mbr_parts(part) > 1) {
		msg_display(MSG_installbootsel);
		process_menu(MENU_yesno, NULL);
		if (yesno && !md_read_bootcode(_PATH_BOOTSEL, &mbr)) {
			configure_bootsel();
			netbsd_mbr_installed = 2;
		}
	}

	if (!netbsd_mbr_installed) {
		if (mbr_root_above_chs(part))
			msg_display(MSG_installlbambr);
		else if (count_mbr_parts(part) > 1)
			msg_display(MSG_installnormalmbr);
		else
			msg_display(MSG_installmbr);
		process_menu(MENU_yesno, NULL);
		if (yesno && !md_read_bootcode(_PATH_MBR, &mbr))
			netbsd_mbr_installed = 1;
	}

	return 1;
}

/*
 * Read MBR code from a file.
 * The existing partition table and bootselect configuration is kept.
 */
static int
md_read_bootcode(const char *path, mbr_sector_t *mbr)
{
	int fd;
	struct stat st;
	size_t len;
	mbr_sector_t new_mbr;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return -1;

	if (fstat(fd, &st) < 0 || st.st_size != sizeof *mbr) {
		close(fd);
		return -1;
	}

	if (read(fd, &new_mbr, sizeof new_mbr) != sizeof new_mbr) {
		close(fd);
		return -1;
	}
	close(fd);

	if (new_mbr.mbr_bootsel.mbrb_magic != native_to_le16(MBR_MAGIC))
		return -1;

	if (mbr->mbr_bootsel.mbrb_magic == native_to_le16(MBR_MAGIC)) {
		len = offsetof(mbr_sector_t, mbr_bootsel);
		if (!(mbr->mbr_bootsel.mbrb_flags & BFL_NEWMBR))
			/*
			 * Meaning of keys has changed, force a sensible
			 * default (old code didn't preseve the answer).
			 */
			mbr->mbr_bootsel.mbrb_defkey = SCAN_ENTER;
	} else
		len = offsetof(mbr_sector_t, mbr_parts);

	memcpy(mbr, &new_mbr, len);
	/* Keep flags from object file - indicate the properties */
	mbr->mbr_bootsel.mbrb_flags = new_mbr.mbr_bootsel.mbrb_flags;
	mbr->mbr_signature = native_to_le16(MBR_MAGIC);

	return 0;
}

int
md_pre_disklabel(void)
{
	msg_display(MSG_dofdisk);

	/* write edited MBR onto disk. */
	if (write_mbr(diskdev, &mbr, 1) != 0) {
		msg_display(MSG_wmbrfail);
		process_menu(MENU_ok, NULL);
		return 1;
	}
	md_upgrade_mbrtype();
	return 0;
}

int
md_post_disklabel(void)
{
	if (rammb <= 32)
		set_swap(diskdev, bsdlabel, 1);

	return 0;
}

int
md_post_newfs(void)
{
	int ret;
	int len;
	int td, sd;
	char bootxx[8192 + 4];
#define bp (*(struct i386_boot_params *)(bootxx + 512 * 2 + 8))

	ret = cp_to_target("/usr/mdec/biosboot", "/boot");
	if (ret)
		return ret;

	msg_display(MSG_getboottype);
	process_menu(MENU_getboottype, NULL);
	msg_display(MSG_dobootblks, diskdev);

	/* Copy bootstrap in by hand - /sbin/installboot explodes ramdisks */
	ret = 1;

	snprintf(bootxx, sizeof bootxx, "/dev/r%sa", diskdev);
	td = open(bootxx, O_RDWR);
	sd = open("/usr/mdec/bootxx_ffsv1", O_RDONLY);
	if (td == -1 || sd == -1)
		goto bad_bootxx;
	len = read(sd, bootxx, sizeof bootxx);
	if (len < 2048 || len > 8192)
		goto bad_bootxx;

	if (*(uint32_t *)(bootxx + 512 * 2 + 4) != X86_BOOT_MAGIC_1)
		goto bad_bootxx;

	if (!strncmp(boottype, "serial", 6)) {
		bp.bp_consdev = 1;	/* com0 */
		bp.bp_conspeed = atoi(boottype + 6);
	}

	if (write(td, bootxx, 512) != 512)
		goto bad_bootxx;
	len -= 512 * 2;
	if (pwrite(td, bootxx + 512 * 2, len, 512 * 2) != len)
		goto bad_bootxx;
	ret = 0;

    bad_bootxx:
	close(td);
	close(sd);

	return ret;
}

int
md_copy_filesystem(void)
{
	return 0;
}


int
md_make_bsd_partitions(void)
{

	return make_bsd_partitions();
}

int
md_pre_update(void)
{
	if (rammb <= 8)
		set_swap(diskdev, NULL, 1);
	return 1;
}

/*
 * any additional partition validation
 */
int
md_check_partitions(void)
{
	return 1;
}


/* Upgrade support */
int
md_update(void)
{
	move_aout_libs();
	endwin();
	md_copy_filesystem();
	md_post_newfs();
	md_upgrade_mbrtype();
	wrefresh(curscr);
	wmove(stdscr, 0, 0);
	wclear(stdscr);
	wrefresh(stdscr);
	return 1;
}

void
md_upgrade_mbrtype(void)
{
	struct mbr_partition *mbrp;
	int i, netbsdpart = -1, oldbsdpart = -1, oldbsdcount = 0;

	if (read_mbr(diskdev, &mbr) < 0)
		return;

	mbrp = &mbr.mbr_parts[0];

	for (i = 0; i < NMBRPART; i++) {
		if (mbrp[i].mbrp_typ == MBR_PTYPE_386BSD) {
			oldbsdpart = i;
			oldbsdcount++;
		} else if (mbrp[i].mbrp_typ == MBR_PTYPE_NETBSD)
			netbsdpart = i;
	}

	if (netbsdpart == -1 && oldbsdcount == 1) {
		mbrp[oldbsdpart].mbrp_typ = MBR_PTYPE_NETBSD;
		write_mbr(diskdev, &mbr, 0);
	}
}



void
md_cleanup_install(void)
{
	char realfrom[STRSIZE];
	char realto[STRSIZE];
	char cmd[STRSIZE];

	strncpy(realfrom, target_expand("/etc/rc.conf"), STRSIZE);
	strncpy(realto, target_expand("/etc/rc.conf.install"), STRSIZE);
	snprintf(cmd, sizeof cmd, "sed "
			"-e 's/rc_configured=NO/rc_configured=YES/' "
			" < %s > %s", realfrom, realto);
	scripting_fprintf(logfp, "%s\n", cmd);
	do_system(cmd);

	run_prog(RUN_FATAL, NULL, "mv -f %s %s", realto, realfrom);
	
	add_rc_conf("wscons=YES\n");

#if defined(__i386__)
	/*
	 * For GENERIC_TINY, do not enable any extra screens or wsmux.
	 * Otherwise, run getty on 4 VTs.
	 */
	if (sets_selected & SET_KERNEL_TINY) {
		strlcpy(realfrom, target_expand("/etc/wscons.conf"), STRSIZE);
		strlcpy(realto, target_expand("/etc/wscons.conf.install"),
		    STRSIZE);
		snprintf(cmd, sizeof cmd, "sed"
			    " -e '/^screen/s/^/#/'"
			    " -e '/^mux/s/^/#/'"
			    " < %s > %s", realfrom, realto);
	} else
#endif
	       {
		strlcpy(realfrom, target_expand("/etc/ttys"), STRSIZE);
		strlcpy(realto, target_expand("/etc/ttys.install"), STRSIZE);
		snprintf(cmd, sizeof cmd, "sed "
				"-e '/^ttyE[1-9]/s/off/on/'"
				" < %s > %s", realfrom, realto);
	}
		
	scripting_fprintf(logfp, "%s\n", cmd);
	do_system(cmd);
	run_prog(RUN_FATAL, NULL, "mv -f %s %s", realto, realfrom);

	run_prog(0, NULL, "rm -f %s", target_expand("/sysinst"));
	run_prog(0, NULL, "rm -f %s", target_expand("/.termcap"));
	run_prog(0, NULL, "rm -f %s", target_expand("/.profile"));
}

int
md_bios_info(dev)
	char *dev;
{
	static struct disklist *disklist = NULL;
	static int mib[2] = {CTL_MACHDEP, CPU_DISKINFO};
	int i;
	size_t len;
	struct biosdisk_info *bip;
	struct nativedisk_info *nip = NULL, *nat;
	int cyl, head, sec;

	if (disklist == NULL) {
		if (sysctl(mib, 2, NULL, &len, NULL, 0) < 0)
			goto nogeom;
		disklist = malloc(len);
		if (disklist == NULL) {
			fprintf(stderr, "Out of memory\n");
			return -1;
		}
		sysctl(mib, 2, disklist, &len, NULL, 0);
	}

	nativedisk = NULL;

	for (i = 0; i < disklist->dl_nnativedisks; i++) {
		nat = &disklist->dl_nativedisks[i];
		if (!strcmp(dev, nat->ni_devname)) {
			nativedisk = nip = nat;
			break;
		}
	}
	if (nip == NULL || nip->ni_nmatches == 0) {
nogeom:
		msg_display(MSG_nobiosgeom, dlcyl, dlhead, dlsec);
		if (guess_biosgeom_from_mbr(&mbr, &cyl, &head, &sec) >= 0)
			msg_display_add(MSG_biosguess, cyl, head, sec);
		biosdisk = NULL;
	} else {
		guess_biosgeom_from_mbr(&mbr, &cyl, &head, &sec);
		if (nip->ni_nmatches == 1) {
			bip = &disklist->dl_biosdisks[nip->ni_biosmatches[0]];
			msg_display(MSG_onebiosmatch);
			msg_table_add(MSG_onebiosmatch_header);
			msg_table_add(MSG_onebiosmatch_row, bip->bi_dev,
			    bip->bi_cyl, bip->bi_head, bip->bi_sec,
			    (unsigned)bip->bi_lbasecs,
			    (unsigned)(bip->bi_lbasecs / (1000000000 / 512)));
			msg_display_add(MSG_biosgeom_advise);
			biosdisk = bip;
			process_menu(MENU_biosonematch, &biosdisk);
		} else {
			msg_display(MSG_biosmultmatch);
			msg_table_add(MSG_biosmultmatch_header);
			for (i = 0; i < nip->ni_nmatches; i++) {
				bip = &disklist->dl_biosdisks[
							nip->ni_biosmatches[i]];
				msg_table_add(MSG_biosmultmatch_row, i,
				    bip->bi_dev, bip->bi_cyl, bip->bi_head,
				    bip->bi_sec, (unsigned)bip->bi_lbasecs,
				    (unsigned)bip->bi_lbasecs/(1000000000/512));
			}
			process_menu(MENU_biosmultmatch, &i);
			if (i == -1)
				biosdisk = NULL;
			else
				biosdisk = &disklist->dl_biosdisks[
							nip->ni_biosmatches[i]];
		}
	}
	if (biosdisk == NULL)
		set_bios_geom(cyl, head, sec);
	else {
		bcyl = biosdisk->bi_cyl;
		bhead = biosdisk->bi_head;
		bsec = biosdisk->bi_sec;
	}
	if (biosdisk != NULL && (biosdisk->bi_flags & BIFLAG_EXTINT13))
		bsize = dlsize;
	else
		bsize = bcyl * bhead * bsec;
	bcylsize = bhead * bsec;
	return 0;
}

static int
count_mbr_parts(pt)
	struct mbr_partition *pt;
{
	int i, count = 0;

	for (i = 0; i < NMBRPART; i++)
		if (pt[i].mbrp_typ != 0)
			count++;

	return count;
}

static int
mbr_root_above_chs(mbr_partition_t *pt)
{

	return pt[bsdpart].mbrp_start + DEFROOTSIZE * (MEG / 512)
							>= bcyl * bhead * bsec;
}

#if defined(__i386__)
unsigned int
get_bootmodel(void)
{
	struct utsname ut;
#ifdef DEBUG
	char *envstr;

	envstr = getenv("BOOTMODEL");
	if (envstr != NULL)
		return atoi(envstr);
#endif

	if (uname(&ut) < 0)
		ut.version[0] = 0;

	if (strstr(ut.version, "TINY") != NULL)
		return SET_KERNEL_TINY;
	if (strstr(ut.version, "LAPTOP") != NULL)
		return SET_KERNEL_LAPTOP;
	if (strstr(ut.version, "PS2") != NULL)
		return SET_KERNEL_PS2;
	return SET_KERNEL_GENERIC;
}
#endif

void
md_init(void)
{

	/* Default to install same type of kernel as we are running */
	sets_selected = (sets_selected & ~SET_KERNEL) | get_bootmodel();
}

void
md_set_sizemultname(void)
{

	set_sizemultname_meg();
}
