/*	$NetBSD: cksum.c,v 1.33 2006/04/23 16:40:16 hubertf Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James W. Williams of NASA Goddard Space Flight Center.
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

/*-
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * James W. Williams of NASA Goddard Space Flight Center.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#if HAVE_NBTOOL_CONFIG_H
#include "nbtool_config.h"
#endif

#include <sys/cdefs.h>
#if defined(__COPYRIGHT) && !defined(lint)
__COPYRIGHT("@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n");
#endif /* not lint */

#if defined(__RCSID) && !defined(lint)
#if 0
static char sccsid[] = "@(#)cksum.c	8.2 (Berkeley) 4/28/95";
#endif
__RCSID("$NetBSD: cksum.c,v 1.33 2006/04/23 16:40:16 hubertf Exp $");
#endif /* not lint */

#include <sys/cdefs.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <md5.h>
#include <md4.h>
#include <md2.h>
#include <sha1.h>
#include <crypto/sha2.h>
#include <crypto/rmd160.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#define HASH_MD2	0
#define HASH_MD4	1
#define HASH_MD5	2
#define HASH_SHA1	3
#define HASH_RMD160	4

typedef char *(*_filefunc)(const char *, char *);

struct hash {
	const char *progname;
	const char *hashname;
	void (*stringfunc)(const char *);
	void (*timetrialfunc)(void);
	void (*testsuitefunc)(void);
	void (*filterfunc)(int);
	char *(*filefunc)(const char *, char *);
} hashes[] = {
	{ "md2", "MD2",
	  MD2String, MD2TimeTrial, MD2TestSuite,
	  MD2Filter, MD2File },
	{ "md4", "MD4",
	  MD4String, MD4TimeTrial, MD4TestSuite,
	  MD4Filter, MD4File },
	{ "md5", "MD5",
	  MD5String, MD5TimeTrial, MD5TestSuite,
	  MD5Filter, MD5File },
	{ "sha1", "SHA1",
	  SHA1String, SHA1TimeTrial, SHA1TestSuite,
	  SHA1Filter, (_filefunc) SHA1File },
	{ "rmd160", "RMD160",
	  RMD160String, RMD160TimeTrial, RMD160TestSuite,
	  RMD160Filter, (_filefunc) RMD160File },
	{ "sha256", "SHA256",
	  SHA256_String, SHA256_TimeTrial, SHA256_TestSuite,
	  SHA256_Filter, (_filefunc) SHA256_File },
	{ "sha384", "SHA384",
	  SHA384_String, SHA384_TimeTrial, SHA384_TestSuite,
	  SHA384_Filter, (_filefunc) SHA384_File },
	{ "sha512", "SHA512",
	  SHA512_String, SHA512_TimeTrial, SHA512_TestSuite,
	  SHA512_Filter, (_filefunc) SHA512_File },
	{ NULL }
};

int	hash_digest_file(char *, struct hash *, int);
void	requirehash(const char *);
void	usage(void);

int
main(int argc, char **argv)
{
	int ch, fd, rval, dosum, pflag, nohashstdin;
	u_int32_t val;
	off_t len;
	char *fn;
	const char *progname;
	int (*cfncn) (int, u_int32_t *, off_t *);
	void (*pfncn) (char *, u_int32_t, off_t);
	struct hash *hash;
	int normal, i, check_warn;
	char *checkfile;

	cfncn = NULL;
	pfncn = NULL;
	dosum = pflag = nohashstdin = 0;
	normal = 0;
	checkfile = NULL;
	check_warn = 0;

	setlocale(LC_ALL, "");

	progname = getprogname();

	for (hash = hashes; hash->hashname != NULL; hash++)
		if (strcmp(progname, hash->progname) == 0)
			break;

	if (hash->hashname == NULL) {
		hash = NULL;

		if (!strcmp(progname, "sum")) {
			dosum = 1;
			cfncn = csum1;
			pfncn = psum1;
		} else {
			cfncn = crc;
			pfncn = pcrc;
		}
	}

	/*
	 * The -1, -2, -4, -5, -6, and -m flags should be deprecated, but
	 * are still supported in code to not break anything that might
	 * be using them.
	 */
	while ((ch = getopt(argc, argv, "a:c:mno:ps:twx12456")) != -1)
		switch(ch) {
		case 'a':
			if (hash != NULL || dosum) {
				warnx("illegal use of -a option\n");
				usage();
			}
			i = 0;
			while (hashes[i].hashname != NULL) {
				if (!strcasecmp(hashes[i].hashname, optarg)) {
					hash = &hashes[i];
					break;
				}
				i++;
			}
			if (hash == NULL) {
				if (!strcasecmp(optarg, "old1")) {
					cfncn = csum1;
					pfncn = psum1;
				} else if (!strcasecmp(optarg, "old2")) {
					cfncn = csum2;
					pfncn = psum2;
				} else if (!strcasecmp(optarg, "crc")) {
					cfncn = crc;
					pfncn = pcrc;
				} else {
					warnx("illegal argument to -a option");
					usage();
				}
			}
			break;
		case '2':
			if (dosum) {
				warnx("sum mutually exclusive with md2");
				usage();
			}
			hash = &hashes[HASH_MD2];
			break;
		case '4':
			if (dosum) {
				warnx("sum mutually exclusive with md4");
				usage();
			}
			hash = &hashes[HASH_MD4];
			break;
		case 'm':
		case '5':
			if (dosum) {
				warnx("sum mutually exclusive with md5");
				usage();
			}
			hash = &hashes[HASH_MD5];
			break;
		case '1':
			if (dosum) {
				warnx("sum mutually exclusive with sha1");
				usage();
			}
			hash = &hashes[HASH_SHA1];
			break;
		case '6':
			if (dosum) {
				warnx("sum mutually exclusive with rmd160");
				usage();
			}
			hash = &hashes[HASH_RMD160];
			break;
		case 'c':
			checkfile = optarg;
			break;
		case 'n':
			normal = 1;
			break;
		case 'o':
			if (hash) {
				warnx("%s mutually exclusive with sum",
				      hash->hashname);
				usage();
			}
			if (!strcmp(optarg, "1")) {
				cfncn = csum1;
				pfncn = psum1;
			} else if (!strcmp(optarg, "2")) {
				cfncn = csum2;
				pfncn = psum2;
			} else {
				warnx("illegal argument to -o option");
				usage();
			}
			break;
		case 'p':
			if (hash == NULL)
				requirehash("-p");
			pflag = 1;
			break;
		case 's':
			if (hash == NULL)
				requirehash("-s");
			nohashstdin = 1;
			hash->stringfunc(optarg);
			break;
		case 't':
			if (hash == NULL)
				requirehash("-t");
			nohashstdin = 1;
			hash->timetrialfunc();
			break;
		case 'w':
			check_warn = 1;
			break;
		case 'x':
			if (hash == NULL)
				requirehash("-x");
			nohashstdin = 1;
			hash->testsuitefunc();
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (checkfile) {
		/*
		 * Verify checksums
		 */
		FILE *f;
		char buf[BUFSIZ];
		char *s, *p_filename, *p_cksum;
		int l_filename, l_cksum;
		char filename[BUFSIZ];
		char cksum[BUFSIZ];
		int ok,cnt,badcnt;

		rval = 0;
		cnt = badcnt = 0;
		
		f = fopen(checkfile, "r");
		if (f == NULL)
			err(1, "Cannot read %s", checkfile);
		while(fgets(buf, sizeof(buf), f) != NULL) {
			s=strrchr(buf, '\n');
			if (s)
				*s = '\0';

			p_cksum = p_filename = NULL;

			p_filename = strchr(buf, '(');
			if (p_filename) {
				/*
				 * Assume 'normal' output if there's a '('
				 */
				p_filename += 1;
				normal = 0;

				p_cksum = strrchr(p_filename, ')');
				if (p_cksum == NULL) {
					if (check_warn)
						warnx("bogus format: %s. "
						      "Skipping...",
						      buf);
					rval = 1;
					continue;
				}
 				p_cksum += 4;

				l_cksum = strlen(p_cksum);
				l_filename = p_cksum - p_filename - 4;
					
				/* Sanity check */
				if (strncmp(buf, hash->hashname,
					    strlen(hash->hashname)) != 0) {
					warnx("%.*s: %s checksum expected, "
					      "%.*s found, skipping.",
					      l_filename,
					      p_filename,
					      hash->hashname,
					      strlen(hash->hashname),
					      buf);
					rval = 1;
					continue;
				}

			} else {
				if (hash) {
					/*
					 * 'normal' output, no (ck)sum
					 */
					normal = 1;
					
					p_cksum = buf;
					p_filename = strchr(buf, ' ');
					if (p_filename == NULL) {
						if (check_warn)
							warnx("no filename in %s? "
							      "Skipping...", buf);
						rval = 1;
						continue;
					}
					p_filename++;
					l_filename = strlen(p_filename);
					l_cksum = p_filename - buf - 1;
				} else {
					/*
					 * sum/cksum output format
					 */
					p_cksum = buf;
					s=strchr(p_cksum, ' ');
					if (s == NULL) {
						if (check_warn)
							warnx("bogus format: %s."
							      " Skipping...",
							      buf);
						rval = 1;
						continue;
					}
					l_cksum = s - p_cksum;

					p_filename = strrchr(buf, ' ');
					if (p_filename == NULL) {
						if (check_warn)
							warnx("no filename in %s?"
							      " Skipping...",
							      buf);
						rval = 1;
						continue;
					}
					p_filename++;
					l_filename = strlen(p_filename);
				}
			}

			strlcpy(filename, p_filename, l_filename+1);
			strlcpy(cksum, p_cksum, l_cksum+1);

			if (hash) {
				if (access(filename, R_OK) == 0
				    && strcmp(cksum, hash->filefunc(filename, NULL)) == 0)
					ok = 1;
				else
					ok = 0;
			} else {
				if ((fd = open(filename, O_RDONLY, 0)) < 0) {
					if (check_warn)
						warn("%s", filename);
					rval = 1;
					ok = 0;
				} else {
					if (cfncn(fd, &val, &len)) 
						ok = 0;
					else {
						u_int32_t should_val;
						
						should_val =
						  strtoul(cksum, NULL, 10);
						if (val == should_val)
							ok = 1;
						else
							ok = 0;
					}
				}
			}

			if (ok)
#ifdef LINUX_CHECK_COMPAT
				printf("%s: OK\n", filename)
#endif
				;
			else {
				printf("%s: FAILED\n", filename);
				badcnt++;
			}
			cnt++;

		}
		fclose(f);

#ifdef LINUX_CHECK_COMPAT
		if (badcnt > 0)
			printf("%s: WARNING: %d of %d computed checksums did NOT match\n",
			       progname, badcnt, cnt);
#endif
		
	} else {
		/*
		 * Calculate checksums
		 */

		fd = STDIN_FILENO;
		fn = NULL;
		rval = 0;
		do {
			if (*argv) {
				fn = *argv++;
				if (hash != NULL) {
					if (hash_digest_file(fn, hash, normal)) {
						warn("%s", fn);
						rval = 1;
					}
					continue;
				}
				if ((fd = open(fn, O_RDONLY, 0)) < 0) {
					warn("%s", fn);
					rval = 1;
					continue;
				}
			} else if (hash && !nohashstdin) {
				hash->filterfunc(pflag);
			}
			
			if (hash == NULL) {
				if (cfncn(fd, &val, &len)) {
					warn("%s", fn ? fn : "stdin");
					rval = 1;
				} else
					pfncn(fn, val, len);
				(void)close(fd);
			}
		} while (*argv);
	}
	exit(rval);
}

int
hash_digest_file(char *fn, struct hash *hash, int normal)
{
	char *cp;

	cp = hash->filefunc(fn, NULL);
	if (cp == NULL)
		return 1;

	if (normal)
		printf("%s %s\n", cp, fn);
	else
		printf("%s (%s) = %s\n", hash->hashname, fn, cp);

	free(cp);

	return 0;
}

void
requirehash(const char *flg)
{
	warnx("%s flag requires `md2', `md4', `md5', `sha1', or `rmd160'",
	    flg);
	usage();
}

void
usage(void)
{

	(void)fprintf(stderr, "usage: cksum [-nw] [-a algorithm | -c file | -m | -1 | -2 | -4 | -5 | -6 \n\t\t| [-o 1 | 2]] [file ...]\n");
	(void)fprintf(stderr, "       sum [-c] [file ...]\n");
	(void)fprintf(stderr,
	    "       md2 [-n] [-p | -t | -x | -s string] [file ...]\n");
	(void)fprintf(stderr,
	    "       md4 [-n] [-p | -t | -x | -s string] [file ...]\n");
	(void)fprintf(stderr,
	    "       md5 [-n] [-p | -t | -x | -s string] [file ...]\n");
	(void)fprintf(stderr,
	    "       sha1 [-n] [-p | -t | -x | -s string] [file ...]\n");
	(void)fprintf(stderr,
	    "       rmd160 [-n] [-p | -t | -x | -s string] [file ...]\n");
	exit(1);
}
