/*==============================================================================
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
*/

#include <string>
#include <vector>
#include <iostream>

/*
 * Config file:
 *  The config file consists of lines containing whitespace (ASCII 9 or 32)
 *  seperated fields.  The fields are:
 *      NAME (unique)
 *      SEQID
 *      extra (optional)
 */

class ConfigFile {
    ConfigFile() {}
    ConfigFile(std::istream &is);
public:
    struct Line {
        std::string NAME;
        std::string SEQID;
        std::string EXTRA;
    };
    struct Unparsed {
        unsigned lineno;
        std::string line;
    };
    
    std::vector<Line const> lines;
    std::vector<Unparsed const> unparsed;
    std::string msg;

    ~ConfigFile() {}
    
    void printDescription(std::ostream &, bool detail = false) const;

    static ConfigFile load(std::istream &is) {
        return ConfigFile(is);
    }
    static ConfigFile load(std::string const &filename);
};
