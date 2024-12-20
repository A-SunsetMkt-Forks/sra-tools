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

#ifndef _h_fq_seq_csra_iter_
#define _h_fq_seq_csra_iter_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_cmn_iter_
#include "cmn_iter.h"
#endif

typedef struct fq_seq_csra_opt_t
{
    bool with_read_len;
    bool with_name;
    bool with_read_type;
    bool with_cmp_read;
    bool with_quality;
    bool with_spotgroup;
} fq_seq_csra_opt_t;

typedef struct fq_seq_csra_rec_t
{
    int64_t row_id;
    uint32_t num_alig_id;
    uint64_t prim_alig_id[ 2 ];
    uint32_t num_read_len;
    uint32_t * read_len;
    uint32_t num_read_type;
    uint8_t * read_type;
    String name;
    String read;
    String quality;
    String spotgroup;
} fq_seq_csra_rec_t;

struct fq_seq_csra_iter_t;

rc_t fq_seq_csra_iter_make( const cmn_iter_params_t * params,
                            fq_seq_csra_opt_t opt,
                            struct fq_seq_csra_iter_t ** iter );

void fq_seq_csra_iter_release( struct fq_seq_csra_iter_t * self );

bool fq_seq_csra_iter_get_data( struct fq_seq_csra_iter_t * self,
                                fq_seq_csra_rec_t * rec, rc_t * rc );

#ifdef __cplusplus
}
#endif

#endif
