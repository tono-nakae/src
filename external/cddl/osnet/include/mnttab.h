/* $FreeBSD: src/cddl/compat/opensolaris/include/mnttab.h,v 1.4.2.1 2009/08/03 08:13:06 kensmith Exp $ */

#ifndef	_OPENSOLARIS_MNTTAB_H_
#define	_OPENSOLARIS_MNTTAB_H_

#include <stdio.h>
#include <paths.h>

#define	MNTTAB		_PATH_DEVNULL
#define	MNT_LINE_MAX	1024

#define	umount2(p, f)	unmount(p, f)

struct mnttab {
	char	*mnt_special;
	char	*mnt_mountp;
	char	*mnt_fstype;
	char	*mnt_mntopts;
};

int getmntany(FILE *fd, struct mnttab *mgetp, struct mnttab *mrefp);

#endif	/* !_OPENSOLARIS_MNTTAB_H_ */
