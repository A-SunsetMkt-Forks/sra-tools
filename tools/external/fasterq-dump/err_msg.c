/*===========================================================================
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
*/

#include "err_msg.h"

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

rc_t ErrMsg( const char * fmt, ... ) {
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( 0 == rc ) {
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    }
    va_end( list );
    return rc;
}

rc_t InfoMsg( const char * fmt, ... ) {
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( 0 == rc ) {
        rc = pLogMsg( klogInfo, "$(M)", "M=%s", buffer );
    }
    va_end( list );
    return rc;
}
