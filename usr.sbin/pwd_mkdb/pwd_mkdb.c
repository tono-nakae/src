/*	$NetBSD: pwd_mkdb.c,v 1.43 2009/06/18 22:13:56 christos Exp $	*/

/*
 * Copyright (c) 2000, 2009 The NetBSD Foundation, Inc.
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

/*
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

/*
 * Portions Copyright(C) 1994, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 2000, 2009\
 The NetBSD Foundation, Inc.  All rights reserved.\
  Copyright (c) 1991, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
__SCCSID("from: @(#)pwd_mkdb.c	8.5 (Berkeley) 4/20/94");
__RCSID("$NetBSD: pwd_mkdb.c,v 1.43 2009/06/18 22:13:56 christos Exp $");
#endif /* not lint */

#if HAVE_NBTOOL_CONFIG_H
#include "compat_pwd.h"
#else
#include <pwd.h>
#endif

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef HAVE_NBTOOL_CONFIG_H
#include <machine/bswap.h>
#endif

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#define	MAX_CACHESIZE	8*1024*1024
#define	MIN_CACHESIZE	2*1024*1024

#define	PERM_INSECURE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define	PERM_SECURE	(S_IRUSR | S_IWUSR)

#if HAVE_NBTOOL_CONFIG_H
static const char __yp_token[] = "__YP!";
#else
/* Pull this out of the C library. */
extern const char __yp_token[];
#endif

static HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	0,		/* cachesize */
	NULL,		/* hash() */
	0		/* lorder */
};

#define	FILE_INSECURE	0x01
#define	FILE_SECURE	0x02
#define	FILE_ORIG	0x04


struct pwddb {
	DB *db;
	char dbname[MAX(MAXPATHLEN, LINE_MAX * 2)];
	const char *fname;
	uint32_t rversion;
	uint32_t wversion;
};

static char	*pname;				/* password file name */
static char	prefix[MAXPATHLEN];
static char	oldpwdfile[MAX(MAXPATHLEN, LINE_MAX * 2)];
static int 	lorder = BYTE_ORDER;
static int	clean;
static int	verbose;
static int	warning;
static struct pwddb sdb, idb;


void	bailout(void) __attribute__((__noreturn__));
void	cp(const char *, const char *, mode_t);
int	deldbent(struct pwddb *, int, void *);
void	error(const char *);
int	getdbent(struct pwddb *, int, void *, struct passwd **);
void	inconsistancy(void);
void	install(const char *, const char *);
int	main(int, char **);
void	putdbents(struct pwddb *, struct passwd *, const char *, int, int, int,
    int);
void	putyptoken(struct pwddb *);
void	rm(const char *);
int	scan(FILE *, struct passwd *, int *, int *);
void	usage(void) __attribute__((__noreturn__));
void	wr_error(const char *);
void	checkversion(struct pwddb *);
uint32_t getversion(const char *);
void	setversion(struct pwddb *);

#define SWAP(sw) \
    ((sizeof(sw) == 2 ? (typeof(sw))bswap16((uint16_t)sw) : \
    (sizeof(sw) == 4 ? (typeof(sw))bswap32((uint32_t)sw) : \
    (sizeof(sw) == 8 ? (typeof(sw))bswap64((uint64_t)sw) : (abort(), 0)))))

static void
closedb(struct pwddb *db)
{
    if ((*db->db->close)(db->db) < 0)
	    wr_error(db->dbname);
}

static void
opendb(struct pwddb *db, const char *dbname, const char *username,
    uint32_t req_version, int flags, int perm)
{
	char buf[MAXPATHLEN];

	(void)snprintf(db->dbname, sizeof(db->dbname), "%s%s.tmp", prefix,
	    dbname);

	if (username != NULL) {
		snprintf(buf, sizeof(buf), "%s%s", prefix, dbname);
		cp(buf, db->dbname, perm);
	}

	db->db = dbopen(db->dbname, flags, perm, DB_HASH, &openinfo);
	if (db->db == NULL)
		error(db->dbname);

	db->fname = dbname;
	db->rversion = getversion(dbname);
	if (req_version == ~0U)
		db->wversion = db->rversion;
	else
		db->wversion = req_version;

	if (warning && db->rversion == 0 && db->wversion == 0) {
		warnx("Database %s is a version %u database.",
		    db->fname, db->rversion);
		warnx("Use %s -V 1 to upgrade once you've recompiled "
		    "all your binaries.", getprogname());
	}
	if (verbose)
		fprintf(stderr, "%s: %s version %u requested %u\n",
		    getprogname(), db->fname, db->rversion, db->wversion);

	if (db->wversion != db->rversion) {
		if (username != NULL)
			checkversion(db);
		else if (verbose) {
		    fprintf(stderr, "%s: changing %s from version "
			"%u to version %u\n",
			getprogname(), db->fname,
			db->rversion, db->wversion);
		}
	}
	setversion(db);
}

int
main(int argc, char *argv[])
{
	int ch, makeold, tfd, lineno, found, rv, hasyp, secureonly;
	struct passwd pwd, *tpwd;
	char *username;
	FILE *fp, *oldfp;
	sigset_t set;
	int dbflg, uid_dbflg, newuser, olduid, flags;
	struct stat st;
	u_int cachesize;
	uint32_t req_version;

	prefix[0] = '\0';
	makeold = 0;
	oldfp = NULL;
	username = NULL;
	hasyp = 0;
	secureonly = 0;
	found = 0;
	newuser = 0;
	cachesize = 0;
	verbose = 0;
	warning = 0;
	req_version = ~0U;

	while ((ch = getopt(argc, argv, "BLc:d:psu:V:vw")) != -1)
		switch (ch) {
		case 'B':			/* big-endian output */
			lorder = BIG_ENDIAN;
			break;
		case 'L':			/* little-endian output */
			lorder = LITTLE_ENDIAN;
			break;
		case 'c':
			cachesize = atoi(optarg) * 1024 * 1024;
			break;
		case 'd':			/* set prefix */
			strlcpy(prefix, optarg, sizeof(prefix));
			break;
		case 'p':			/* create V7 "file.orig" */
			makeold = 1;
			break;
		case 's':			/* modify secure db only */
			secureonly = 1;
			break;
		case 'u':			/* modify one user only */
			username = optarg;
			break;
		case 'V':
			req_version = (uint32_t)atoi(optarg);
			if (req_version > 1)
				err(1, "Unknown version %u\n", req_version);
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			warning++;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	if (username != NULL)
		if (username[0] == '+' || username[0] == '-')
			usage();
	if (secureonly)
		makeold = 0;

	/*
	 * This could be changed to allow the user to interrupt.
	 * Probably not worth the effort.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGTSTP);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGTERM);
	(void)sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

	/* We don't care what the user wants. */
	(void)umask(0);

	if (username == NULL)
		flags = O_RDWR | O_CREAT | O_EXCL;
	else
		flags = O_RDWR;

	pname = *argv;
	/* Open the original password file */
	if ((fp = fopen(pname, "r")) == NULL)
		error(pname);

	openinfo.lorder = lorder;

	if (fstat(fileno(fp), &st) == -1)
		error(pname);

	if (cachesize) {
		openinfo.cachesize = cachesize;
	} else {
		/* Tweak openinfo values for large passwd files. */
		cachesize = st.st_size * 20;
		if (cachesize > MAX_CACHESIZE)
			cachesize = MAX_CACHESIZE;
		else if (cachesize < MIN_CACHESIZE)
			cachesize = MIN_CACHESIZE;
		openinfo.cachesize = cachesize;
	}

	/* Open the temporary insecure password database. */
	if (!secureonly) {
		opendb(&idb, _PATH_MP_DB, username, req_version,
		    flags, PERM_INSECURE);
		clean |= FILE_INSECURE;
	}
		 

	/* Open the temporary encrypted password database. */
	opendb(&sdb, _PATH_SMP_DB, username, req_version, flags, PERM_SECURE);
	clean |= FILE_SECURE;

	/*
	 * Open file for old password file.  Minor trickiness -- don't want to
	 * chance the file already existing, since someone (stupidly) might
	 * still be using this for permission checking.  So, open it first and
	 * fdopen the resulting fd.  The resulting file should be readable by
	 * everyone.
	 */
	if (makeold) {
		(void)snprintf(oldpwdfile, sizeof(oldpwdfile), "%s.orig",
		    pname);
		if ((tfd = open(oldpwdfile, O_WRONLY | O_CREAT | O_EXCL,
		    PERM_INSECURE)) < 0)
			error(oldpwdfile);
		clean |= FILE_ORIG;
		if ((oldfp = fdopen(tfd, "w")) == NULL)
			error(oldpwdfile);
	}

	if (username != NULL) {
		uid_dbflg = 0;
		dbflg = 0;

		/*
		 * Determine if this is a new entry.
		 */
		if (getdbent(&sdb, _PW_KEYBYNAME, username, &tpwd))
			newuser = 1;
		else {
			newuser = 0;
			olduid = tpwd->pw_uid;
		}

	} else {
		uid_dbflg = R_NOOVERWRITE;
		dbflg = R_NOOVERWRITE;
	}

	/*
	 * If we see something go by that looks like YP, we save a special
	 * pointer record, which if YP is enabled in the C lib, will speed
	 * things up.
	 */
	for (lineno = 0; scan(fp, &pwd, &flags, &lineno);) {
		/*
		 * Create original format password file entry.
		 */
		if (makeold) {
			(void)fprintf(oldfp, "%s:*:%d:%d:%s:%s:%s\n",
			    pwd.pw_name, pwd.pw_uid, pwd.pw_gid, pwd.pw_gecos,
			    pwd.pw_dir, pwd.pw_shell);
			if (ferror(oldfp))
				wr_error(oldpwdfile);
		}

		if (username == NULL) {
			/* Look like YP? */
			if (pwd.pw_name[0] == '+' || pwd.pw_name[0] == '-')
				hasyp++;

			/* Warn about potentially unsafe uid/gid overrides. */
			if (pwd.pw_name[0] == '+') {
				if ((flags & _PASSWORD_NOUID) == 0 &&
				    pwd.pw_uid == 0)
					warnx("line %d: superuser override "
					    "in YP inclusion", lineno);
				if ((flags & _PASSWORD_NOGID) == 0 &&
				    pwd.pw_gid == 0)
					warnx("line %d: wheel override "
					    "in YP inclusion", lineno);
			}

			/* Write the database entry out. */
			if (!secureonly)
				putdbents(&idb, &pwd, "*", flags, lineno, dbflg,
				    uid_dbflg);
			continue;
		} else if (strcmp(username, pwd.pw_name) != 0)
			continue;

		if (found) {
			warnx("user `%s' listed twice in password file",
			    username);
			bailout();
		}

		/*
		 * Ensure that the text file and database agree on
		 * which line the record is from.
		 */
		rv = getdbent(&sdb, _PW_KEYBYNUM, &lineno, &tpwd);
		if (newuser) {
			if (rv == 0)
				inconsistancy();
		} else if (rv == -1 ||
			strcmp(username, tpwd->pw_name) != 0)
			inconsistancy();
		else if ((uid_t)olduid != pwd.pw_uid) {
			/*
			 * If we're changing UID, remove the BYUID
			 * record for the old UID only if it has the
			 * same username.
			 */
			if (!getdbent(&sdb, _PW_KEYBYUID, &olduid, &tpwd)) {
				if (strcmp(username, tpwd->pw_name) == 0) {
					if (!secureonly)
						deldbent(&idb, _PW_KEYBYUID,
						    &olduid);
					deldbent(&sdb, _PW_KEYBYUID, &olduid);
				}
			} else
				inconsistancy();
		}

		/*
		 * If there's an existing BYUID record for the new UID and
		 * the username doesn't match then be sure not to overwrite
		 * it.
		 */
		if (!getdbent(&sdb, _PW_KEYBYUID, &pwd.pw_uid, &tpwd))
			if (strcmp(username, tpwd->pw_name) != 0)
				uid_dbflg = R_NOOVERWRITE;

		/* Write the database entries out */
		if (!secureonly)
			putdbents(&idb, &pwd, "*", flags, lineno, dbflg,
			    uid_dbflg);
		putdbents(&sdb, &pwd, pwd.pw_passwd, flags, lineno, dbflg,
		    uid_dbflg);

		found = 1;
		if (!makeold)
			break;
	}

	if (!secureonly) {
		/* Store YP token if needed. */
		if (hasyp)
			putyptoken(&idb);

		/* Close the insecure database. */
		closedb(&idb);
	}

	/*
	 * If rebuilding the databases, we re-parse the text file and write
	 * the secure entries out in a separate pass.
	 */
	if (username == NULL) {
		rewind(fp);
		for (lineno = 0; scan(fp, &pwd, &flags, &lineno);)
			putdbents(&sdb, &pwd, pwd.pw_passwd, flags,
			    lineno, dbflg, uid_dbflg);

		/* Store YP token if needed. */
		if (hasyp)
			putyptoken(&sdb);
	} else if (!found) {
		warnx("user `%s' not found in password file", username);
		bailout();
	}

	/* Close the secure database. */
	closedb(&sdb);

	/* Install as the real password files. */
	if (!secureonly)
		install(idb.dbname, idb.fname);
	install(sdb.dbname, sdb.fname);

	/* Install the V7 password file. */
	if (makeold) {
		if (fflush(oldfp) == EOF)
			wr_error(oldpwdfile);
		if (fclose(oldfp) == EOF)
			wr_error(oldpwdfile);
		install(oldpwdfile, _PATH_PASSWD);
	}

	/* Set master.passwd permissions, in case caller forgot. */
	(void)fchmod(fileno(fp), S_IRUSR|S_IWUSR);
	if (fclose(fp) == EOF)
		wr_error(pname);

	/*
	 * Move the temporary master password file LAST -- chpass(1),
	 * passwd(1), vipw(8) and friends all use its existence to block
	 * other incarnations of themselves.  The rename means that
	 * everything is unlocked, as the original file can no longer be
	 * accessed.
	 */
	install(pname, _PATH_MASTERPASSWD);
	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

int
scan(FILE *fp, struct passwd *pw, int *flags, int *lineno)
{
	static char line[LINE_MAX];
	char *p;
	int oflags;

	if (fgets(line, sizeof(line), fp) == NULL)
		return (0);
	(*lineno)++;

	/*
	 * ``... if I swallow anything evil, put your fingers down my
	 * throat...''
	 *	-- The Who
	 */
	if ((p = strchr(line, '\n')) == NULL) {
		warnx("line too long");
		errno = EFTYPE;	/* XXX */
		error(pname);
	}
	*p = '\0';
	if (strcmp(line, "+") == 0)
		strcpy(line, "+:::::::::");	/* pw_scan() can't handle "+" */
	oflags = 0;
	if (!pw_scan(line, pw, &oflags)) {
		warnx("at line #%d", *lineno);
		errno = EFTYPE;	/* XXX */
		error(pname);
	}
	*flags = oflags;

	return (1);
}

void
install(const char *from, const char *to)
{
	char buf[MAXPATHLEN];
	char errbuf[BUFSIZ];
	int sverrno;

	snprintf(buf, sizeof(buf), "%s%s", prefix, to);
	if (rename(from, buf)) {
		sverrno = errno;
		(void)snprintf(errbuf, sizeof(errbuf), "%s to %s", from, buf);
		errno = sverrno;
		error(errbuf);
	}
}

void
rm(const char *victim)
{

	if (unlink(victim) < 0)
		warn("unlink(%s)", victim);
}

void                    
cp(const char *from, const char *to, mode_t mode)              
{               
	static char buf[MAXBSIZE];
	int from_fd, rcount, to_fd, wcount, sverrno;

	if ((from_fd = open(from, O_RDONLY, 0)) < 0)
		error(from);
	if ((to_fd = open(to, O_WRONLY | O_CREAT | O_EXCL, mode)) < 0)
		error(to);
	while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
		wcount = write(to_fd, buf, rcount);
		if (rcount != wcount || wcount == -1) {
			sverrno = errno;
			(void)snprintf(buf, sizeof(buf), "%s to %s", from, to);
			errno = sverrno;
			error(buf);
		}
	}

	if (rcount < 0) {
		sverrno = errno;
		(void)snprintf(buf, sizeof(buf), "%s to %s", from, to);
		errno = sverrno;
		error(buf);
	}
}

void
wr_error(const char *str)
{
	char errbuf[BUFSIZ];
	int sverrno;

	sverrno = errno;

	(void)snprintf(errbuf, sizeof(errbuf),
		"attempt to write %s failed", str);

	errno = sverrno;
	error(errbuf);
}

void
error(const char *str)
{

	warn("%s", str);
	bailout();
}

void
inconsistancy(void)
{

	warnx("text files and databases are inconsistent");
	warnx("re-build the databases without -u");
	bailout();
}

void
bailout(void)
{

	if ((clean & FILE_ORIG) != 0)
		rm(oldpwdfile);
	if ((clean & FILE_SECURE) != 0)
		rm(sdb.dbname);
	if ((clean & FILE_INSECURE) != 0)
		rm(idb.dbname);

	exit(EXIT_FAILURE);
}

uint32_t
getversion(const char *fname)
{
	DBT data, key;
	int ret;
	uint32_t version = 0;
	DB *db;

	db = dbopen(fname, O_RDONLY, PERM_INSECURE, DB_HASH, NULL);
	if (db == NULL) {
		warn("Cannot open database %s", fname);
		bailout();
	}
	key.data = __UNCONST("VERSION");
	key.size = strlen((const char *)key.data) + 1;

	switch (ret = (*db->get)(db, &key, &data, 0)) {
	case -1:	/* Error */
		warn("Cannot get VERSION record from database");
		goto out;
	case 0:
		if (data.size != sizeof(version)) {
		    warnx("Bad VERSION record in database");
		    goto out;
		}
		memcpy(&version, data.data, sizeof(version));
		/*FALLTHROUGH*/
	case 1:
		if (ret == 1)
			warnx("Database %s has no version info", fname);
		(*db->close)(db);
		return version;
	default:
		warnx("internal error db->get returns %d", ret);
		goto out;
	}
out:
	(*db->close)(db);
	bailout();
}

void
setversion(struct pwddb *db)
{
	DBT data, key;
	key.data = __UNCONST("VERSION");
	key.size = strlen((const char *)key.data) + 1;

	data.data = &db->wversion;
	data.size = sizeof(uint32_t);

	if ((*db->db->put)(db->db, &key, &data, 0) != 0) {
		warn("Can't write VERSION record to %s", db->dbname);
		bailout();
	}
}

void
checkversion(struct pwddb *db)
{
	(void)fprintf(stderr, "%s: you cannot change a single "
	    "record from version %u to version %u\n", getprogname(),
	    db->rversion, db->wversion);
	bailout();
}

/*
 * Write entries to a database for a single user. 
 *
 * The databases actually contain three copies of the original data.  Each
 * password file entry is converted into a rough approximation of a ``struct
 * passwd'', with the strings placed inline.  This object is then stored as
 * the data for three separate keys.  The first key * is the pw_name field
 * prepended by the _PW_KEYBYNAME character.  The second key is the pw_uid
 * field prepended by the _PW_KEYBYUID character.  The third key is the line
 * number in the original file prepended by the _PW_KEYBYNUM character. 
 * (The special characters are prepended to ensure that the keys do not
 * collide.)
 */
#define	COMPACT(e)	for (t = e; (*p++ = *t++) != '\0';)

void
putdbents(struct pwddb *db, struct passwd *pw, const char *passwd, int flags,
      int lineno, int dbflg, int uid_dbflg)
{
	struct passwd pwd;
	char buf[MAX(MAXPATHLEN, LINE_MAX * 2)], tbuf[1024], *p;
	DBT data, key;
	const char *t;
	u_int32_t x;
	int len;

	memcpy(&pwd, pw, sizeof(pwd));
	data.data = (u_char *)buf;
	key.data = (u_char *)tbuf;

	if (lorder != BYTE_ORDER) {
		pwd.pw_uid = SWAP(pwd.pw_uid);
		pwd.pw_gid = SWAP(pwd.pw_gid);
	}

#define WRITEPWTIMEVAR(pwvar) \
	do { \
		if (db->wversion == 0 && \
		    sizeof(pwvar) == sizeof(uint64_t)) { \
			uint32_t tmp = (uint32_t)pwvar; \
			if (lorder != BYTE_ORDER) \
				tmp = SWAP(tmp); \
			memmove(p, &tmp, sizeof(tmp)); \
			p += sizeof(tmp); \
		} else if (db->wversion == 1 && \
		    sizeof(pwvar) == sizeof(uint32_t)) { \
			uint64_t tmp = pwvar; \
			if (lorder != BYTE_ORDER) \
				tmp = SWAP(tmp); \
			memmove(p, &tmp, sizeof(tmp)); \
			p += sizeof(tmp); \
		} else { \
			if (lorder != BYTE_ORDER) \
				pwvar = SWAP(pwvar); \
			memmove(p, &pwvar, sizeof(pwvar)); \
			p += sizeof(pwvar); \
		} \
	} while (/*CONSTCOND*/0)

	/* Create insecure data. */
	p = buf;
	COMPACT(pwd.pw_name);
	COMPACT(passwd);
	memmove(p, &pwd.pw_uid, sizeof(pwd.pw_uid));
	p += sizeof(pwd.pw_uid);
	memmove(p, &pwd.pw_gid, sizeof(pwd.pw_gid));
	p += sizeof(pwd.pw_gid);
	WRITEPWTIMEVAR(pwd.pw_change);
	COMPACT(pwd.pw_class);
	COMPACT(pwd.pw_gecos);
	COMPACT(pwd.pw_dir);
	COMPACT(pwd.pw_shell);
	WRITEPWTIMEVAR(pwd.pw_expire);
	x = flags;
	if (lorder != BYTE_ORDER)
		x = SWAP(x);
	memmove(p, &x, sizeof(x));
	p += sizeof(flags);
	data.size = p - buf;

	/* Store insecure by name. */
	tbuf[0] = _PW_KEYBYNAME;
	len = strlen(pwd.pw_name);
	memmove(tbuf + 1, pwd.pw_name, len);
	key.size = len + 1;
	if ((*db->db->put)(db->db, &key, &data, dbflg) == -1)
		wr_error(db->dbname);

	/* Store insecure by number. */
	tbuf[0] = _PW_KEYBYNUM;
	x = lineno;
	if (lorder != BYTE_ORDER)
		x = SWAP(x);
	memmove(tbuf + 1, &x, sizeof(x));
	key.size = sizeof(x) + 1;
	if ((*db->db->put)(db->db, &key, &data, dbflg) == -1)
		wr_error(db->dbname);

	/* Store insecure by uid. */
	tbuf[0] = _PW_KEYBYUID;
	memmove(tbuf + 1, &pwd.pw_uid, sizeof(pwd.pw_uid));
	key.size = sizeof(pwd.pw_uid) + 1;
	if ((*db->db->put)(db->db, &key, &data, uid_dbflg) == -1)
		wr_error(db->dbname);
}

int
deldbent(struct pwddb *db, int type, void *keyp)
{
	char tbuf[1024];
	DBT key;
	u_int32_t x;
	int len, rv;

	key.data = (u_char *)tbuf;

	switch (tbuf[0] = type) {
	case _PW_KEYBYNAME:
		len = strlen((char *)keyp);
		memcpy(tbuf + 1, keyp, len);
		key.size = len + 1;
		break;

	case _PW_KEYBYNUM:
	case _PW_KEYBYUID:
		x = *(int *)keyp;
		if (lorder != BYTE_ORDER)
			x = SWAP(x);
		memmove(tbuf + 1, &x, sizeof(x));
		key.size = sizeof(x) + 1;
		break;
	}

	if ((rv = (*db->db->del)(db->db, &key, 0)) == -1)
		wr_error(db->dbname);
	return (rv);
}

int
getdbent(struct pwddb *db, int type, void *keyp, struct passwd **tpwd)
{
	static char buf[MAX(MAXPATHLEN, LINE_MAX * 2)];
	static struct passwd pwd;
	char tbuf[1024], *p;
	DBT key, data;
	u_int32_t x;
	int len, rv;

	data.data = (u_char *)buf;
	data.size = sizeof(buf);
	key.data = (u_char *)tbuf;

	switch (tbuf[0] = type) {
	case _PW_KEYBYNAME:
		len = strlen((char *)keyp);
		memcpy(tbuf + 1, keyp, len);
		key.size = len + 1;
		break;

	case _PW_KEYBYNUM:
	case _PW_KEYBYUID:
		x = *(int *)keyp;
		if (lorder != BYTE_ORDER)
			x = SWAP(x);
		memmove(tbuf + 1, &x, sizeof(x));
		key.size = sizeof(x) + 1;
		break;
	}

	if ((rv = (*db->db->get)(db->db, &key, &data, 0)) == 1)
		return (rv);
	if (rv == -1)
		error(db->dbname);

	p = (char *)data.data;

	pwd.pw_name = p;
	while (*p++ != '\0')
		continue;
	pwd.pw_passwd = p;
	while (*p++ != '\0')
		continue;

	memcpy(&pwd.pw_uid, p, sizeof(pwd.pw_uid));
	p += sizeof(pwd.pw_uid);
	memcpy(&pwd.pw_gid, p, sizeof(pwd.pw_gid));
	p += sizeof(pwd.pw_gid);

#define READPWTIMEVAR(pwvar) \
	do { \
		if (db->rversion == 0 && \
		    sizeof(pwvar) == sizeof(uint64_t)) { \
			uint32_t tmp; \
			memcpy(&tmp, p, sizeof(tmp)); \
			p += sizeof(tmp); \
			if (lorder != BYTE_ORDER) \
				pwvar = SWAP(tmp); \
			else \
				pwvar = tmp; \
		} else if (db->rversion == 1 && \
		    sizeof(pwvar) == sizeof(uint32_t)) { \
			uint64_t tmp; \
			memcpy(&tmp, p, sizeof(tmp)); \
			p += sizeof(tmp); \
			if (lorder != BYTE_ORDER) \
				pwvar = (uint32_t)SWAP(tmp); \
			else \
				pwvar = (uint32_t)tmp; \
		} else { \
			memcpy(&pwvar, p, sizeof(pwvar)); \
			p += sizeof(pwvar); \
			if (lorder != BYTE_ORDER) \
				pwvar = SWAP(pwvar); \
		} \
	} while (/*CONSTCOND*/0)
		
	READPWTIMEVAR(pwd.pw_change);

	pwd.pw_class = p;
	while (*p++ != '\0')
		continue;
	pwd.pw_gecos = p;
	while (*p++ != '\0')
		continue;
	pwd.pw_dir = p;
	while (*p++ != '\0')
		continue;
	pwd.pw_shell = p;
	while (*p++ != '\0')
		continue;

	READPWTIMEVAR(pwd.pw_expire);

	if (lorder != BYTE_ORDER) {
		pwd.pw_uid = SWAP(pwd.pw_uid);
		pwd.pw_gid = SWAP(pwd.pw_gid);
	}

	*tpwd = &pwd;
	return (0);
}

void
putyptoken(struct pwddb *db)
{
	DBT data, key;

	key.data = __UNCONST(__yp_token);
	key.size = strlen(__yp_token);
	data.data = (u_char *)NULL;
	data.size = 0;

	if ((*db->db->put)(db->db, &key, &data, R_NOOVERWRITE) == -1)
		wr_error(db->dbname);
}

void
usage(void)
{

	(void)fprintf(stderr,
	    "Usage: %s [-BLpsvw] [-c cachesize] [-d directory] [-u user] "
	    "[-V version] file\n",
	    getprogname());
	exit(EXIT_FAILURE);
}
