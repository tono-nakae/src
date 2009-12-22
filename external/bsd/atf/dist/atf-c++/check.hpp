//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007, 2008, 2009 The NetBSD Foundation, Inc.
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

#if !defined(_ATF_CXX_CHECK_HPP_)
#define _ATF_CXX_CHECK_HPP_

extern "C" {
#include "atf-c/check.h"
}

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "atf-c++/fs.hpp"
#include "atf-c++/text.hpp"
#include "atf-c++/utils.hpp"

namespace atf {

namespace process {
class argv_array;
} // namespace process

namespace check {

// ------------------------------------------------------------------------
// The "check_result" class.
// ------------------------------------------------------------------------

//!
//! \brief A class that contains results of executed command.
//!
//! The check_result class holds information about results
//! of executing arbitrary command and manages files containing
//! its output.
//!
class check_result {
    //!
    //! \brief Internal representation of a result.
    //!
    atf_check_result_t m_result;

    //!
    //! \brief Copy of m_result.m_argv but in a C++ collection.
    //!
    std::vector< std::string > m_argv;

    //!
    //! \brief Constructs a results object and grabs ownership of the
    //! parameter passed in.
    //!
    check_result(const atf_check_result_t* result);

    friend check_result test_constructor(const char* const*);
    friend check_result exec(const atf::process::argv_array&);

public:
    //!
    //! \brief Destroys object and removes all managed files.
    //!
    ~check_result(void);

    //!
    //! \brief Returns the argument list used by the command that caused
    //! this result.
    //!
    const std::vector< std::string >& argv(void) const;

    //!
    //! \brief Returns whether the command exited correctly or not.
    //!
    bool exited(void) const;

    //!
    //! \brief Returns command's exit status.
    //!
    int exitcode(void) const;

    //!
    //! \brief Returns the path to file contaning command's stdout.
    //!
    const atf::fs::path stdout_path(void) const;

    //!
    //! \brief Returns the path to file contaning command's stderr.
    //!
    const atf::fs::path stderr_path(void) const;
};

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

bool build_c_o(const atf::fs::path&, const atf::fs::path&,
               const atf::process::argv_array&);
bool build_cpp(const atf::fs::path&, const atf::fs::path&,
               const atf::process::argv_array&);
bool build_cxx_o(const atf::fs::path&, const atf::fs::path&,
                 const atf::process::argv_array&);
check_result exec(const atf::process::argv_array&);

// Useful for testing only.
check_result test_constructor(void);

} // namespace check
} // namespace atf

#endif // !defined(_ATF_CXX_CHECK_HPP_)
