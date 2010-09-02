/*	$NetBSD: t_lwproc.c,v 1.2 2010/09/02 09:57:34 pooka Exp $	*/

/*
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/wait.h>

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

#include <atf-c.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "../../h_macros.h"

ATF_TC(makelwp);
ATF_TC_HEAD(makelwp, tc)
{

	atf_tc_set_md_var(tc, "descr", "tests that lwps can be attached to "
	    "processes");
}

ATF_TC_BODY(makelwp, tc)
{
	struct lwp *l;
	pid_t pid;

	rump_init();
	RZ(rump_pub_lwproc_newlwp(0));
	ATF_REQUIRE_EQ(rump_pub_lwproc_newlwp(37), ESRCH);
	l = rump_pub_lwproc_curlwp();

	RZ(rump_pub_lwproc_newproc());
	ATF_REQUIRE(rump_pub_lwproc_curlwp() != l);
	l = rump_pub_lwproc_curlwp();

	RZ(rump_pub_lwproc_newlwp(rump_sys_getpid()));
	ATF_REQUIRE(rump_pub_lwproc_curlwp() != l);

	pid = rump_sys_getpid();
	ATF_REQUIRE(pid != -1 && pid != 0);
}

ATF_TC(proccreds);
ATF_TC_HEAD(proccreds, tc)
{

	atf_tc_set_md_var(tc, "descr", "check that procs have different creds");
}

ATF_TC_BODY(proccreds, tc)
{
	struct lwp *l1, *l2;

	rump_init();
	RZ(rump_pub_lwproc_newproc());
	l1 = rump_pub_lwproc_curlwp();

	RZ(rump_pub_lwproc_newproc());
	l2 = rump_pub_lwproc_curlwp();

	RL(rump_sys_setuid(22));
	ATF_REQUIRE_EQ(rump_sys_getuid(), 22);

	rump_pub_lwproc_switch(l1);
	ATF_REQUIRE_EQ(rump_sys_getuid(), 0); /* from parent, proc0 */
	RL(rump_sys_setuid(11));
	ATF_REQUIRE_EQ(rump_sys_getuid(), 11);

	rump_pub_lwproc_switch(l2);
	ATF_REQUIRE_EQ(rump_sys_getuid(), 22);
	rump_pub_lwproc_newlwp(rump_sys_getpid());
	ATF_REQUIRE_EQ(rump_sys_getuid(), 22);
}


ATF_TC(inherit);
ATF_TC_HEAD(inherit, tc)
{

	atf_tc_set_md_var(tc, "descr", "new processes inherit creds from "
	    "parents");
}

ATF_TC_BODY(inherit, tc)
{

	rump_init();

	RZ(rump_pub_lwproc_newproc());
	RL(rump_sys_setuid(66));
	ATF_REQUIRE_EQ(rump_sys_getuid(), 66);

	RZ(rump_pub_lwproc_newproc());
	ATF_REQUIRE_EQ(rump_sys_getuid(), 66);

	/* release lwp and proc */
	rump_pub_lwproc_releaselwp();
	ATF_REQUIRE_EQ(rump_sys_getuid(), 0);
}

ATF_TC(lwps);
ATF_TC_HEAD(lwps, tc)
{

	atf_tc_set_md_var(tc, "descr", "proc can hold many lwps and is "
	    "automatically g/c'd when the last one exits");
}

#define LOOPS 128
ATF_TC_BODY(lwps, tc)
{
	struct lwp *l[LOOPS];
	pid_t mypid;
	struct lwp *l_orig;
	int i;

	rump_init();

	RZ(rump_pub_lwproc_newproc());
	mypid = rump_sys_getpid();
	RL(rump_sys_setuid(375));

	l_orig = rump_pub_lwproc_curlwp();
	for (i = 0; i < LOOPS; i++) {
		mypid = rump_sys_getpid();
		ATF_REQUIRE(mypid != -1 && mypid != 0);
		RZ(rump_pub_lwproc_newlwp(mypid));
		l[i] = rump_pub_lwproc_curlwp();
		ATF_REQUIRE_EQ(rump_sys_getuid(), 375);
	}

	rump_pub_lwproc_switch(l_orig);
	rump_pub_lwproc_releaselwp();
	for (i = 0; i < LOOPS; i++) {
		rump_pub_lwproc_switch(l[i]);
		ATF_REQUIRE_EQ(rump_sys_getpid(), mypid);
		ATF_REQUIRE_EQ(rump_sys_getuid(), 375);
		rump_pub_lwproc_releaselwp();
		ATF_REQUIRE_EQ(rump_sys_getpid(), 0);
		ATF_REQUIRE_EQ(rump_sys_getuid(), 0);
	}

	ATF_REQUIRE_EQ(rump_pub_lwproc_newlwp(mypid), ESRCH);
}

ATF_TC(nolwprelease);
ATF_TC_HEAD(nolwprelease, tc)
{

	atf_tc_set_md_var(tc, "descr", "check that lwp context is required "
	    "for lwproc_releaselwp()");
}

ATF_TC_BODY(nolwprelease, tc)
{
	int status;

	switch (fork()) {
	case 0:
		rump_init();
		rump_pub_lwproc_releaselwp();
		atf_tc_fail("survived");
		break;
	case -1:
		atf_tc_fail_errno("fork");
		break;
	default:
		wait(&status);
		ATF_REQUIRE(WIFSIGNALED(status));
		ATF_REQUIRE_EQ(WTERMSIG(status), SIGABRT);

	}
}

ATF_TC(nolwp);
ATF_TC_HEAD(nolwp, tc)
{

	atf_tc_set_md_var(tc, "descr", "check that curlwp for an implicit "
	    "context is NULL");
}

ATF_TC_BODY(nolwp, tc)
{

	rump_init();
	ATF_REQUIRE_EQ(rump_pub_lwproc_curlwp(), NULL);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, makelwp);
	ATF_TP_ADD_TC(tp, proccreds);
	ATF_TP_ADD_TC(tp, inherit);
	ATF_TP_ADD_TC(tp, lwps);
	ATF_TP_ADD_TC(tp, nolwprelease);
	ATF_TP_ADD_TC(tp, nolwp);

	return atf_no_error();
}
