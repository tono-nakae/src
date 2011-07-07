/*	$NetBSD: t_getenv.c,v 1.1 2011/07/07 15:50:23 jruoho Exp $ */

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matthias Scheler.
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
#include <sys/cdefs.h>
__RCSID("$NetBSD: t_getenv.c,v 1.1 2011/07/07 15:50:23 jruoho Exp $");

#include <atf-c.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define	THREADED_NUM_THREADS	8
#define	THREADED_NUM_VARS	16
#define	THREADED_VAR_NAME	"THREADED%zu"
#define	THREADED_RUN_TIME	10

extern char	**environ;
static void	 *thread_getenv_r(void *);
static void	 *thread_putenv(void *);
static void	 *thread_setenv(void *);
static void	 *thread_unsetenv(void *);

static void *
thread_getenv_r(void *arg)
{
	time_t endtime;

	endtime = *(time_t *)arg;
	do {
		size_t i;
		char name[32], value[128];

		i = lrand48() % THREADED_NUM_VARS;
		(void)snprintf(name, sizeof(name), THREADED_VAR_NAME, i);

		if (getenv_r(name, value, sizeof(value)) == -1) {
			ATF_CHECK(errno == ENOENT);
		}
	} while (time(NULL) < endtime);

	return NULL;
}


static void *
thread_putenv(void *arg)
{
	time_t endtime;
	size_t i;
	static char vars[THREADED_NUM_VARS][128];

	for (i = 0; i < THREADED_NUM_VARS; i++) {
		(void)snprintf(vars[i], sizeof(vars[i]),
		    THREADED_VAR_NAME "=putenv %ld", i, lrand48());
	}

	endtime = *(time_t *)arg;
	do {
		char name[128];

		i = lrand48() % THREADED_NUM_VARS;
		(void)strlcpy(name, vars[i], sizeof(name));
		*strchr(name, '=') = '\0';

		ATF_CHECK(unsetenv(name) != -1);
		ATF_CHECK(putenv(vars[i]) != -1);
	} while (time(NULL) < endtime);

	return NULL;
}

static void *
thread_setenv(void *arg)
{
	time_t endtime;

	endtime = *(time_t *)arg;
	do {
		size_t i;
		char name[32], value[64];

		i = lrand48() % THREADED_NUM_VARS;
		(void)snprintf(name, sizeof(name), THREADED_VAR_NAME, i);
		(void)snprintf(value, sizeof(value), "setenv %ld", lrand48());

		ATF_CHECK(setenv(name, value, 1) != -1);
	} while (time(NULL) < endtime);

	return NULL;
}

static void *
thread_unsetenv(void *arg)
{
	time_t endtime;

	endtime = *(time_t *)arg;
	do {
		size_t i;
		char name[32];

		i = lrand48() % THREADED_NUM_VARS;
		(void)snprintf(name, sizeof(name), THREADED_VAR_NAME, i);

		ATF_CHECK(unsetenv(name) != -1);
	} while (time(NULL) < endtime);

	return NULL;
}

ATF_TC(clearenv_basic);
ATF_TC_HEAD(clearenv_basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test user clearing environment directly");
}

ATF_TC_BODY(clearenv_basic, tc)
{
	char name[1024], value[1024];

	for (size_t i = 0; i < 1024; i++) {
		snprintf(name, sizeof(name), "crap%zu", i);
		snprintf(value, sizeof(value), "%zu", i);
		ATF_CHECK(setenv(name, value, 1) != -1);
	}

	*environ = NULL;

	for (size_t i = 0; i < 1; i++) {
		snprintf(name, sizeof(name), "crap%zu", i);
		snprintf(value, sizeof(value), "%zu", i);
		ATF_CHECK(setenv(name, value, 1) != -1);
	}

	ATF_CHECK_STREQ(getenv("crap0"), "0");
	ATF_CHECK(getenv("crap1") == NULL);
	ATF_CHECK(getenv("crap2") == NULL);
}

ATF_TC(getenv_basic);
ATF_TC_HEAD(getenv_basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test setenv(3), getenv(3)");
}

ATF_TC_BODY(getenv_basic, tc)
{
	ATF_CHECK(setenv("EVIL", "very=bad", 1) != -1);
	ATF_CHECK_STREQ(getenv("EVIL"), "very=bad");
	ATF_CHECK(getenv("EVIL=very") == NULL);
	ATF_CHECK(unsetenv("EVIL") != -1);
}

ATF_TC(getenv_r_thread);
ATF_TC_HEAD(getenv_r_thread, tc)
{
	char timeout[32];

	atf_tc_set_md_var(tc, "descr", "Test getenv_r(3) with threads");

	(void)snprintf(timeout, sizeof(timeout), "%d", THREADED_RUN_TIME + 5);

	atf_tc_set_md_var(tc, "timeout", timeout);
}

ATF_TC_BODY(getenv_r_thread, tc)
{
	pthread_t threads[THREADED_NUM_THREADS];
	time_t endtime;
	size_t i, j;

	endtime = time(NULL) + THREADED_RUN_TIME;

	for (i = j = 0; j < 2; j++) {

		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_getenv_r,
		    &endtime) == 0);
	}

	for (j = 0; j < i; j++)
		ATF_CHECK(pthread_join(threads[j], NULL) == 0);
}

ATF_TC(putenv_basic);
ATF_TC_HEAD(putenv_basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test putenv(3), getenv(3), unsetenv(3)");
}


ATF_TC_BODY(putenv_basic, tc)
{
	char string[1024];

	snprintf(string, sizeof(string), "crap=true");
	ATF_CHECK(putenv(string) != -1);
	ATF_CHECK_STREQ(getenv("crap"), "true");
	string[1] = 'l';
	ATF_CHECK_STREQ(getenv("clap"), "true");
	ATF_CHECK(getenv("crap") == NULL);
	string[1] = 'r';
	ATF_CHECK(unsetenv("crap") != -1);
	ATF_CHECK(getenv("crap") == NULL);

	ATF_CHECK_ERRNO(EINVAL, putenv(NULL) == -1);
	ATF_CHECK_ERRNO(EINVAL, putenv(__UNCONST("val")) == -1);
	ATF_CHECK_ERRNO(EINVAL, putenv(__UNCONST("=val")) == -1);
}

ATF_TC(putenv_thread);
ATF_TC_HEAD(putenv_thread, tc)
{
	char timeout[32];

	atf_tc_set_md_var(tc, "descr", "Test concurrent access by putenv(3)");

	(void)snprintf(timeout, sizeof(timeout), "%d", THREADED_RUN_TIME + 5);

	atf_tc_set_md_var(tc, "timeout", timeout);
}

ATF_TC_BODY(putenv_thread, tc)
{
	pthread_t threads[THREADED_NUM_THREADS];
	time_t endtime;
	size_t i, j;

	endtime = time(NULL) + THREADED_RUN_TIME;

	for (i = j = 0; j < 2; j++) {

		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_putenv,
		    &endtime) == 0);
	}

	for (j = 0; j < i; j++)
		ATF_CHECK(pthread_join(threads[j], NULL) == 0);
}

ATF_TC(setenv_basic);
ATF_TC_HEAD(setenv_basic, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test setenv(3), getenv(3), unsetenv(3)");
	atf_tc_set_md_var(tc, "timeout", "300");
}

ATF_TC_BODY(setenv_basic, tc)
{
	const size_t numvars = 8192;
	size_t i, offset;
	char name[1024];
	char value[1024];

	offset = lrand48();
	for (i = 0; i < numvars; i++) {
		(void)snprintf(name, sizeof(name), "var%zu",
		    (i * 7 + offset) % numvars);
		(void)snprintf(value, sizeof(value), "value%ld", lrand48());
		ATF_CHECK(setenv(name, value, 1) != -1);
		ATF_CHECK(setenv(name, "foo", 0) != -1);
		ATF_CHECK_STREQ(getenv(name), value);
	}

	offset = lrand48();
	for (i = 0; i < numvars; i++) {
		(void)snprintf(name, sizeof(name), "var%zu",
		    (i * 11 + offset) % numvars);
		ATF_CHECK(unsetenv(name) != -1);
		ATF_CHECK(getenv(name) == NULL);
		ATF_CHECK(unsetenv(name) != -1);
	}

	ATF_CHECK_ERRNO(EINVAL, setenv(NULL, "val", 1) == -1);
	ATF_CHECK_ERRNO(EINVAL, setenv("", "val", 1) == -1);
	ATF_CHECK_ERRNO(EINVAL, setenv("v=r", "val", 1) == -1);
	ATF_CHECK_ERRNO(EINVAL, setenv("var", NULL, 1) == -1);

	ATF_CHECK(setenv("var", "=val", 1) == 0);
	ATF_CHECK_STREQ(getenv("var"), "=val");
}

ATF_TC(setenv_mixed);
ATF_TC_HEAD(setenv_mixed, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "Test mixing setenv(3), unsetenv(3) and putenv(3)");
}


ATF_TC_BODY(setenv_mixed, tc)
{
	char string[32];

	(void)strcpy(string, "mixedcrap=putenv");

	ATF_CHECK(setenv("mixedcrap", "setenv", 1) != -1);
	ATF_CHECK_STREQ(getenv("mixedcrap"), "setenv");
	ATF_CHECK(putenv(string) != -1);
	ATF_CHECK_STREQ(getenv("mixedcrap"), "putenv");
	ATF_CHECK(unsetenv("mixedcrap") != -1);
	ATF_CHECK(getenv("mixedcrap") == NULL);

	ATF_CHECK(putenv(string) != -1);
	ATF_CHECK_STREQ(getenv("mixedcrap"), "putenv");
	ATF_CHECK(setenv("mixedcrap", "setenv", 1) != -1);
	ATF_CHECK_STREQ(getenv("mixedcrap"), "setenv");
	ATF_CHECK(unsetenv("mixedcrap") != -1);
	ATF_CHECK(getenv("mixedcrap") == NULL);
}

ATF_TC(setenv_thread);
ATF_TC_HEAD(setenv_thread, tc)
{
	char timeout[32];

	atf_tc_set_md_var(tc, "descr", "Test concurrent access by setenv(3)");

	(void)snprintf(timeout, sizeof(timeout), "%d", THREADED_RUN_TIME + 5);

	atf_tc_set_md_var(tc, "timeout", timeout);
}

ATF_TC_BODY(setenv_thread, tc)
{
	pthread_t threads[THREADED_NUM_THREADS];
	time_t endtime;
	size_t i, j;

	endtime = time(NULL) + THREADED_RUN_TIME;

	for (i = j = 0; j < 2; j++) {

		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_setenv,
		    &endtime) == 0);
	}

	for (j = 0; j < i; j++)
		ATF_CHECK(pthread_join(threads[j], NULL) == 0);
}

ATF_TC(unsetenv_thread);
ATF_TC_HEAD(unsetenv_thread, tc)
{
	char timeout[32];

	atf_tc_set_md_var(tc, "descr", "Test unsetenv(3) with threads");

	(void)snprintf(timeout, sizeof(timeout), "%d", THREADED_RUN_TIME + 5);

	atf_tc_set_md_var(tc, "timeout", timeout);
}

ATF_TC_BODY(unsetenv_thread, tc)
{
	pthread_t threads[THREADED_NUM_THREADS];
	time_t endtime;
	size_t i, j;

	endtime = time(NULL) + THREADED_RUN_TIME;

	for (i = j = 0; j < 2; j++) {

		ATF_CHECK(pthread_create(&threads[i++], NULL, thread_unsetenv,
		    &endtime) == 0);
	}

	for (j = 0; j < i; j++)
		ATF_CHECK(pthread_join(threads[j], NULL) == 0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, clearenv_basic);
	ATF_TP_ADD_TC(tp, getenv_basic);
	ATF_TP_ADD_TC(tp, getenv_r_thread);
	ATF_TP_ADD_TC(tp, putenv_basic);
	ATF_TP_ADD_TC(tp, putenv_thread);
	ATF_TP_ADD_TC(tp, setenv_basic);
	ATF_TP_ADD_TC(tp, setenv_mixed);
	ATF_TP_ADD_TC(tp, setenv_thread);
	ATF_TP_ADD_TC(tp, unsetenv_thread);

	return atf_no_error();
}
