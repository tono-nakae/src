/*	$KAME: main.c,v 1.29 2001/02/06 15:15:46 sakane Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <paths.h>
#include <err.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"

#include "cfparse.h"
#include "isakmp_var.h"
#include "remoteconf.h"
#include "localconf.h"
#include "session.h"
#include "oakley.h"
#include "pfkey.h"
#include "crypto_openssl.h"
#include "random.h"
#include "backupsa.h"

int f_foreground = 0;	/* force running in foreground. */
int f_local = 0;	/* local test mode.  behave like a wall. */
int vflag = 1;		/* for print-isakmp.c */
static int loading_sa = 0;	/* install sa when racoon boots up. */

static char version[] = "@(#)racoon 20001216 sakane@ydc.co.jp";

int main __P((int, char **));
static void Usage __P((void));
static void parse __P((int, char **));
static void restore_params __P((void));
static void save_params __P((void));
static void saverestore_params __P((int));
static void cleanup_pidfile __P((void));

void
Usage()
{
	printf("Usage: %s [-dFv%s] %s[-f (file)] [-l (file)] [-p (port)]\n",
		pname,
#ifdef INET6
		"46",
#else
		"",
#endif
#ifdef ENABLE_ADMINPORT
		"[-a (port)] "
#else
		""
#endif
		"[-B]"
		);
	printf("   -d: debug level, more -d will generate more debug message.\n");
	printf("   -F: run in foreground, do not become daemon.\n");
	printf("   -v: be more verbose\n");
#ifdef ENABLE_ADMINPORT
	printf("   -a: port number for admin port.\n");
#endif
	printf("   -f: pathname for configuration file.\n");
	printf("   -l: pathname for log file.\n");
	printf("   -p: port number for isakmp (default: %d).\n", PORT_ISAKMP);
#ifdef INET6
	printf("   -6: IPv6 mode.\n");
	printf("   -4: IPv4 mode.\n");
#endif
	printf("   -B: install SA to the kernel from the file "
		"specified by the configuration file.\n");
	exit(1);
}

int
main(ac, av)
	int ac;
	char **av;
{
	int error;

	/* don't let anyone read files I write */
	umask(077);
	if (umask(077) != 077) {
		errx(1, "could not set umask");
		/*NOTREACHED*/
	}

	initlcconf();
	initrmconf();
	oakley_dhinit();
	eay_init_error();

	parse(ac, av);

	ploginit();
	random_init();

	plog(LLV_INFO, LOCATION, NULL, "%s\n", version);
	plog(LLV_INFO, LOCATION, NULL, "@(#)"
	"This product linked software developed by the OpenSSL Project "
	"for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
	"\n");

	if (pfkey_init() < 0)
		exit(1);

	/*
	 * in order to prefer the parameters by command line,
	 * saving some parameters before parsing configuration file.
	 */
	save_params();
	error = cfparse();
	if (error != 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to parse configuration file.\n");
		exit(1);
	}
	restore_params();

	/*
	 * install SAs from the specified file.  If the file is not specified
	 * by the configuration file, racoon will exit.
	 */
	if (loading_sa && !f_local) {
		if (backupsa_from_file() != 0)
			exit(1);
	}

	if (f_foreground)
		close(0);
	else {
		char *pid_file = _PATH_VARRUN "racoon.pid";
		pid_t pid;
		FILE *fp;

		if (daemon(0, 0) < 0) {
			plog(LLV_ERROR, LOCATION, NULL,
				"failed to be daemon. (%s)\n", strerror(errno));
			exit(1);
		}
		/*
		 * In case somebody has started inetd manually, we need to
		 * clear the logname, so that old servers run as root do not
		 * get the user's logname..
		 */
		if (setlogin("") < 0) {
			plog(LLV_ERROR, LOCATION, NULL,
				"cannot clear logname: %s\n", strerror(errno));
			/* no big deal if it fails.. */
		}
		pid = getpid();
		fp = fopen(pid_file, "w");
		if (fp) {
			fprintf(fp, "%ld\n", (long)pid);
			fclose(fp);
		} else {
			plog(LLV_ERROR, LOCATION, NULL,
				"cannot open %s", pid_file);
		}
		if (!f_local) {
			if (atexit(cleanup_pidfile) < 0) {
				plog(LLV_ERROR, LOCATION, NULL,
					"cannot register pidfile cleanup");
			}
		}
	}

	session();

	exit(0);
}

static void
cleanup_pidfile()
{
	char *pid_file = _PATH_VARRUN "racoon.pid";

	(void) unlink(pid_file);
}

static void
parse(ac, av)
	int ac;
	char **av;
{
	extern char *optarg;
	extern int optind;
	int c;
#ifdef YYDEBUG
	extern int yydebug;
#endif

	pname = strrchr(*av, '/');
	if (pname)
		pname++;
	else
		pname = *av;

	while ((c = getopt(ac, av, "dFp:a:f:l:vZB"
#ifdef YYDEBUG
			"y"
#endif
#ifdef INET6
			"46"
#endif
			)) != EOF) {
		switch (c) {
		case 'd':
			loglevel++;
			break;
		case 'F':
			printf("Foreground mode.\n");
			f_foreground = 1;
			break;
		case 'p':
			lcconf->port_isakmp = atoi(optarg);
			break;
		case 'a':
#ifdef ENABLE_ADMINPORT
			lcconf->port_admin = atoi(optarg);
			break;
#else
			fprintf(stderr, "%s: the option is disabled "
			    "in the configuration\n", pname);
			exit(1);
#endif
		case 'f':
			lcconf->racoon_conf = optarg;
			break;
		case 'l':
			plogset(optarg);
			break;
		case 'v':
			vflag++;
			break;
		case 'Z':
			/*
			 * only local test.
			 * To specify -Z option and to choice a appropriate
			 * port number for ISAKMP, you can launch some racoons
			 * on the local host for debug.
			 * pk_sendadd() on initiator side is always failed
			 * even if this flag is used.  Because there is same
			 * spi in the SAD which is inserted by pk_sendgetspi()
			 * on responder side.
			 */
			printf("Local test mode.\n");
			f_local = 1;
			break;
#ifdef YYDEBUG
		case 'y':
			yydebug = 1;
			break;
#endif
#ifdef INET6
		case '4':
			lcconf->default_af = AF_INET;
			break;
		case '6':
			lcconf->default_af = AF_INET6;
			break;
#endif
		case 'B':
			loading_sa++;
			break;
		default:
			Usage();
			break;
		}
	}
	ac -= optind;
	av += optind;

	optind = 1;
	optarg = 0;

	return;
}

static void
restore_params()
{
	saverestore_params(1);
}

static void
save_params()
{
	saverestore_params(0);
}

static void
saverestore_params(f)
	int f;
{
	static u_int16_t s_port_isakmp;
#ifdef ENABLE_ADMINPORT
	static u_int16_t s_port_admin;
#endif

	/* 0: save, 1: restore */
	if (f) {
		lcconf->port_isakmp = s_port_isakmp;
#ifdef ENABLE_ADMINPORT
		lcconf->port_admin = s_port_admin;
#endif
	} else {
		s_port_isakmp = lcconf->port_isakmp;
#ifdef ENABLE_ADMINPORT
		s_port_admin = lcconf->port_admin;
#endif
	}
}
