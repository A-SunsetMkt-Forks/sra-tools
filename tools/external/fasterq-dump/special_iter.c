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

#include "special_iter.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

typedef struct special_iter_t {
    struct cmn_iter_t * cmn;
    uint32_t prim_alig_id, cmp_read_id, spot_group_id;
} special_iter_t;

void destroy_special_iter( struct special_iter_t * iter ) {
    if ( NULL != iter ) {
        cmn_iter_release( iter -> cmn );
        free( ( void * ) iter );
    }
}

rc_t make_special_iter( cmn_iter_params_t * params, struct special_iter_t ** iter ) {
    rc_t rc = 0;
    special_iter_t * i = calloc( 1, sizeof * i );
    if ( NULL == i ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_special_iter.calloc( %d ) -> %R", ( sizeof * i ), rc );
    } else {
        rc = cmn_iter_make( params, "SEQUENCE", &( i -> cmn ) );
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( i -> cmn, "PRIMARY_ALIGNMENT_ID", &( i -> prim_alig_id ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( i -> cmn, "CMP_READ", &( i -> cmp_read_id ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( i -> cmn, "SPOT_GROUP", &( i -> spot_group_id ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_detect_range( i -> cmn, i -> prim_alig_id );
        }
        if ( 0 != rc ) {
            destroy_special_iter( i );
        } else {
            *iter = i;
        }
    }
    return rc;
}

bool get_from_special_iter( struct special_iter_t * iter, special_rec_t * rec, rc_t * rc ) {
    bool res = cmn_iter_get_next( iter -> cmn, rc );
    if ( res ) {
        rec -> row_id = cmn_iter_get_row_id( iter -> cmn );
        *rc = cmn_iter_read_uint64_array( iter -> cmn, iter -> prim_alig_id,
                                          rec -> prim_alig_id, 2, &( rec -> num_reads ) );
        if ( 0 == *rc ) {
            *rc = cmn_iter_read_String( iter -> cmn, iter -> cmp_read_id, &( rec -> cmp_read ) );
        }
        if ( 0 == *rc ) {
            *rc = cmn_iter_read_String( iter -> cmn, iter -> spot_group_id, &( rec -> spot_group ) );
        }
    }
    return res;

}

uint64_t get_row_count_of_special_iter( struct special_iter_t * iter ) {
    return cmn_iter_get_row_count( iter -> cmn );
}
