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

#include "arg_helper.h"

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

rc_t CC ArgsOptionCount( const struct Args * self, const char * option_name, uint32_t * count );
rc_t CC ArgsOptionValue( const struct Args * self, const char * option_name, uint32_t iteration, const void ** value );

static uint32_t str_2_u32( const char * s, uint32_t dflt ) {
    uint32_t res = dflt;
    if ( NULL != s ) {
        size_t l = string_size( s );
        if ( l > 0 ) {
            char * endptr;
            res = ( uint32_t )strtol( s, &endptr, 0 );
        }
    }
    return res;
}

uint32_t get_env_u32( const char * name, uint32_t dflt ) {
    return str_2_u32( getenv( name ), dflt );
}

const char * get_str_option( const struct Args *args, const char *name, const char * dflt ) {
    const char* res = dflt;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( 0 == rc && count > 0 ) {
        rc = ArgsOptionValue( args, name, 0, (const void**)&res );
        if ( 0 != rc ) { res = dflt; }
    }
    return res;
}

bool get_bool_option( const struct Args *args, const char *name ) {
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    return ( 0 == rc && count > 0 );
}

uint64_t get_uint64_t_option( const struct Args * args, const char *name, uint64_t dflt ) {
    uint64_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( NULL != s ) {
        size_t l = string_size( s );
        if ( l > 0 ) {
            char * endptr;
            res = strtol( s, &endptr, 0 );
        }
    }
    return res;
}

uint32_t get_uint32_t_option( const struct Args * args, const char *name, uint32_t dflt ) {
    return str_2_u32( get_str_option( args, name, NULL ), dflt );
}

size_t get_size_t_option( const struct Args * args, const char *name, size_t dflt ) {
    size_t res = dflt;
    const char * s = get_str_option( args, name, NULL );
    if ( NULL != s ) {
        size_t l = string_size( s );
        if ( l > 0 ) {
            size_t multipl = 1;
            switch( s[ l - 1 ] ) {
                case 'k' :
                case 'K' : multipl = 1024; break;
                case 'm' :
                case 'M' : multipl = 1024 * 1024; break;
                case 'g' :
                case 'G' : multipl = 1024 * 1024 * 1024; break;
            }
            if ( multipl > 1 ) {
                char * src = string_dup( s, l - 1 );
                if ( NULL != src ) {
                    char * endptr;
                    res = strtol( src, &endptr, 0 ) * multipl;
                    free( src );
                }
            } else {
                char * endptr;
                res = strtol( s, &endptr, 0 );
            }
        }
    }
    return res;
}
