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
* =========================================================================== */

#include <kfs/file.h> /* KFile */
#include <klib/data-buffer.h> /* KDataBuffer */

#include <limits.h> /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef enum {
    ePIStreamed,
    ePIFiled,
    ePIResumed,
} EPrfInfo;

typedef struct {
    EPrfInfo info;
    uint64_t pos;
} PrfInfo;

typedef enum {
    eTextual,
    eBinEol,
    eBin8,
} EType;

typedef struct {
    const  char       * _name; /* don't free ! */
    bool                _vdbcache;
    const  String     *  cache;
    struct KDirectory * _dir;
    KFile             *  file;
    bool                _fatal;
    bool                _loaded;
    char                 tmpName [PATH_MAX];
    uint64_t             pos;
    bool                _resume;
    bool                 invalid;
    EType               _tfType;
    KFile             * _tf;
    uint64_t            _tfPos;
    KDataBuffer         _buf;
    uint32_t            _lastPos;
    KTime_t             _committed;

    PrfInfo info;
} PrfOutFile;

rc_t PrfOutFileInit(
    PrfOutFile * self, bool resume, const char * name, bool vdbcache);
rc_t PrfOutFileMkName(PrfOutFile * self, const String * cache);
rc_t PrfOutFileOpen(PrfOutFile * self, bool force);
bool PrfOutFileIsLoaded(const PrfOutFile * self);
rc_t PrfOutFileCommitTry(PrfOutFile * self);
rc_t PrfOutFileCommitDo(PrfOutFile * self);
rc_t PrfOutFileClose(PrfOutFile * self);
rc_t PrfOutFileWhack(PrfOutFile * self, bool success);
const char * PrfOutFileMkLog(const PrfOutFile * self);

rc_t PrfOutFileConvert(KDirectory * dir, const char * path, bool * recognized);
