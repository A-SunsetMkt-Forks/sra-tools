/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Project:
 *  sratools command line tool
 *
 * Purpose:
 *  Main entry point for tool and initial dispatch
 *
 */

#pragma once
#include <cstdlib>

#include "../../shared/toolkit.vers.h"
#include "debug.hpp"
#include "util.hpp"

namespace sratools {

struct ToolPath {
#if WINDOWS
    static char constexpr seperator = '\\';
#else
    static char constexpr seperator = '/';
#endif
private:
    std::string path_;
    std::string basename_;
    std::string version_;

    static std::string get_path(std::string const &pathname)
    {
        auto const sep = pathname.find_last_of(ToolPath::seperator);
        return (sep == std::string::npos) ? std::string() : pathname.substr(0, sep);
    }

    static std::string get_exec_path(std::string const &argv0, char *extra[]);

    ToolPath(std::string const &path, std::string const &name, std::string const &vers)
    : path_(path), basename_(name), version_(vers)
    {}
    ToolPath(std::string const &argv0, char *extra[]);

    friend ToolPath makeToolPath(char const *argv0, char *extra[]);
public:
    std::string const &path() const { return path_; }
    std::string const &basename() const { return basename_; }
    std::string const &version() const { return version_; }
    std::string fullpath() const {
#if WINDOWS
        return path_ + ToolPath::seperator + basename_ + ".exe";
#else // POSIX
        auto result = path_ + ToolPath::seperator + basename_;
        if (version_.size() > 0) result += '.' + version_;
        return result;
#endif
    }
    bool executable() const {
        if (logging_state::testing_level() > 0)
            return true; ///< since we aren't going to ever actually try to run the thing, might as well let it slide.
#if WINDOWS
        return true; ///< yeah Windows really doesn't know!!!
#else // POSIX
        return access(fullpath().c_str(), X_OK) == 0;
#endif
    }

    static std::string toolkit_version( void )
    {
        auto const major = ((TOOLKIT_VERS) >> 24) & 0xFF;
        auto const minor = ((TOOLKIT_VERS)>> 16) & 0xFF;
        auto const subvers = ( TOOLKIT_VERS ) & 0xFFFF;
        return std::to_string(major) + '.' + std::to_string(minor) + '.' + std::to_string(subvers);
    }

    ToolPath getPathFor(std::string const &name) const
    {
        auto result = ToolPath(path(), name, version());
#if DEBUG || _DEBUGGING
        {
            if (result.executable())
                return result;
        }
        if (ends_with("-orig", name))
            return getPathFor(name.substr(0, name.size() - 5));
        result = ToolPath(path(), name, "");
#endif
        return result;
    }
};

} // namespace sratools
