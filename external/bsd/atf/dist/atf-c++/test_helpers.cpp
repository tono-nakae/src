//
// Automated Testing Framework (atf)
//
// Copyright (c) 2009, 2010 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

extern "C" {
#include <regex.h>
}

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "atf-c++/check.hpp"
#include "atf-c++/config.hpp"
#include "atf-c++/fs.hpp"
#include "atf-c++/macros.hpp"
#include "atf-c++/process.hpp"

#include "test_helpers.hpp"

void
build_check_cxx_o_aux(const atf::fs::path& sfile, const char* failmsg)
{
    std::vector< std::string > optargs;
    optargs.push_back("-I" + atf::config::get("atf_includedir"));

    if (!atf::check::build_cxx_o(sfile, atf::fs::path("test.o"),
                                 atf::process::argv_array(optargs)))
        ATF_FAIL(failmsg);
}

void
build_check_cxx_o(const atf::tests::tc& tc, const char* sfile,
                  const char* failmsg)
{
    const atf::fs::path sfilepath =
        atf::fs::path(tc.get_config_var("srcdir")) / sfile;
    build_check_cxx_o_aux(sfilepath, failmsg);
}

void
header_check(const atf::tests::tc& tc, const char *hdrname)
{
    std::ofstream srcfile("test.c");
    ATF_CHECK(srcfile);
    srcfile << "#include <" << hdrname << ">\n";
    srcfile.close();

    const std::string failmsg = std::string("Header check failed; ") +
        hdrname + " is not self-contained";
    build_check_cxx_o_aux(atf::fs::path("test.c"), failmsg.c_str());
}

atf::fs::path
get_process_helpers_path(const atf::tests::tc& tc)
{
    return atf::fs::path(tc.get_config_var("srcdir")) /
           ".." / "atf-c" / "process_helpers";
}

bool
grep_file(const char* name, const char* regex)
{
    std::ifstream is(name);
    ATF_CHECK(is);

    bool found = false;

    std::string line;
    std::getline(is, line);
    while (!found && is.good()) {
        if (grep_string(line, regex))
            found = true;
        else
            std::getline(is, line);
    }

    return found;
}

bool
grep_string(const std::string& str, const char* regex)
{
    int res;
    regex_t preg;

    std::cout << "Looking for '" << regex << "' in '" << str << "'\n";
    ATF_CHECK(::regcomp(&preg, regex, REG_EXTENDED) == 0);

    res = ::regexec(&preg, str.c_str(), 0, NULL, 0);
    ATF_CHECK(res == 0 || res == REG_NOMATCH);

    ::regfree(&preg);

    return res == 0;
}
