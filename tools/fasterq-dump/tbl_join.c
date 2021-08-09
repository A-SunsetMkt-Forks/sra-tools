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
#include "tbl_join.h"
#include "fastq_iter.h"
#include "cleanup_task.h"
#include "join_results.h"
#include "progress_thread.h"

#include <klib/out.h>
#include <kproc/thread.h>
#include <insdc/insdc.h>

static bool filter1( join_stats * stats,
                     const fastq_rec * rec,
                     const join_options * jo )
{
    bool process;

    if ( jo -> min_read_len > 0 )
    {
        process = ( rec -> read . len >= jo -> min_read_len );
    }
    else
    {
        process = ( rec -> read . len > 0 );
    }

    if ( !process )
    {
        stats -> reads_too_short++;
    }
    else
    {
        if ( jo -> skip_tech )
        {
            process = ( rec -> read_type[ 0 ] & READ_TYPE_BIOLOGICAL ) == READ_TYPE_BIOLOGICAL;
            if ( !process ) { stats -> reads_technical++; }
        }
    }
    return process;
}

static bool filter( join_stats * stats,
                    const fastq_rec * rec,
                    const join_options * jo,
                    uint32_t read_id_0 )
{
    bool process = true;
    if ( jo -> skip_tech && rec -> num_read_type > read_id_0 )
    {
        process = ( rec -> read_type[ read_id_0 ] & READ_TYPE_BIOLOGICAL ) == READ_TYPE_BIOLOGICAL;
        if ( !process ) { stats -> reads_technical++; }
    }
    
    if ( process )
    {
        if ( jo -> min_read_len > 0 )
        {
            process = ( rec -> read_len[ read_id_0 ] >= jo -> min_read_len );
        }
        else
        {
            process = ( rec -> read_len[ read_id_0 ] > 0 );
        }
        if ( !process ) { stats -> reads_too_short++; }
    }
    return process;
}

static rc_t print_fastq_1_read( join_stats * stats,
                                struct join_results * results,
                                const fastq_rec * rec,
                                const join_options * jo,
                                uint32_t dst_id,
                                uint32_t read_id )
{
    rc_t rc = 0;
   
    if ( rec -> read . len != rec -> quality . len )
    {
        ErrMsg( "row #%ld : READ.len(%u) != QUALITY.len(%u) (A)\n", rec -> row_id, rec -> read . len, rec -> quality . len );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    if ( filter1( stats, rec, jo ) ) /* above */
    {
        if ( join_results_match( results, &( rec -> read ) ) ) /* join_results.c */
        {
            rc = join_results_print_fastq_v1( results,
                                              rec -> row_id,
                                              dst_id,
                                              read_id,
                                              jo -> rowid_as_name ? NULL : &( rec -> name ),
                                              &( rec -> read ),
                                              &( rec -> quality ) ); /* join_results.c */
            if ( 0 == rc ) { stats -> reads_written++; }
        }
    }
    return rc;
}

static rc_t print_fasta_1_read( join_stats * stats,
                                struct join_results * results,
                                const fastq_rec * rec,
                                const join_options * jo,
                                uint32_t dst_id,
                                uint32_t read_id )
{
    rc_t rc = 0;

    if ( filter1( stats, rec, jo ) )
    {
        if ( join_results_match( results, &( rec -> read ) ) )  /* join_results.c */
        {
            rc = join_results_print_fastq_v1( results,
                                              rec -> row_id,
                                              dst_id,
                                              read_id,
                                              jo -> rowid_as_name ? NULL : &( rec -> name ),
                                              &( rec -> read ),
                                              NULL ); /* join_results.c */
            if ( 0 == rc ) { stats -> reads_written++; }
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */
static rc_t print_fastq_n_reads_split( join_stats * stats,
                                       struct join_results * results,
                                       const fastq_rec * rec,
                                       const join_options * jo )
{
    rc_t rc = 0;
    String R, Q;
    uint32_t read_id_0 = 0;
    uint32_t offset = 0;
    uint32_t read_len_sum = 0;
    
    if ( rec -> read . len != rec -> quality . len )
    {
        ErrMsg( "row #%ld : READ.len(%u) != QUALITY.len(%u) (B)\n", rec -> row_id, rec -> read . len, rec -> quality . len );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    while ( read_id_0 < rec -> num_read_len )
    {
        read_len_sum += rec -> read_len[ read_id_0++ ];
    }

    if ( rec -> read . len != read_len_sum )
    {
        ErrMsg( "row #%ld : READ.len(%u) != sum(READ_LEN)(%u) (C)\n", rec -> row_id, rec -> read . len, read_len_sum );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    read_id_0 = 0;

    while ( 0 == rc && read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            if ( filter( stats, rec, jo, read_id_0 ) )
            {
                R . addr = &rec -> read . addr[ offset ];
                R . size = rec -> read_len[ read_id_0 ];
                R . len  = ( uint32_t )R . size;

                if ( join_results_match( results, &R ) ) /* join_results.c */
                {
                    Q . addr = &rec -> quality . addr[ offset ];
                    Q . size = rec -> read_len[ read_id_0 ];
                    Q . len  = ( uint32_t )Q . size;

                    if ( join_results_match( results, &( rec -> read ) ) ) /* join_results.c */
                    {
                        rc = join_results_print_fastq_v1( results,
                                                        rec -> row_id,
                                                        0,
                                                        read_id_0 + 1,
                                                        jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                        &R,
                                                        &Q ); /* join_results.c */
                    }
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
            }
            offset += rec -> read_len[ read_id_0 ];
        }
        else { stats -> reads_zero_length++; }
        read_id_0++;
    }
    return rc;
}

static rc_t print_fasta_n_reads_split( join_stats * stats,
                                       struct join_results * results,
                                       const fastq_rec * rec,
                                       const join_options * jo )
{
    rc_t rc = 0;
    String R;
    uint32_t read_id_0 = 0;
    uint32_t offset = 0;
    uint32_t read_len_sum = 0;

    while ( read_id_0 < rec -> num_read_len )
    {
        read_len_sum += rec -> read_len[ read_id_0++ ];
    }

    if ( rec -> read . len != read_len_sum )
    {
        ErrMsg( "row #%ld : READ.len(%u) != sum(READ_LEN)(%u) (C)\n", rec -> row_id, rec -> read . len, read_len_sum );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    read_id_0 = 0;

    while ( 0 == rc && read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            if ( filter( stats, rec, jo, read_id_0 ) )
            {
                R . addr = &rec -> read . addr[ offset ];
                R . size = rec -> read_len[ read_id_0 ];
                R . len  = ( uint32_t )R . size;

                if ( join_results_match( results, &R ) ) /* join_results.c */
                {
                    if ( join_results_match( results, &( rec -> read ) ) )
                    {
                        rc = join_results_print_fastq_v1( results,
                                                        rec -> row_id,
                                                        0,
                                                        read_id_0 + 1,
                                                        jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                        &R,
                                                        NULL ); /* join_results.c */
                    }
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
            }
            offset += rec -> read_len[ read_id_0 ];
        }
        else { stats -> reads_zero_length++; }
        read_id_0++;
    }
    return rc;
}

static rc_t print_fastq_n_reads_split_file( join_stats * stats,
                                            struct join_results * results,
                                            const fastq_rec * rec,
                                            const join_options * jo )
{
    rc_t rc = 0;
    String R, Q;
    uint32_t read_id_0 = 0;
    uint32_t write_id_1 = 1;
    uint32_t offset = 0;
    uint32_t read_len_sum = 0;
    
    if ( rec -> read . len != rec -> quality . len )
    {
        ErrMsg( "row #%ld : READ.len(%u) != QUALITY.len(%u) (D)\n", rec -> row_id, rec -> read . len, rec -> quality . len );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    while ( read_id_0 < rec -> num_read_len )
    {
        read_len_sum += rec -> read_len[ read_id_0++ ];
    }

    if ( rec -> read . len != read_len_sum )
    {
        ErrMsg( "row #%ld : READ.len(%u) != sum(READ_LEN)(%u) (E)\n", rec -> row_id, rec -> read . len, read_len_sum );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    read_id_0 = 0;

    while ( 0 == rc && read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            bool process = filter( stats, rec, jo, read_id_0 ); /* above */
            if ( process )
            {
                R . addr = &rec -> read . addr[ offset ];
                R . size = rec -> read_len[ read_id_0 ];
                R . len  = ( uint32_t )R . size;

                if ( join_results_match( results, &R ) ) /* join_results.c */
                {
                    Q . addr = &rec -> quality . addr[ offset ];
                    Q . size = rec -> read_len[ read_id_0 ];
                    Q . len  = ( uint32_t )Q . size;

                    rc = join_results_print_fastq_v1( results,
                                                      rec -> row_id,
                                                      write_id_1,
                                                      read_id_0 + 1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &R,
                                                      &Q ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
            }
            offset += rec -> read_len[ read_id_0 ];
        }
        else { stats -> reads_zero_length++; }

        write_id_1++;
        read_id_0++;
    }
    return rc;
}

static rc_t print_fasta_n_reads_split_file( join_stats * stats,
                                            struct join_results * results,
                                            const fastq_rec * rec,
                                            const join_options * jo )
{
    rc_t rc = 0;
    String R;
    uint32_t read_id_0 = 0;
    uint32_t write_id_1 = 1;
    uint32_t offset = 0;
    uint32_t read_len_sum = 0;

    while ( read_id_0 < rec -> num_read_len )
    {
        read_len_sum += rec -> read_len[ read_id_0++ ];
    }

    if ( rec -> read . len != read_len_sum )
    {
        ErrMsg( "row #%ld : READ.len(%u) != sum(READ_LEN)(%u) (E)\n", rec -> row_id, rec -> read . len, read_len_sum );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }
    read_id_0 = 0;
    while ( 0 == rc && read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            bool process = filter( stats, rec, jo, read_id_0 );
            if ( process )
            {
                R . addr = &rec -> read . addr[ offset ];
                R . size = rec -> read_len[ read_id_0 ];
                R . len  = ( uint32_t )R . size;

                if ( join_results_match( results, &R ) )
                {
                    rc = join_results_print_fastq_v1( results,
                                                      rec -> row_id,
                                                      write_id_1,
                                                      read_id_0 + 1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &R,
                                                      NULL );
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
            }
            offset += rec -> read_len[ read_id_0 ];
        }
        else { stats -> reads_zero_length++; }
        write_id_1++;
        read_id_0++;
    }
    return rc;
}

static rc_t print_fastq_n_reads_split_3( join_stats * stats,
                                         struct join_results * results,
                                         const fastq_rec * rec,
                                         const join_options * jo )
{
    rc_t rc = 0;
    String R, Q;
    uint32_t read_id_0 = 0;
    uint32_t write_id_1 = 1;
    uint32_t valid_reads = 0;
    uint32_t valid_bio_reads = 0;
    uint32_t offset = 0;
    uint32_t read_len_sum = 0;
    
    if ( rec -> read . len != rec -> quality . len )
    {
        ErrMsg( "row #%ld : READ.len(%u) != QUALITY.len(%u) (F)\n", rec -> row_id, rec -> read . len, rec -> quality . len );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    while ( read_id_0 < rec -> num_read_len )
    {
        read_len_sum += rec -> read_len[ read_id_0 ];
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            valid_reads++;
            if ( ( rec -> read_type[ read_id_0 ] & READ_TYPE_BIOLOGICAL ) == READ_TYPE_BIOLOGICAL )
            {
                if ( jo -> min_read_len > 0 )
                {
                    if ( rec -> read_len[ read_id_0 ] >= jo -> min_read_len ) { valid_bio_reads++; }
                }
                else { valid_bio_reads++; }
            }
        }
        read_id_0++;
    }

    if ( rec -> read . len != read_len_sum )
    {
        ErrMsg( "row #%ld : READ.len(%u) != sum(READ_LEN)(%u) (G)\n", rec -> row_id, rec -> read . len, read_len_sum );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    if ( 0 == valid_reads ) { return rc; }

    read_id_0 = 0;
    while ( 0 == rc && read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            bool process = filter( stats, rec, jo, read_id_0 ); /* above */
            if ( process )
            {
                R . addr = &rec -> read . addr[ offset ];
                R . size = rec -> read_len[ read_id_0 ];
                R . len  = ( uint32_t )R . size;

                if ( join_results_match( results, &R ) )
                {
                    Q . addr = &rec -> quality . addr[ offset ];
                    Q . size = rec -> read_len[ read_id_0 ];
                    Q . len  = ( uint32_t )Q . size;

                    if ( valid_bio_reads < 2 ) { write_id_1 = 0; }

                    rc = join_results_print_fastq_v1( results,
                                                      rec -> row_id,
                                                      write_id_1,
                                                      read_id_0 + 1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &R,
                                                      &Q );
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
                if ( write_id_1 > 0 ) { write_id_1++; }
            }
            offset += rec -> read_len[ read_id_0 ];
        }
        else { stats -> reads_zero_length++; }
        read_id_0++;
    }
    return rc;
}

static rc_t print_fasta_n_reads_split_3( join_stats * stats,
                                         struct join_results * results,
                                         const fastq_rec * rec,
                                         const join_options * jo )
{
    rc_t rc = 0;
    String R;
    uint32_t read_id_0 = 0;
    uint32_t write_id_1 = 1;
    uint32_t valid_reads = 0;
    uint32_t valid_bio_reads = 0;
    uint32_t offset = 0;
    uint32_t read_len_sum = 0;

    while ( read_id_0 < rec -> num_read_len )
    {
        read_len_sum += rec -> read_len[ read_id_0 ];
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            valid_reads++;
            if ( ( rec -> read_type[ read_id_0 ] & READ_TYPE_BIOLOGICAL ) == READ_TYPE_BIOLOGICAL )
            {
                if ( jo -> min_read_len > 0 )
                {
                    if ( rec -> read_len[ read_id_0 ] >= jo -> min_read_len ) { valid_bio_reads++; }
                }
                else { valid_bio_reads++; }
            }
        }
        read_id_0++;
    }

    if ( rec -> read . len != read_len_sum )
    {
        ErrMsg( "row #%ld : READ.len(%u) != sum(READ_LEN)(%u) (G)\n", rec -> row_id, rec -> read . len, read_len_sum );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
        {
            return SILENT_RC( rcApp, rcNoTarg, rcReading, rcItem, rcInvalid );
        }
    }

    if ( 0 == valid_reads ) { return rc; }

    read_id_0 = 0;
    while ( 0 == rc && read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            bool process = filter( stats, rec, jo, read_id_0 ); /* above */
            if ( process )
            {
                R . addr = &rec -> read . addr[ offset ];
                R . size = rec -> read_len[ read_id_0 ];
                R . len  = ( uint32_t )R . size;

                if ( join_results_match( results, &R ) )
                {
                    if ( valid_bio_reads < 2 ) { write_id_1 = 0; }
                    rc = join_results_print_fastq_v1( results,
                                                      rec -> row_id,
                                                      write_id_1,
                                                      read_id_0 + 1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &R,
                                                      NULL );
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
                if ( write_id_1 > 0 ) { write_id_1++; }
            }
            offset += rec -> read_len[ read_id_0 ];
        }
        else { stats -> reads_zero_length++; }
        read_id_0++;
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t perform_fastq_whole_spot_join( cmn_params * cp,
                                join_stats * stats,
                                const char * tbl_name,
                                struct join_results * results,
                                struct bg_progress * progress,
                                const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = ( jo -> min_read_len > 0 );
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = false;
    opt . with_cmp_read = false;
    opt . with_quality = true;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) { ErrMsg( "perform_fastq_join().make_fastq_iter() -> %R", rc ); }
    else
    {
        rc_t rc_iter;
        fastq_rec rec;
        join_options local_opt =
        {
            jo -> rowid_as_name,
            false,
            jo -> print_read_nr,
            jo -> print_name,
            jo -> terminate_on_invalid,
            jo -> min_read_len,
            jo -> filter_bases
        };

        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            stats -> spots_read++;
            stats -> reads_read += rec . num_read_len;

            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                rc = print_fastq_1_read( stats, results, &rec, &local_opt, 1, 1 ); /* above */
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t perform_fastq_split_spot_join( cmn_params * cp,
                                      join_stats * stats,
                                      const char * tbl_name,
                                      struct join_results * results,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = jo -> skip_tech;
    opt . with_cmp_read = false;
    opt . with_quality = true;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 == rc )
    {
        rc_t rc_iter;
        fastq_rec rec;
        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_read_len;

                if ( 1 == rec . num_read_len )
                {
                    rc = print_fastq_1_read( stats, results, &rec, jo, 0, 1 ); /* above */
                }
                else
                {
                    rc = print_fastq_n_reads_split( stats, results, &rec, jo ); /* above */
                }

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter );
    }
    else { ErrMsg( "make_fastq_iter() -> %R", rc ); }
    return rc;
}

static rc_t perform_fastq_split_file_join( cmn_params * cp,
                                      join_stats * stats,
                                      const char * tbl_name,
                                      struct join_results * results,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = jo -> skip_tech;
    opt . with_cmp_read = false;
    opt . with_quality = true;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 == rc )
    {
        rc_t rc_iter;
        fastq_rec rec;
        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_read_len;

                if ( 1 == rec . num_read_len )
                {
                    rc = print_fastq_1_read( stats, results, &rec, jo, 1, 1 ); /* above */
                }
                else
                {
                    rc = print_fastq_n_reads_split_file( stats, results, &rec, jo ); /* above */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter );
    }
    else { ErrMsg( "make_fastq_iter() -> %R", rc ); }
    return rc;
}

static rc_t perform_fastq_split_3_join( cmn_params * cp,
                                      join_stats * stats,
                                      const char * tbl_name,
                                      struct join_results * results,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = false;
    opt . with_quality = true;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 == rc )
    {
        rc_t rc_iter;
        fastq_rec rec;
        join_options local_opt =
        {
            jo -> rowid_as_name,
            true,
            jo -> print_read_nr,
            jo -> print_name,
            jo -> terminate_on_invalid,
            jo -> min_read_len,
            jo -> filter_bases
        };

        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_read_len;

                if ( 1 == rec . num_read_len )
                {
                    rc = print_fastq_1_read( stats, results, &rec, &local_opt, 0, 1 ); /* above */
                }
                else
                {
                    rc = print_fastq_n_reads_split_3( stats, results, &rec, &local_opt ); /* above */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter );
    }
    else { ErrMsg( "make_fastq_iter() -> %R", rc ); }
    return rc;
}

/* just like perform_fastq_whole_spot_join(), but without quality and calling print_fasta_1_read() */
static rc_t perform_fasta_whole_spot_join( cmn_params * cp,
                                join_stats * stats,
                                const char * tbl_name,
                                struct join_results * results,
                                struct bg_progress * progress,
                                const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;   /* fastq_iter.h / fastq_iter.c */
    fastq_iter_opt opt;             /* fastq_iter.h */
    opt . with_read_len = ( jo -> min_read_len > 0 );
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = false;
    opt . with_cmp_read = false;
    opt . with_quality = false;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) { ErrMsg( "perform_fasta_join().make_fastq_iter() -> %R", rc ); }
    else
    {
        rc_t rc_iter;
        fastq_rec rec;
        join_options local_opt =
        {
            jo -> rowid_as_name,
            false,
            jo -> print_read_nr,
            jo -> print_name,
            jo -> terminate_on_invalid,
            jo -> min_read_len,
            jo -> filter_bases
        };

        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            stats -> spots_read++;
            stats -> reads_read += rec . num_read_len;

            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                rc = print_fasta_1_read( stats, results, &rec, &local_opt, 1, 1 ); /* above */
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter );
    }
    return rc;
}

static rc_t perform_fasta_split_spot_join( cmn_params * cp,
                                      join_stats * stats,
                                      const char * tbl_name,
                                      struct join_results * results,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = jo -> skip_tech;
    opt . with_cmp_read = false;
    opt . with_quality = false;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 == rc )
    {
        rc_t rc_iter;
        fastq_rec rec;
        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_read_len;

                if ( 1 == rec . num_read_len )
                {
                    rc = print_fasta_1_read( stats, results, &rec, jo, 0, 1 ); /* above */
                }
                else
                {
                    rc = print_fasta_n_reads_split( stats, results, &rec, jo ); /* above */
                }

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter );
    }
    else { ErrMsg( "make_fastq_iter() -> %R", rc ); }
    return rc;
}

static rc_t perform_fasta_split_file_join( cmn_params * cp,
                                      join_stats * stats,
                                      const char * tbl_name,
                                      struct join_results * results,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = jo -> skip_tech;
    opt . with_cmp_read = false;
    opt . with_quality = false;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 == rc )
    {
        rc_t rc_iter;
        fastq_rec rec;
        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_read_len;

                if ( 1 == rec . num_read_len )
                {
                    rc = print_fasta_1_read( stats, results, &rec, jo, 1, 1 ); /* above */
                }
                else
                {
                    rc = print_fasta_n_reads_split_file( stats, results, &rec, jo ); /* above */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter );
    }
    else { ErrMsg( "make_fastq_iter() -> %R", rc ); }
    return rc;
}

static rc_t perform_fasta_split_3_join( cmn_params * cp,
                                      join_stats * stats,
                                      const char * tbl_name,
                                      struct join_results * results,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = false;
    opt . with_quality = false;

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( 0 == rc )
    {
        rc_t rc_iter;
        fastq_rec rec;
        join_options local_opt =
        {
            jo -> rowid_as_name,
            true,
            jo -> print_read_nr,
            jo -> print_name,
            jo -> terminate_on_invalid,
            jo -> min_read_len,
            jo -> filter_bases
        };

        while ( 0 == rc && get_from_fastq_sra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) /* fastq-iter.c */
        {
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_read_len;

                if ( 1 == rec . num_read_len )
                {
                    rc = print_fasta_1_read( stats, results, &rec, &local_opt, 0, 1 ); /* above */
                }
                else
                {
                    rc = print_fasta_n_reads_split_3( stats, results, &rec, &local_opt ); /* above */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
        if ( 0 != rc ) { set_quitting(); /* helper.c */ }
        destroy_fastq_sra_iter( iter ); /* fastq-iter.c */
    }
    else { ErrMsg( "make_fastq_iter() -> %R", rc ); }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

typedef struct join_thread_data
{
    char part_file[ 4096 ];

    join_stats stats;

    KDirectory * dir;
    const VDBManager * vdb_mgr;

    const char * accession_short;
    const char * accession_path;
    const char * tbl_name;
    struct bg_progress * progress;
    struct temp_registry * registry;
    KThread * thread;

    int64_t first_row;
    uint64_t row_count;
    size_t cur_cache;
    size_t buf_size;
    format_t fmt;
    const join_options * join_options;

} join_thread_data;

static rc_t CC cmn_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    join_thread_data * jtd = data;
    struct join_results * results = NULL;

    rc = make_join_results( jtd -> dir,
                            &results,
                            jtd -> registry,
                            jtd -> part_file,
                            jtd -> accession_short,
                            jtd -> buf_size,
                            4096,
                            jtd -> join_options -> print_read_nr,
                            jtd -> join_options -> print_name,
                            jtd -> join_options -> filter_bases ); /* join_results.c */

    if ( 0 == rc && NULL != results )
    {
        cmn_params cp = { jtd -> dir, jtd -> vdb_mgr, 
                          jtd -> accession_short, jtd -> accession_path,
                          jtd -> first_row, jtd -> row_count, jtd -> cur_cache };
        switch( jtd -> fmt )
        {
            case ft_fastq_whole_spot : rc = perform_fastq_whole_spot_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */

            case ft_fastq_split_spot : rc = perform_fastq_split_spot_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */

            case ft_fastq_split_file : rc = perform_fastq_split_file_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */

            case ft_fastq_split_3   : rc = perform_fastq_split_3_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */

            case ft_fasta_whole_spot : rc = perform_fasta_whole_spot_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */

            case ft_fasta_split_spot :  rc = perform_fasta_split_spot_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */

            case ft_fasta_split_file : rc = perform_fasta_split_file_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */;

            case ft_fasta_split_3 :  rc = perform_fasta_split_3_join( &cp,
                                            &jtd -> stats,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> progress,
                                            jtd -> join_options ); break; /* above */;

            case ft_unknown : break;                /* this should not happen */
            case ft_special : break;                /* this also should not happen */
            case ft_fasta_us_split_spot : break;    /* and neither should this */
        }
        destroy_join_results( results ); /* join_results.c */
    }
    return rc;
}

static rc_t extract_sra_row_count( KDirectory * dir,
                                   const VDBManager * vdb_mgr,
                                   const char * accession_short,
                                   const char * accession_path,
                                   const char * tbl_name,
                                   size_t cur_cache,
                                   uint64_t * res )
{
    cmn_params cp = { dir, vdb_mgr, accession_short, accession_path, 0, 0, cur_cache }; /* helper.h */
    struct fastq_sra_iter * iter; 
    fastq_iter_opt opt = { false, false, false, false };
    rc_t rc = make_fastq_sra_iter( &cp, opt, tbl_name, &iter ); /* fastq_iter.c */
    if ( 0 == rc )
    {
        *res = get_row_count_of_fastq_sra_iter( iter ); /* fastq_iter.c */
        destroy_fastq_sra_iter( iter ); /* fastq_iter.c */
    }
    return rc;
}

static rc_t is_column_name_present( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    const char * tbl_name,
                    bool * presence )
{
    rc_t rc;
    if ( NULL == tbl_name )
    {
        rc = cmn_check_tbl_column( dir, vdb_mgr, accession_short, accession_path,
                                    "NAME", presence ); /* cmn_iter.c */
    }
    else
    {
        rc = cmn_check_db_column( dir, vdb_mgr, accession_short, accession_path, tbl_name,
                                    "NAME", presence ); /* cmn_iter.c */
    }
    return rc;
}

static void correct_join_options( join_options * dst, const join_options * src, bool name_column_present )
{
    dst -> rowid_as_name = name_column_present ? src -> rowid_as_name : true;
    dst -> skip_tech = src -> skip_tech;
    dst -> print_read_nr = src -> print_read_nr;
    dst -> print_name = name_column_present;
    dst -> min_read_len = src -> min_read_len;
    dst -> filter_bases = src -> filter_bases;
    dst -> terminate_on_invalid = src -> terminate_on_invalid;
}

static uint64_t calculate_rows_per_thread( uint32_t * num_threads, uint64_t row_count )
{
    uint64_t res = row_count;
    uint64_t limit = 100 * ( *num_threads );
    if ( row_count < limit )
    {
        *num_threads = 1;
    }
    else
    {
        res = ( row_count / ( *num_threads ) ) + 1;
    }
    return res;
}

static rc_t join_the_threads_and_collect_status( Vector *threads, join_stats * stats )
{
    rc_t rc = 0;
    uint32_t i, n = VectorLength( threads );
    for ( i = VectorStart( threads ); i < n; ++i )
    {
        join_thread_data * jtd = VectorGet( threads, i );
        if ( NULL != jtd )
        {
            rc_t rc_thread;
            KThreadWait( jtd -> thread, &rc_thread );
            if ( 0 != rc_thread )
            {
                rc = rc_thread;
            }
            KThreadRelease( jtd -> thread );
            add_join_stats( stats, &jtd -> stats );
            free( jtd );
        }
    }
    VectorWhack ( threads, NULL, NULL );
    return rc;
}

rc_t execute_tbl_join( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    join_stats * stats,
                    const char * tbl_name,
                    const struct temp_dir * temp_dir,
                    struct temp_registry * registry,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    bool show_progress,
                    format_t fmt,
                    const join_options * join_options )
{
    rc_t rc = 0;

    if ( show_progress )
    {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "join   :" );
        KOutHandlerSetStdOut();
    }

    if ( 0 == rc )
    {
        uint64_t row_count = 0;
        rc = extract_sra_row_count( dir, vdb_mgr, accession_short, accession_path, tbl_name, cur_cache, &row_count ); /* above */
        if ( 0 == rc && row_count > 0 )
        {
            bool name_column_present;
            rc = is_column_name_present( dir, vdb_mgr, accession_short, accession_path, tbl_name, &name_column_present );
            if ( 0 == rc )
            {
                Vector threads;
                int64_t row = 1;
                uint32_t thread_id;
                uint64_t rows_per_thread;
                struct bg_progress * progress = NULL;
                struct join_options corrected_join_options; /* helper.h */

                VectorInit( &threads, 0, num_threads );
                correct_join_options( &corrected_join_options, join_options, name_column_present );
                rows_per_thread = calculate_rows_per_thread( &num_threads, row_count );
                if ( show_progress )
                {
                    rc = bg_progress_make( &progress, row_count, 0, 0 ); /* progress_thread.c */
                }

                for ( thread_id = 0; 0 == rc && thread_id < num_threads; ++thread_id )
                {
                    join_thread_data * jtd = calloc( 1, sizeof * jtd );
                    if ( NULL != jtd )
                    {
                        jtd -> dir              = dir;
                        jtd -> vdb_mgr          = vdb_mgr;
                        jtd -> accession_short  = accession_short;
                        jtd -> accession_path   = accession_path;
                        jtd -> tbl_name         = tbl_name;
                        jtd -> first_row        = row;
                        jtd -> row_count        = rows_per_thread;
                        jtd -> cur_cache        = cur_cache;
                        jtd -> buf_size         = buf_size;
                        jtd -> progress         = progress;
                        jtd -> registry         = registry;
                        jtd -> fmt              = fmt;
                        jtd -> join_options     = &corrected_join_options;

                        rc = make_joined_filename( temp_dir, jtd -> part_file, sizeof jtd -> part_file,
                                    accession_short, thread_id ); /* temp_dir.c */

                        if ( 0 == rc )
                        {
                            /* thread executes cmn_thread_func() located above */
                            rc = helper_make_thread( &jtd -> thread, cmn_thread_func, jtd, THREAD_BIG_STACK_SIZE ); /* helper.c */
                            if ( 0 != rc )
                            {
                                ErrMsg( "tbl_join.c helper_make_thread( fastq/special #%d ) -> %R", thread_id, rc );
                            }
                            else
                            {
                                rc = VectorAppend( &threads, NULL, jtd );
                                if ( 0 != rc )
                                {
                                    ErrMsg( "tbl_join.c VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                                }
                            }
                            row += rows_per_thread;
                        }
                    }
                }
                rc = join_the_threads_and_collect_status( &threads, stats );
                bg_progress_release( progress ); /* progress_thread.c ( ignores NULL ) */
            }
        }
    }
    return rc;
}

static rc_t CC fast_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    join_thread_data * jtd = data;
    /* insert fasta-split-spot reading and printing only */
    return rc;
}

rc_t execute_fast_tbl_join( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    join_stats * stats,
                    const char * tbl_name,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    bool show_progress,
                    const join_options * join_options )
{
    rc_t rc = 0;

    if ( show_progress )
    {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "read :" );
        KOutHandlerSetStdOut();
    }

    if ( 0 == rc )
    {
        uint64_t row_count = 0;
        rc = extract_sra_row_count( dir, vdb_mgr, accession_short, accession_path, tbl_name, cur_cache, &row_count ); /* above */
        if ( 0 == rc && row_count > 0 )
        {
            bool name_column_present;
            rc = is_column_name_present( dir, vdb_mgr, accession_short, accession_path, tbl_name, &name_column_present );
            if ( 0 == rc )
            {
                Vector threads;
                int64_t row = 1;
                uint32_t thread_id;
                uint64_t rows_per_thread;
                struct bg_progress * progress = NULL;
                struct join_options corrected_join_options; /* helper.h */

                VectorInit( &threads, 0, num_threads );
                correct_join_options( &corrected_join_options, join_options, name_column_present );
                rows_per_thread = calculate_rows_per_thread( &num_threads, row_count );
                if ( show_progress )
                {
                    rc = bg_progress_make( &progress, row_count, 0, 0 ); /* progress_thread.c */
                }

                /* big difference her : we create one instance of common-join-results,
                   and use it for all threads! */

                for ( thread_id = 0; 0 == rc && thread_id < num_threads; ++thread_id )
                {
                    join_thread_data * jtd = calloc( 1, sizeof * jtd );
                    if ( NULL != jtd )
                    {
                        jtd -> dir              = dir;
                        jtd -> vdb_mgr          = vdb_mgr;
                        jtd -> accession_short  = accession_short;
                        jtd -> accession_path   = accession_path;
                        jtd -> tbl_name         = tbl_name;
                        jtd -> first_row        = row;
                        jtd -> row_count        = rows_per_thread;
                        jtd -> cur_cache        = cur_cache;
                        jtd -> buf_size         = buf_size;
                        jtd -> progress         = progress;
                        jtd -> registry         = NULL;
                        jtd -> fmt              = ft_fasta_us_split_spot; /* we handle only this one... */
                        jtd -> join_options     = &corrected_join_options;
                        jtd -> part_file[ 0 ]   = 0;    /* we are not using a part-file */

                        if ( 0 == rc )
                        {
                            /* thread executes cmn_thread_func() located above */
                            rc = helper_make_thread( &jtd -> thread, fast_thread_func, jtd, THREAD_BIG_STACK_SIZE ); /* helper.c */
                            if ( 0 != rc )
                            {
                                ErrMsg( "tbl_join.c helper_make_thread( fasta #%d ) -> %R", thread_id, rc );
                            }
                            else
                            {
                                rc = VectorAppend( &threads, NULL, jtd );
                                if ( 0 != rc )
                                {
                                    ErrMsg( "tbl_join.c VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                                }
                            }
                            row += rows_per_thread;
                        }
                    }
                }

                rc = join_the_threads_and_collect_status( &threads, stats );
                bg_progress_release( progress ); /* progress_thread.c ( ignores NULL ) */
            }
        }
    }

    return rc;
}

