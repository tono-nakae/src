/*      $NetBSD: edquota.c,v 1.39 2011/11/25 16:55:05 dholland Exp $ */
/*
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#ifndef lint
__COPYRIGHT("@(#) Copyright (c) 1980, 1990, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)edquota.c	8.3 (Berkeley) 4/27/95";
#else
__RCSID("$NetBSD: edquota.c,v 1.39 2011/11/25 16:55:05 dholland Exp $");
#endif
#endif /* not lint */

/*
 * Disk quota editor.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/statvfs.h>

#include <quota/quotaprop.h>
#include <quota/quota.h>
#include <ufs/ufs/quota1.h>
#include <sys/quota.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "printquota.h"
#include "getvfsquota.h"
#include "quotautil.h"

#include "pathnames.h"

static const char *quotagroup = QUOTAGROUP;

#define MAX_TMPSTR	(100+MAXPATHLEN)

/* flags for quotause */
#define	FOUND	0x01
#define	QUOTA2	0x02
#define	DEFAULT	0x04

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	quotaval qv[QUOTA_NLIMITS];
	char	fsname[MAXPATHLEN + 1];
	char	*qfname;
};

struct quotalist {
	struct quotause *head;
	struct quotause *tail;
};

static void	usage(void) __dead;

static int Hflag = 0;
static int Dflag = 0;

/* more compact form of constants */
#define QL_BLK QUOTA_LIMIT_BLOCK
#define QL_FL  QUOTA_LIMIT_FILE

////////////////////////////////////////////////////////////
// support code

/*
 * This routine converts a name for a particular quota class to
 * an identifier. This routine must agree with the kernel routine
 * getinoquota as to the interpretation of quota classes.
 */
static int
getidbyname(const char *name, int quotaclass)
{
	struct passwd *pw;
	struct group *gr;

	if (alldigits(name))
		return atoi(name);
	switch(quotaclass) {
	case QUOTA_CLASS_USER:
		if ((pw = getpwnam(name)) != NULL)
			return pw->pw_uid;
		warnx("%s: no such user", name);
		break;
	case QUOTA_CLASS_GROUP:
		if ((gr = getgrnam(name)) != NULL)
			return gr->gr_gid;
		warnx("%s: no such group", name);
		break;
	default:
		warnx("%d: unknown quota type", quotaclass);
		break;
	}
	sleep(1);
	return -1;
}

////////////////////////////////////////////////////////////
// quotause operations

/*
 * Create an empty quotause structure.
 */
static struct quotause *
quotause_create(void)
{
	struct quotause *qup;

	qup = malloc(sizeof(*qup));
	if (qup == NULL) {
		err(1, "malloc");
	}

	qup->next = NULL;
	qup->flags = 0;
	memset(qup->qv, 0, sizeof(qup->qv));
	qup->fsname[0] = '\0';
	qup->qfname = NULL;

	return qup;
}

/*
 * Free a quotause structure.
 */
static void
quotause_destroy(struct quotause *qup)
{
	free(qup->qfname);
	free(qup);
}

////////////////////////////////////////////////////////////
// quotalist operations

/*
 * Create a quotause list.
 */
static struct quotalist *
quotalist_create(void)
{
	struct quotalist *qlist;

	qlist = malloc(sizeof(*qlist));
	if (qlist == NULL) {
		err(1, "malloc");
	}

	qlist->head = NULL;
	qlist->tail = NULL;

	return qlist;
}

/*
 * Free a list of quotause structures.
 */
static void
quotalist_destroy(struct quotalist *qlist)
{
	struct quotause *qup, *nextqup;

	for (qup = qlist->head; qup; qup = nextqup) {
		nextqup = qup->next;
		quotause_destroy(qup);
	}
	free(qlist);
}

static bool
quotalist_empty(struct quotalist *qlist)
{
	return qlist->head == NULL;
}

static void
quotalist_append(struct quotalist *qlist, struct quotause *qup)
{
	/* should not already be on a list */
	assert(qup->next == NULL);

	if (qlist->head == NULL) {
		qlist->head = qup;
	} else {
		qlist->tail->next = qup;
	}
	qlist->tail = qup;
}

////////////////////////////////////////////////////////////
// ffs quota v1

static void
putprivs1(uint32_t id, int quotaclass, struct quotause *qup)
{
	struct dqblk dqblk;
	int fd;

	quotaval_to_dqblk(qup->qv, &dqblk);
	assert((qup->flags & DEFAULT) == 0);

	if ((fd = open(qup->qfname, O_WRONLY)) < 0) {
		warnx("open `%s'", qup->qfname);
	} else {
		(void)lseek(fd,
		    (off_t)(id * (long)sizeof (struct dqblk)),
		    SEEK_SET);
		if (write(fd, &dqblk, sizeof (struct dqblk)) !=
		    sizeof (struct dqblk))
			warnx("writing `%s'", qup->qfname);
		close(fd);
	}
}

static struct quotause *
getprivs1(long id, int quotaclass, const char *filesys)
{
	struct fstab *fs;
	char qfpathname[MAXPATHLEN];
	struct quotause *qup;
	struct dqblk dqblk;
	int fd;

	setfsent();
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_vfstype, "ffs"))
			continue;
		if (strcmp(fs->fs_spec, filesys) == 0 ||
		    strcmp(fs->fs_file, filesys) == 0)
			break;
	}
	if (fs == NULL)
		return NULL;

	if (!hasquota(qfpathname, sizeof(qfpathname), fs,
	    ufsclass2qtype(quotaclass)))
		return NULL;

	qup = quotause_create();
	strcpy(qup->fsname, fs->fs_file);
	if ((fd = open(qfpathname, O_RDONLY)) < 0) {
		fd = open(qfpathname, O_RDWR|O_CREAT, 0640);
		if (fd < 0 && errno != ENOENT) {
			warnx("open `%s'", qfpathname);
			quotause_destroy(qup);
			return NULL;
		}
		warnx("Creating quota file %s", qfpathname);
		sleep(3);
		(void)fchown(fd, getuid(),
		    getidbyname(quotagroup, QUOTA_CLASS_GROUP));
		(void)fchmod(fd, 0640);
	}
	(void)lseek(fd, (off_t)(id * sizeof(struct dqblk)),
	    SEEK_SET);
	switch (read(fd, &dqblk, sizeof(struct dqblk))) {
	case 0:			/* EOF */
		/*
		 * Convert implicit 0 quota (EOF)
		 * into an explicit one (zero'ed dqblk)
		 */
		memset(&dqblk, 0, sizeof(struct dqblk));
		break;

	case sizeof(struct dqblk):	/* OK */
		break;

	default:		/* ERROR */
		warn("read error in `%s'", qfpathname);
		close(fd);
		quotause_destroy(qup);
		return NULL;
	}
	close(fd);
	qup->qfname = qfpathname;
	endfsent();
	dqblk_to_quotaval(&dqblk, qup->qv);
	return qup;
}

////////////////////////////////////////////////////////////
// ffs quota v2

static struct quotause *
getprivs2(long id, int quotaclass, const char *filesys, int defaultq)
{
	struct quotause *qup;
	int8_t version;

	qup = quotause_create();
	strcpy(qup->fsname, filesys);
	if (defaultq)
		qup->flags |= DEFAULT;
	if (!getvfsquota(filesys, qup->qv, &version,
	    id, quotaclass, defaultq, Dflag)) {
		/* no entry, get default entry */
		if (!getvfsquota(filesys, qup->qv, &version,
		    id, quotaclass, 1, Dflag)) {
			free(qup);
			return NULL;
		}
	}
	if (version == 2)
		qup->flags |= QUOTA2;
	return qup;
}

static void
putprivs2(uint32_t id, int quotaclass, struct quotause *qup)
{

	prop_dictionary_t dict, data, cmd;
	prop_array_t cmds, datas;
	struct plistref pref;
	int8_t error8;
	uint64_t *valuesp[QUOTA_NLIMITS];

	valuesp[QL_BLK] =
	    &qup->qv[QL_BLK].qv_hardlimit;
	valuesp[QL_FL] =
	    &qup->qv[QL_FL].qv_hardlimit;

	data = quota64toprop(id, (qup->flags & DEFAULT) ? 1 : 0,
	    valuesp, ufs_quota_entry_names, UFS_QUOTA_NENTRIES,
	    ufs_quota_limit_names, QUOTA_NLIMITS);

	if (data == NULL)
		err(1, "quota64toprop(id)");

	dict = quota_prop_create();
	cmds = prop_array_create();
	datas = prop_array_create();

	if (dict == NULL || cmds == NULL || datas == NULL) {
		errx(1, "can't allocate proplist");
	}

	if (!prop_array_add_and_rel(datas, data))
		err(1, "prop_array_add(data)");
	
	if (!quota_prop_add_command(cmds, "set",
	    ufs_quota_class_names[quotaclass], datas))
		err(1, "prop_add_command");
	if (!prop_dictionary_set(dict, "commands", cmds))
		err(1, "prop_dictionary_set(command)");
	if (Dflag)
		printf("message to kernel:\n%s\n",
		    prop_dictionary_externalize(dict));

	if (prop_dictionary_send_syscall(dict, &pref) != 0)
		err(1, "prop_dictionary_send_syscall");
	prop_object_release(dict);

	if (quotactl(qup->fsname, &pref) != 0)
		err(1, "quotactl");

	if (prop_dictionary_recv_syscall(&pref, &dict) != 0) {
		err(1, "prop_dictionary_recv_syscall");
	}

	if (Dflag)
		printf("reply from kernel:\n%s\n",
		    prop_dictionary_externalize(dict));

	if ((errno = quota_get_cmds(dict, &cmds)) != 0) {
		err(1, "quota_get_cmds");
	}
	/* only one command, no need to iter */
	cmd = prop_array_get(cmds, 0);
	if (cmd == NULL)
		err(1, "prop_array_get(cmd)");

	if (!prop_dictionary_get_int8(cmd, "return", &error8))
		err(1, "prop_get(return)");

	if (error8) {
		errno = error8;
		if (qup->flags & DEFAULT)
			warn("set default %s quota",
			    ufs_quota_class_names[quotaclass]);
		else
			warn("set %s quota for %u",
			    ufs_quota_class_names[quotaclass], id);
	}
	prop_object_release(dict);
}

////////////////////////////////////////////////////////////
// quota format switch

/*
 * Collect the requested quota information.
 */
static struct quotalist *
getprivs(long id, int defaultq, int quotaclass, const char *filesys)
{
	struct statvfs *fst;
	int nfst, i;
	struct quotalist *qlist;
	struct quotause *qup;

	qlist = quotalist_create();

	nfst = getmntinfo(&fst, MNT_WAIT);
	if (nfst == 0)
		errx(1, "no filesystems mounted!");

	for (i = 0; i < nfst; i++) {
		if ((fst[i].f_flag & ST_QUOTA) == 0)
			continue;
		if (filesys &&
		    strcmp(fst[i].f_mntonname, filesys) != 0 &&
		    strcmp(fst[i].f_mntfromname, filesys) != 0)
			continue;
		qup = getprivs2(id, quotaclass, fst[i].f_mntonname, defaultq);
		if (qup == NULL) {
			/*
			 * XXX: returning NULL is totally wrong. On
			 * serious error, abort; on minor error, warn
			 * and continue.
			 *
			 * Note: we cannot warn unconditionally here
			 * because this case apparently includes "no
			 * quota entry on this volume" and that causes
			 * the atf tests to fail. Bletch.
			 */
			/*return NULL;*/
			/*warnx("getprivs2 failed");*/
			continue;
		}

		quotalist_append(qlist, qup);
	}

	if (filesys && quotalist_empty(qlist)) {
		if (defaultq)
			errx(1, "no default quota for version 1");
		/* if we get there, filesys is not mounted. try the old way */
		qup = getprivs1(id, quotaclass, filesys);
		if (qup == NULL) {
			/* XXX. see above */
			/*return NULL;*/
			/*warnx("getprivs1 failed");*/
			return qlist;
		}
		quotalist_append(qlist, qup);
	}
	return qlist;
}

/*
 * Store the requested quota information.
 */
static void
putprivs(uint32_t id, int quotaclass, struct quotalist *qlist)
{
	struct quotause *qup;

        for (qup = qlist->head; qup; qup = qup->next) {
		if (qup->qfname == NULL)
			putprivs2(id, quotaclass, qup);
		else
			putprivs1(id, quotaclass, qup);
	}
}

static void
clearpriv(int argc, char **argv, const char *filesys, int quotaclass)
{
	prop_array_t cmds, datas;
	prop_dictionary_t protodict, dict, data, cmd;
	struct plistref pref;
	bool ret;
	struct statvfs *fst;
	int nfst, i;
	int8_t error8;
	int id;

	/* build a generic command */
	protodict = quota_prop_create();
	cmds = prop_array_create();
	datas = prop_array_create();
	if (protodict == NULL || cmds == NULL || datas == NULL) {
		errx(1, "can't allocate proplist");
	}

	for ( ; argc > 0; argc--, argv++) {
		if ((id = getidbyname(*argv, quotaclass)) == -1)
			continue;
		data = prop_dictionary_create();
		if (data == NULL)
			errx(1, "can't allocate proplist");

		ret = prop_dictionary_set_uint32(data, "id", id);
		if (!ret)
			err(1, "prop_dictionary_set(id)");
		if (!prop_array_add_and_rel(datas, data))
			err(1, "prop_array_add(data)");
	}
	if (!quota_prop_add_command(cmds, "clear",
	    ufs_quota_class_names[quotaclass], datas))
		err(1, "prop_add_command");

	if (!prop_dictionary_set(protodict, "commands", cmds))
		err(1, "prop_dictionary_set(command)");

	/* now loop over quota-enabled filesystems */
	nfst = getmntinfo(&fst, MNT_WAIT);
	if (nfst == 0)
		errx(1, "no filesystems mounted!");

	for (i = 0; i < nfst; i++) {
		if ((fst[i].f_flag & ST_QUOTA) == 0)
			continue;
		if (filesys && strcmp(fst[i].f_mntonname, filesys) != 0 &&
		    strcmp(fst[i].f_mntfromname, filesys) != 0)
			continue;
		if (Dflag) {
			fprintf(stderr, "message to kernel for %s:\n%s\n",
			    fst[i].f_mntonname,
			    prop_dictionary_externalize(protodict));
		}

		if (prop_dictionary_send_syscall(protodict, &pref) != 0)
			err(1, "prop_dictionary_send_syscall");
		if (quotactl(fst[i].f_mntonname, &pref) != 0)
			err(1, "quotactl");

		if (prop_dictionary_recv_syscall(&pref, &dict) != 0) {
			err(1, "prop_dictionary_recv_syscall");
		}

		if (Dflag) {
			fprintf(stderr, "reply from kernel for %s:\n%s\n",
			    fst[i].f_mntonname,
			    prop_dictionary_externalize(dict));
		}
		if ((errno = quota_get_cmds(dict, &cmds)) != 0) {
			err(1, "quota_get_cmds");
		}
		/* only one command, no need to iter */
		cmd = prop_array_get(cmds, 0);
		if (cmd == NULL)
			err(1, "prop_array_get(cmd)");

		if (!prop_dictionary_get_int8(cmd, "return", &error8))
			err(1, "prop_get(return)");
		if (error8) {
			errno = error8;
			warn("clear %s quota entries on %s",
			    ufs_quota_class_names[quotaclass],
			    fst[i].f_mntonname);
		}
		prop_object_release(dict);
	}
	prop_object_release(protodict);
}

////////////////////////////////////////////////////////////
// editor

/*
 * Take a list of privileges and get it edited.
 */
static int
editit(const char *ltmpfile)
{
	pid_t pid;
	int lst;
	char p[MAX_TMPSTR];
	const char *ed;
	sigset_t s, os;

	sigemptyset(&s);
	sigaddset(&s, SIGINT);
	sigaddset(&s, SIGQUIT);
	sigaddset(&s, SIGHUP);
	if (sigprocmask(SIG_BLOCK, &s, &os) == -1)
		err(1, "sigprocmask");
top:
	switch ((pid = fork())) {
	case -1:
		if (errno == EPROCLIM) {
			warnx("You have too many processes");
			return 0;
		}
		if (errno == EAGAIN) {
			sleep(1);
			goto top;
		}
		warn("fork");
		return 0;
	case 0:
		if (sigprocmask(SIG_SETMASK, &os, NULL) == -1)
			err(1, "sigprocmask");
		setgid(getgid());
		setuid(getuid());
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = _PATH_VI;
		if (strlen(ed) + strlen(ltmpfile) + 2 >= MAX_TMPSTR) {
			errx(1, "%s", "editor or filename too long");
		}
		snprintf(p, sizeof(p), "%s %s", ed, ltmpfile);
		execlp(_PATH_BSHELL, _PATH_BSHELL, "-c", p, NULL);
		err(1, "%s", ed);
	default:
		if (waitpid(pid, &lst, 0) == -1)
			err(1, "waitpid");
		if (sigprocmask(SIG_SETMASK, &os, NULL) == -1)
			err(1, "sigprocmask");
		if (!WIFEXITED(lst) || WEXITSTATUS(lst) != 0)
			return 0;
		return 1;
	}
}

/*
 * Convert a quotause list to an ASCII file.
 */
static int
writeprivs(struct quotalist *qlist, int outfd, const char *name,
    int quotaclass)
{
	struct quotause *qup;
	FILE *fd;
	char b0[32], b1[32], b2[32], b3[32];

	(void)ftruncate(outfd, 0);
	(void)lseek(outfd, (off_t)0, SEEK_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL)
		errx(1, "fdopen");
	if (name == NULL) {
		fprintf(fd, "Default %s quotas:\n",
		    ufs_quota_class_names[quotaclass]);
	} else {
		fprintf(fd, "Quotas for %s %s:\n",
		    ufs_quota_class_names[quotaclass], name);
	}
	for (qup = qlist->head; qup; qup = qup->next) {
		struct quotaval *q = qup->qv;
		fprintf(fd, "%s (version %d):\n",
		     qup->fsname, (qup->flags & QUOTA2) ? 2 : 1);
		if ((qup->flags & DEFAULT) == 0 || (qup->flags & QUOTA2) != 0) {
			fprintf(fd, "\tblocks in use: %s, "
			    "limits (soft = %s, hard = %s",
			    intprt(b1, 21, q[QL_BLK].qv_usage,
			    HN_NOSPACE | HN_B, Hflag), 
			    intprt(b2, 21, q[QL_BLK].qv_softlimit,
			    HN_NOSPACE | HN_B, Hflag),
			    intprt(b3, 21, q[QL_BLK].qv_hardlimit,
				HN_NOSPACE | HN_B, Hflag));
			if (qup->flags & QUOTA2)
				fprintf(fd, ", ");
		} else
			fprintf(fd, "\tblocks: (");
			
		if (qup->flags & (QUOTA2|DEFAULT)) {
		    fprintf(fd, "grace = %s",
			timepprt(b0, 21, q[QL_BLK].qv_grace, Hflag));
		}
		fprintf(fd, ")\n");
		if ((qup->flags & DEFAULT) == 0 || (qup->flags & QUOTA2) != 0) {
			fprintf(fd, "\tinodes in use: %s, "
			    "limits (soft = %s, hard = %s",
			    intprt(b1, 21, q[QL_FL].qv_usage,
			    HN_NOSPACE, Hflag),
			    intprt(b2, 21, q[QL_FL].qv_softlimit,
			    HN_NOSPACE, Hflag),
			    intprt(b3, 21, q[QL_FL].qv_hardlimit,
			     HN_NOSPACE, Hflag));
			if (qup->flags & QUOTA2)
				fprintf(fd, ", ");
		} else
			fprintf(fd, "\tinodes: (");

		if (qup->flags & (QUOTA2|DEFAULT)) {
		    fprintf(fd, "grace = %s",
			timepprt(b0, 21, q[QL_FL].qv_grace, Hflag));
		}
		fprintf(fd, ")\n");
	}
	fclose(fd);
	return 1;
}

/*
 * Merge changes to an ASCII file into a quotause list.
 */
static int
readprivs(struct quotalist *qlist, int infd, int dflag)
{
	struct quotause *qup;
	FILE *fd;
	int cnt;
	char fsp[BUFSIZ];
	static char line0[BUFSIZ], line1[BUFSIZ], line2[BUFSIZ];
	static char scurb[BUFSIZ], scuri[BUFSIZ], ssoft[BUFSIZ], shard[BUFSIZ];
	static char stime[BUFSIZ];
	uint64_t softb, hardb, softi, hardi;
	time_t graceb = -1, gracei = -1;
	int version;

	(void)lseek(infd, (off_t)0, SEEK_SET);
	fd = fdopen(dup(infd), "r");
	if (fd == NULL) {
		warn("Can't re-read temp file");
		return 0;
	}
	/*
	 * Discard title line, then read pairs of lines to process.
	 */
	(void) fgets(line1, sizeof (line1), fd);
	while (fgets(line0, sizeof (line0), fd) != NULL &&
	       fgets(line1, sizeof (line2), fd) != NULL &&
	       fgets(line2, sizeof (line2), fd) != NULL) {
		if (sscanf(line0, "%s (version %d):\n", fsp, &version) != 2) {
			warnx("%s: bad format", line0);
			goto out;
		}
#define last_char(str) ((str)[strlen(str) - 1])
		if (last_char(line1) != '\n') {
			warnx("%s:%s: bad format", fsp, line1);
			goto out;
		}
		last_char(line1) = '\0';
		if (last_char(line2) != '\n') {
			warnx("%s:%s: bad format", fsp, line2);
			goto out;
		}
		last_char(line2) = '\0';
		if (dflag && version == 1) {
			if (sscanf(line1,
			    "\tblocks: (grace = %s\n", stime) != 1) {
				warnx("%s:%s: bad format", fsp, line1);
				goto out;
			}
			if (last_char(stime) != ')') {
				warnx("%s:%s: bad format", fsp, line1);
				goto out;
			}
			last_char(stime) = '\0';
			if (timeprd(stime, &graceb) != 0) {
				warnx("%s:%s: bad number", fsp, stime);
				goto out;
			}
			if (sscanf(line2,
			    "\tinodes: (grace = %s\n", stime) != 1) {
				warnx("%s:%s: bad format", fsp, line2);
				goto out;
			}
			if (last_char(stime) != ')') {
				warnx("%s:%s: bad format", fsp, line2);
				goto out;
			}
			last_char(stime) = '\0';
			if (timeprd(stime, &gracei) != 0) {
				warnx("%s:%s: bad number", fsp, stime);
				goto out;
			}
		} else {
			cnt = sscanf(line1,
			    "\tblocks in use: %s limits (soft = %s hard = %s "
			    "grace = %s", scurb, ssoft, shard, stime);
			if (cnt == 3) {
				if (version != 1 ||
				    last_char(scurb) != ',' ||
				    last_char(ssoft) != ',' ||
				    last_char(shard) != ')') {
					warnx("%s:%s: bad format %d",
					    fsp, line1, cnt);
					goto out;
				}
				stime[0] = '\0';
			} else if (cnt == 4) {
				if (version < 2 ||
				    last_char(scurb) != ',' ||
				    last_char(ssoft) != ',' ||
				    last_char(shard) != ',' ||
				    last_char(stime) != ')') {
					warnx("%s:%s: bad format %d",
					    fsp, line1, cnt);
					goto out;
				}
			} else {
				warnx("%s: %s: bad format cnt %d", fsp, line1,
				    cnt);
				goto out;
			}
			/* drop last char which is ',' or ')' */
			last_char(scurb) = '\0';
			last_char(ssoft) = '\0';
			last_char(shard) = '\0';
			last_char(stime) = '\0';
			
			if (intrd(ssoft, &softb, HN_B) != 0) {
				warnx("%s:%s: bad number", fsp, ssoft);
				goto out;
			}
			if (intrd(shard, &hardb, HN_B) != 0) {
				warnx("%s:%s: bad number", fsp, shard);
				goto out;
			}
			if (cnt == 4) {
				if (timeprd(stime, &graceb) != 0) {
					warnx("%s:%s: bad number", fsp, stime);
					goto out;
				}
			}

			cnt = sscanf(line2,
			    "\tinodes in use: %s limits (soft = %s hard = %s "
			    "grace = %s", scuri, ssoft, shard, stime);
			if (cnt == 3) {
				if (version != 1 ||
				    last_char(scuri) != ',' ||
				    last_char(ssoft) != ',' ||
				    last_char(shard) != ')') {
					warnx("%s:%s: bad format %d",
					    fsp, line2, cnt);
					goto out;
				}
				stime[0] = '\0';
			} else if (cnt == 4) {
				if (version < 2 ||
				    last_char(scuri) != ',' ||
				    last_char(ssoft) != ',' ||
				    last_char(shard) != ',' ||
				    last_char(stime) != ')') {
					warnx("%s:%s: bad format %d",
					    fsp, line2, cnt);
					goto out;
				}
			} else {
				warnx("%s: %s: bad format", fsp, line2);
				goto out;
			}
			/* drop last char which is ',' or ')' */
			last_char(scuri) = '\0';
			last_char(ssoft) = '\0';
			last_char(shard) = '\0';
			last_char(stime) = '\0';
			if (intrd(ssoft, &softi, 0) != 0) {
				warnx("%s:%s: bad number", fsp, ssoft);
				goto out;
			}
			if (intrd(shard, &hardi, 0) != 0) {
				warnx("%s:%s: bad number", fsp, shard);
				goto out;
			}
			if (cnt == 4) {
				if (timeprd(stime, &gracei) != 0) {
					warnx("%s:%s: bad number", fsp, stime);
					goto out;
				}
			}
		}
		for (qup = qlist->head; qup; qup = qup->next) {
			struct quotaval *q = qup->qv;
			char b1[32], b2[32];
			if (strcmp(fsp, qup->fsname))
				continue;
			if (version == 1 && dflag) {
				q[QL_BLK].qv_grace = graceb;
				q[QL_FL].qv_grace = gracei;
				qup->flags |= FOUND;
				continue;
			}

			if (strcmp(intprt(b1, 21, q[QL_BLK].qv_usage,
			    HN_NOSPACE | HN_B, Hflag),
			    scurb) != 0 ||
			    strcmp(intprt(b2, 21, q[QL_FL].qv_usage,
			    HN_NOSPACE, Hflag),
			    scuri) != 0) {
				warnx("%s: cannot change current allocation",
				    fsp);
				break;
			}
			/*
			 * Cause time limit to be reset when the quota
			 * is next used if previously had no soft limit
			 * or were under it, but now have a soft limit
			 * and are over it.
			 */
			if (q[QL_BLK].qv_usage &&
			    q[QL_BLK].qv_usage >= softb &&
			    (q[QL_BLK].qv_softlimit == 0 ||
			     q[QL_BLK].qv_usage < q[QL_BLK].qv_softlimit))
				q[QL_BLK].qv_expiretime = 0;
			if (q[QL_FL].qv_usage &&
			    q[QL_FL].qv_usage >= softi &&
			    (q[QL_FL].qv_softlimit == 0 ||
			     q[QL_FL].qv_usage < q[QL_FL].qv_softlimit))
				q[QL_FL].qv_expiretime = 0;
			q[QL_BLK].qv_softlimit = softb;
			q[QL_BLK].qv_hardlimit = hardb;
			if (version == 2)
				q[QL_BLK].qv_grace = graceb;
			q[QL_FL].qv_softlimit  = softi;
			q[QL_FL].qv_hardlimit  = hardi;
			if (version == 2)
				q[QL_FL].qv_grace = gracei;
			qup->flags |= FOUND;
		}
	}
out:
	fclose(fd);
	/*
	 * Disable quotas for any filesystems that have not been found.
	 */
	for (qup = qlist->head; qup; qup = qup->next) {
		struct quotaval *q = qup->qv;
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		q[QL_BLK].qv_softlimit = UQUAD_MAX;
		q[QL_BLK].qv_hardlimit = UQUAD_MAX;
		q[QL_BLK].qv_grace = 0;
		q[QL_FL].qv_softlimit = UQUAD_MAX;
		q[QL_FL].qv_hardlimit = UQUAD_MAX;
		q[QL_FL].qv_grace = 0;
	}
	return 1;
}

////////////////////////////////////////////////////////////
// actions

static void
replicate(const char *fs, int quotaclass, const char *protoname,
	  char **names, int numnames)
{
	long protoid, id;
	struct quotalist *protoprivs;
	struct quotause *qup;
	int i;

	if ((protoid = getidbyname(protoname, quotaclass)) == -1)
		exit(1);
	protoprivs = getprivs(protoid, 0, quotaclass, fs);
	for (qup = protoprivs->head; qup; qup = qup->next) {
		qup->qv[QL_BLK].qv_expiretime = 0;
		qup->qv[QL_FL].qv_expiretime = 0;
	}
	for (i=0; i<numnames; i++) {
		id = getidbyname(names[i], quotaclass);
		if (id < 0)
			continue;
		putprivs(id, quotaclass, protoprivs);
	}
	/* XXX */
	/* quotalist_destroy(protoprivs); */
}

static void
assign(const char *fs, int quotaclass,
       char *soft, char *hard, char *grace,
       char **names, int numnames)
{
	struct quotalist *curprivs;
	struct quotause *lqup;
	u_int64_t softb, hardb, softi, hardi;
	time_t  graceb, gracei;
	char *str;
	long id;
	int dflag;
	int i;

	if (soft) {
		str = strsep(&soft, "/");
		if (str[0] == '\0' || soft == NULL || soft[0] == '\0')
			usage();

		if (intrd(str, &softb, HN_B) != 0)
			errx(1, "%s: bad number", str);
		if (intrd(soft, &softi, 0) != 0)
			errx(1, "%s: bad number", soft);
	}
	if (hard) {
		str = strsep(&hard, "/");
		if (str[0] == '\0' || hard == NULL || hard[0] == '\0')
			usage();

		if (intrd(str, &hardb, HN_B) != 0)
			errx(1, "%s: bad number", str);
		if (intrd(hard, &hardi, 0) != 0)
			errx(1, "%s: bad number", hard);
	}
	if (grace) {
		str = strsep(&grace, "/");
		if (str[0] == '\0' || grace == NULL || grace[0] == '\0')
			usage();

		if (timeprd(str, &graceb) != 0)
			errx(1, "%s: bad number", str);
		if (timeprd(grace, &gracei) != 0)
			errx(1, "%s: bad number", grace);
	}
	for (i=0; i<numnames; i++) {
		if (names[i] == NULL) {
			id = 0;
			dflag = 1;
		} else {
			id = getidbyname(names[i], quotaclass);
			if (id == -1)
				continue;
			dflag = 0;
		}

		curprivs = getprivs(id, dflag, quotaclass, fs);
		for (lqup = curprivs->head; lqup; lqup = lqup->next) {
			struct quotaval *q = lqup->qv;
			if (soft) {
				if (!dflag && softb &&
				    q[QL_BLK].qv_usage >= softb &&
				    (q[QL_BLK].qv_softlimit == 0 ||
				     q[QL_BLK].qv_usage <
				     q[QL_BLK].qv_softlimit))
					q[QL_BLK].qv_expiretime = 0;
				if (!dflag && softi &&
				    q[QL_FL].qv_usage >= softb &&
				    (q[QL_FL].qv_softlimit == 0 ||
				     q[QL_FL].qv_usage <
				     q[QL_FL].qv_softlimit))
					q[QL_FL].qv_expiretime = 0;
				q[QL_BLK].qv_softlimit = softb;
				q[QL_FL].qv_softlimit = softi;
			}
			if (hard) {
				q[QL_BLK].qv_hardlimit = hardb;
				q[QL_FL].qv_hardlimit = hardi;
			}
			if (grace) {
				q[QL_BLK].qv_grace = graceb;
				q[QL_FL].qv_grace = gracei;
			}
		}
		putprivs(id, quotaclass, curprivs);
		quotalist_destroy(curprivs);
	}
}

static void
clear(const char *fs, int quotaclass, char **names, int numnames)
{
	clearpriv(numnames, names, fs, quotaclass);
}

static void
editone(const char *fs, int quotaclass, const char *name,
	int tmpfd, const char *tmppath)
{
	struct quotalist *curprivs;
	long id;
	int dflag;

	if (name == NULL) {
		id = 0;
		dflag = 1;
	} else {
		id = getidbyname(name, quotaclass);
		if (id == -1)
			return;
		dflag = 0;
	}
	curprivs = getprivs(id, dflag, quotaclass, fs);

	if (writeprivs(curprivs, tmpfd, name, quotaclass) == 0)
		goto fail;

	if (editit(tmppath) == 0)
		goto fail;

	if (readprivs(curprivs, tmpfd, dflag) == 0)
		goto fail;

	putprivs(id, quotaclass, curprivs);
fail:
	quotalist_destroy(curprivs);
}

static void
edit(const char *fs, int quotaclass, char **names, int numnames)
{
	char tmppath[] = _PATH_TMPFILE;
	int tmpfd, i;

	tmpfd = mkstemp(tmppath);
	fchown(tmpfd, getuid(), getgid());

	for (i=0; i<numnames; i++) {
		editone(fs, quotaclass, names[i], tmpfd, tmppath);
	}

	close(tmpfd);
	unlink(tmppath);
}

////////////////////////////////////////////////////////////
// main

static void
usage(void)
{
	const char *p = getprogname();
	fprintf(stderr,
	    "Usage: %s [-D] [-H] [-u] [-p <username>] [-f <filesystem>] "
		"-d | <username> ...\n"
	    "\t%s [-D] [-H] -g [-p <groupname>] [-f <filesystem>] "
		"-d | <groupname> ...\n"
	    "\t%s [-D] [-u] [-f <filesystem>] [-s b#/i#] [-h b#/i#] [-t t#/t#] "
		"-d | <username> ...\n"
	    "\t%s [-D] -g [-f <filesystem>] [-s b#/i#] [-h b#/i#] [-t t#/t#] "
		"-d | <groupname> ...\n"
	    "\t%s [-D] [-H] [-u] -c [-f <filesystem>] username ...\n"
	    "\t%s [-D] [-H] -g -c [-f <filesystem>] groupname ...\n",
	    p, p, p, p, p, p);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int quotaclass;
	char *protoname;
	char *soft = NULL, *hard = NULL, *grace = NULL;
	char *fs = NULL;
	int ch;
	int pflag = 0;
	int cflag = 0;
	int dflag = 0;

	if (argc < 2)
		usage();
	if (getuid())
		errx(1, "permission denied");
	protoname = NULL;
	quotaclass = QUOTA_CLASS_USER;
	while ((ch = getopt(argc, argv, "DHcdugp:s:h:t:f:")) != -1) {
		switch(ch) {
		case 'D':
			Dflag++;
			break;
		case 'H':
			Hflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 'd':
			dflag++;
			break;
		case 'p':
			protoname = optarg;
			pflag++;
			break;
		case 'g':
			quotaclass = QUOTA_CLASS_GROUP;
			break;
		case 'u':
			quotaclass = QUOTA_CLASS_USER;
			break;
		case 's':
			soft = optarg;
			break;
		case 'h':
			hard = optarg;
			break;
		case 't':
			grace = optarg;
			break;
		case 'f':
			fs = optarg;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (pflag) {
		if (soft || hard || grace || dflag || cflag)
			usage();
		replicate(fs, quotaclass, protoname, argv, argc);
	} else if (soft || hard || grace) {
		if (cflag)
			usage();
		if (dflag) {
			/* use argv[argc], which is null, to mean 'default' */
			argc++;
		}
		assign(fs, quotaclass, soft, hard, grace, argv, argc);
	} else if (cflag) {
		if (dflag)
			usage();
		clear(fs, quotaclass, argv, argc);
	} else {
		if (dflag) {
			/* use argv[argc], which is null, to mean 'default' */
			argc++;
		}
		edit(fs, quotaclass, argv, argc);
	}
	return 0;
}
