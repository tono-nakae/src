/*	$NetBSD: ifwatchd.c,v 1.7 2002/04/15 17:32:18 tron Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Martin Husemann <martin@NetBSD.ORG>.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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

/*
 * Define this for special treatment of sys/net/if_spppsubr.c based interfaces.
 */
#define SPPP_IF_SUPPORT
 
#include <sys/types.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/if.h>
#include <net/if_dl.h>
#ifdef SPPP_IF_SUPPORT
#include <net/if_sppp.h>
#endif
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <err.h>
#include <ifaddrs.h>

/* local functions */
static void usage(void);
static void dispatch(void*, size_t);
static void check_addrs(char *cp, int addrs, int is_up);
static void invoke_script(struct sockaddr *sa, struct sockaddr *dst, int is_up, int ifindex);
static void list_interfaces(const char *ifnames);
static void rescan_interfaces(void);
static void free_interfaces(void);
static int find_interface(int index);
static void run_initial_ups(void);

#ifdef SPPP_IF_SUPPORT
static int if_is_connected(const char * ifname);
#else
#define	if_is_connected(X)	1
#endif

/* stolen from /sbin/route */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

/* global variables */
static int verbose = 0;
static int inhibit_initial = 0;
static const char *up_script = NULL;
static const char *down_script = NULL;

struct interface_data {
	SLIST_ENTRY(interface_data) next;
	int index;
	char * ifname;
};
SLIST_HEAD(,interface_data) ifs = SLIST_HEAD_INITIALIZER(ifs);

int
main(int argc, char **argv)
{
	int c, s, n;
	int errs = 0;
	char msg[2048], *msgp;

	while ((c = getopt(argc, argv, "vhiu:d:")) != -1)
		switch (c) {
		case 'h':
			usage();
			return 0;
		case 'i':
			inhibit_initial = 1;
			break;
		case 'v':
			verbose++;
			break;

		case 'u':
			up_script = optarg;
			break;

		case 'd':
			down_script = optarg;
			break;

		default:
			errs++;
			break;
		}

	if (errs)
		usage();

	argv += optind;
	argc -= optind;

	if (argc <= 0) 
		usage();

	if (verbose) {
		printf("up_script: %s\ndown_script: %s\n",
			up_script, down_script);
		printf("verbosity = %d\n", verbose);
	}

	while (argc > 0) {
	    list_interfaces(argv[0]);
	    argv++; argc--;
	}

	if (!verbose)
		daemon(0,0);

	if (!inhibit_initial)
		run_initial_ups();

	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0) {
		perror("open routing socket");
		exit(1);
	}

	for (;;) {
		n = read(s, msg, sizeof msg);
		msgp = msg;
		for (msgp = msg; n > 0; n -= ((struct rt_msghdr*)msgp)->rtm_msglen, msgp += ((struct rt_msghdr*)msgp)->rtm_msglen) {
			dispatch(msgp, n);
			
		}
	}

	close(s);
	free_interfaces();

	exit(0);
}

static void
usage()
{
	fprintf(stderr, 
	    "usage:\n"
	    "\tifwatchd [-h] [-v] [-u up-script] [-d down-script] ifname(s)\n"
	    "\twhere:\n"
	    "\t -h       show this help message\n"
	    "\t -v       verbose/debug output, don't run in background\n"
	    "\t -i       no (!) initial run of the up script if the interface\n"
	    "\t          is already up on ifwatchd startup\n"
	    "\t -u <cmd> specify command to run on interface up event\n"
	    "\t -d <cmd> specify command to run on interface down event\n");
	exit(1);
}

static void
dispatch(void *msg, size_t len)
{
	struct rt_msghdr *hd = msg;
	struct ifa_msghdr *ifam;
	int is_up;

	is_up = 0;
	switch (hd->rtm_type) {
	case RTM_NEWADDR:
		is_up = 1;
		goto work;
	case RTM_DELADDR:
		is_up = 0;
		goto work;
	case RTM_IFANNOUNCE:
		rescan_interfaces();
		break;
	}
	if (verbose)
		printf("unknown message ignored\n");
	return;

work:
	ifam = (struct ifa_msghdr *)msg;
	check_addrs((char *)(ifam + 1), ifam->ifam_addrs, is_up);
}

static void
check_addrs(cp, addrs, is_up)
	char    *cp;
	int     addrs, is_up;
{
	struct sockaddr *sa, *ifa = NULL, *brd = NULL;
	int ifndx = 0, i;

	if (addrs == 0)
	    return;
	for (i = 1; i; i <<= 1) {
	    if (i & addrs) {
		sa = (struct sockaddr *)cp;
		if (i == RTA_IFP) {
		    struct sockaddr_dl * li = (struct sockaddr_dl*)sa;
		    ifndx = li->sdl_index;
		    if (!find_interface(ifndx)) {
			if (verbose)
			    printf("ignoring change on interface #%d\n", ifndx);
			return;
		    }
		} else if (i == RTA_IFA) {
		    ifa = sa;
		} else if (i == RTA_BRD) {
		    brd = sa;
		}
		ADVANCE(cp, sa);
	    }
	}
	if (ifa != NULL)
	   invoke_script(ifa, brd, is_up, ifndx);
}

static void
invoke_script(sa, dest, is_up, ifindex)
	struct sockaddr *sa, *dest;
	int is_up, ifindex;
{
	char addr[NI_MAXHOST], daddr[NI_MAXHOST], ifname_buf[IFNAMSIZ],
	     *ifname, *cmd;
	const char *script;

	daddr[0] = 0;
	ifname = if_indextoname(ifindex, ifname_buf);
	if (sa->sa_len == 0) {
	    fprintf(stderr, "illegal socket address (sa_len == 0)\n");
	    return;
	}

	if (getnameinfo(sa, sa->sa_len, addr, sizeof addr, NULL, 0, NI_NUMERICHOST)) {
	    if (verbose)
		printf("getnameinfo failed\n");
	    return;	/* this address can not be handled */
	}
	if (dest != NULL) {
	    if (getnameinfo(dest, dest->sa_len, daddr, sizeof daddr, NULL, 0, NI_NUMERICHOST)) {
		if (verbose)
		    printf("getnameinfo failed\n");
		return;	/* this address can not be handled */
	    }
	}

	script = is_up? up_script : down_script;
	if (script == NULL) return;

	asprintf(&cmd, "%s \"%s\" " _PATH_DEVNULL " 9600 \"%s\" \"%s\"",
			script, ifname, addr, daddr);
	if (cmd == NULL) {
	    fprintf(stderr, "out of memory\n");
	    return;
	}
	if (verbose)
	    printf("calling: %s\n", cmd);
	system(cmd);
	free(cmd);
}

static void list_interfaces(const char *ifnames)
{
	char * names = strdup(ifnames);
	char * name, *lasts;
	static const char sep[] = " \t";
	struct interface_data * p;

	for (name = strtok_r(names, sep, &lasts); name != NULL; name = strtok_r(NULL, sep, &lasts)) {
	    p = malloc(sizeof(*p));
	    SLIST_INSERT_HEAD(&ifs, p, next);
	    p->ifname = strdup(name);
	    p->index = if_nametoindex(p->ifname);
	    if (verbose)
		printf("interface \"%s\" has index %d\n", p->ifname, p->index);
	}
	free(names);
}

static void rescan_interfaces()
{
	struct interface_data * p;
	
	SLIST_FOREACH(p, &ifs, next) {
	    p->index = if_nametoindex(p->ifname);
	    if (verbose)
		printf("interface \"%s\" has index %d\n", p->ifname, p->index);
	}
}

static void free_interfaces()
{
	struct interface_data * p;

	while (!SLIST_EMPTY(&ifs)) {
	    p = SLIST_FIRST(&ifs);
	    SLIST_REMOVE_HEAD(&ifs, next);
	    free(p->ifname);
	    free(p);
	}
}

static int find_interface(index)
	int index;
{
	struct interface_data * p;
	
	SLIST_FOREACH(p, &ifs, next)
	    if (p->index == index)
		return 1;
	return 0;
}

static void run_initial_ups()
{
	struct interface_data * ifd;
	struct ifaddrs *res = NULL, *p;

	if (getifaddrs(&res) == 0) {
	    for (p = res; p; p = p->ifa_next) {
		if ((p->ifa_flags & IFF_UP) == 0)
		    continue;
		if (p->ifa_addr == NULL)
		    continue;
		if (p->ifa_addr->sa_family == AF_LINK)
		    continue;
		SLIST_FOREACH(ifd, &ifs, next) {
		    if (strcmp(ifd->ifname, p->ifa_name) == 0) {
		    	if (if_is_connected(ifd->ifname))
			    invoke_script(p->ifa_addr, p->ifa_dstaddr, 1, ifd->index);
			break;
		    }
		}
	    }
	    freeifaddrs(res);
	}
}

#ifdef SPPP_IF_SUPPORT
/*
 * Special case support for in-kernel PPP interfaces.
 * If these are IFF_UP, but have not yet connected or completed authentication
 * we don't want to call the up script in the initial interface scan (there
 * will be an UP event generated later, when IPCP completes, anyway).
 *
 * If this is no if_spppsubr.c based interface, this ioctl just fails and we
 * treat is as connected.
 */
static int
if_is_connected(const char * ifname)
{
	int s, err;
	struct spppstatus status;

	memset(&status, 0, sizeof status);
	strncpy(status.ifname, ifname, sizeof status.ifname);
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
	    return 1;	/* no idea how to handle this... */
	err = ioctl(s, SPPPGETSTATUS, &status);
	if (err != 0)
	    /* not if_spppsubr.c based - call it connected */
	    status.phase = SPPP_PHASE_NETWORK;
	close(s);
	return status.phase == SPPP_PHASE_NETWORK;
}
#endif
