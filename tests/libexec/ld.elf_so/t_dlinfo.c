/*	$NetBSD: t_dlinfo.c,v 1.2 2010/11/03 16:10:23 christos Exp $	*/

/*
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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

#include <atf-c.h>
#include <dlfcn.h>
#include <link_elf.h>

#include "../../h_macros.h"

ATF_TC(rtld_dlinfo_linkmap_self);
ATF_TC_HEAD(rtld_dlinfo_linkmap_self, tc)
{
	atf_tc_set_md_var(tc, "descr", "dlinfo with RTLD_SELF handle works");
}
ATF_TC_BODY(rtld_dlinfo_linkmap_self, tc)
{
	struct link_map *map;
	int rv;

	rv = dlinfo(RTLD_SELF, RTLD_DI_LINKMAP, &map);
	ATF_CHECK_EQ(rv, 0);
	ATF_CHECK((strstr(map->l_name, "t_dlinfo") != NULL));
}

ATF_TC(rtld_dlinfo_linkmap_inval);
ATF_TC_HEAD(rtld_dlinfo_linkmap_inval, tc)
{
	atf_tc_set_md_var(tc, "descr", "dlinfo with invalid handle fails");
}
ATF_TC_BODY(rtld_dlinfo_linkmap_inval, tc)
{
	void *v;
	int rv;

	rv = dlinfo(NULL, RTLD_DI_LINKMAP, &v);
	ATF_CHECK_EQ(rv, -1);
}

ATF_TC(rtld_dlinfo_linkmap_dlopen);
ATF_TC_HEAD(rtld_dlinfo_linkmap_dlopen, tc)
{
	atf_tc_set_md_var(tc, "descr", "dlinfo dlopen'd handle works");
}
ATF_TC_BODY(rtld_dlinfo_linkmap_dlopen, tc)
{
	struct link_map *map;
	void *handle;
	int rv;

	handle = dlopen("libutil.so", RTLD_LAZY);
	ATF_CHECK(handle);

	rv = dlinfo(handle, RTLD_DI_LINKMAP, &map);
	ATF_CHECK_EQ(rv, 0);
	ATF_CHECK((strstr(map->l_name, "libutil.so") != NULL));
	dlclose(handle);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, rtld_dlinfo_linkmap_self);
	ATF_TP_ADD_TC(tp, rtld_dlinfo_linkmap_inval);
	ATF_TP_ADD_TC(tp, rtld_dlinfo_linkmap_dlopen);
	return 0;
}
