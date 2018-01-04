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
    bool process = true;
    if ( process && ( jo -> min_read_len > 0 ) )
    {
        process = ( rec -> read . len >= jo -> min_read_len );
        if ( !process )
            stats -> fragments_too_short++;
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
        if ( !process )
            stats -> fragments_technical++;
    }
    if ( process && ( jo -> min_read_len > 0 ) )
    {
        process = ( rec -> read_len[ read_id_0 ] >= jo -> min_read_len );
        if ( !process )
            stats -> fragments_too_short++;
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
    if ( filter1( stats, rec, jo ) )
    {
        if ( join_results_match( results, &( rec -> read ) ) )
        {
            rc = join_results_print_fastq_v1( results,
                                              rec -> row_id,
                                              dst_id,
                                              read_id,
                                              jo -> rowid_as_name ? NULL : &( rec -> name ),
                                              &( rec -> read ),
                                              &( rec -> quality ) );
            if ( rc == 0 )
                stats -> fragments_written++;
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
    
    while ( rc == 0 && read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
        {
            if ( filter( stats, rec, jo, read_id_0 ) )
            {
                R . addr = &rec -> read . addr[ offset ];
                R . size = rec -> read_len[ read_id_0 ];
                R . len  = ( uint32_t )R . size;

                if ( join_results_match( results, &R ) )
                {
                    Q . addr = &rec -> quality . addr[ offset ];
                    Q . size = rec -> read_len[ read_id_0 ];
                    Q . len  = ( uint32_t )Q . size;

                    if ( join_results_match( results, &( rec -> read ) ) )
                    rc = join_results_print_fastq_v1( results,
                                                      rec -> row_id,
                                                      0,
                                                      read_id_0 + 1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &R,
                                                      &Q );
                    if ( rc == 0 )
                        stats -> fragments_written++;
                }
            }
            offset += rec -> read_len[ read_id_0 ];           
        }
        else
            stats -> fragments_zero_length++;
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
    
    while ( rc == 0 && read_id_0 < rec -> num_read_len )
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
                    Q . addr = &rec -> quality . addr[ offset ];
                    Q . size = rec -> read_len[ read_id_0 ];
                    Q . len  = ( uint32_t )Q . size;

                    rc = join_results_print_fastq_v1( results,
                                                      rec -> row_id,
                                                      write_id_1,
                                                      read_id_0 + 1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &R,
                                                      &Q );
                    if ( rc == 0 )
                        stats -> fragments_written++;
                }
            }
            offset += rec -> read_len[ read_id_0 ];            
        }
        else
            stats -> fragments_zero_length++;

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
    uint32_t valid_fragments = 0;
    uint32_t offset = 0;
    
    while ( read_id_0 < rec -> num_read_len )
    {
        if ( rec -> read_len[ read_id_0 ] > 0 )
            valid_fragments++;
        read_id_0++;
    }
    
    if ( valid_fragments == 0 )
        return rc;

    if ( valid_fragments < rec -> num_read_len )
        write_id_1 = 0;
    read_id_0 = 0;
    
    while ( rc == 0 && read_id_0 < rec -> num_read_len )
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
                    Q . addr = &rec -> quality . addr[ offset ];
                    Q . size = rec -> read_len[ read_id_0 ];
                    Q . len  = ( uint32_t )Q . size;
                
                    rc = join_results_print_fastq_v1( results,
                                                      rec -> row_id,
                                                      write_id_1,
                                                      read_id_0 + 1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &R,
                                                      &Q );
                    if ( rc == 0 )
                        stats -> fragments_written++;
                }

                if ( write_id_1 > 0 )
                    write_id_1++;
            }
            offset += rec -> read_len[ read_id_0 ];            
        }
        else
            stats -> fragments_zero_length++;
            
        read_id_0++;
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t perform_fastq_join( cmn_params * cp,
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
    
     rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_join().make_fastq_iter() -> %R", rc );
    else
    {
        fastq_rec rec;
        join_options local_opt = { jo -> rowid_as_name, false, jo -> print_frag_nr, jo -> min_read_len, jo -> filter_bases };
        while ( get_from_fastq_sra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            stats -> spots_read++;
            stats -> fragments_read += rec . num_read_len;
            
            rc = Quitting();
            if ( rc == 0 )
            {
                rc = print_fastq_1_read( stats, results, &rec, &local_opt, 1, 1 );
                
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_sra_iter( iter );
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
    
    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( rc == 0 )
    {
        fastq_rec rec;
        while ( get_from_fastq_sra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                stats -> spots_read++;
                stats -> fragments_read += rec . num_read_len;
            
                if ( rec . num_read_len == 1 )
                    rc = print_fastq_1_read( stats, results, &rec, jo, 0, 1 );
                else
                    rc = print_fastq_n_reads_split( stats, results, &rec, jo );

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_sra_iter( iter );
    }
    else
        ErrMsg( "make_fastq_iter() -> %R", rc );
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

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( rc == 0 )
    {
        fastq_rec rec;
        while ( get_from_fastq_sra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                stats -> spots_read++;
                stats -> fragments_read += rec . num_read_len;

                if ( rec . num_read_len == 1 )
                    rc = print_fastq_1_read( stats, results, &rec, jo, 1, 1 );
                else
                    rc = print_fastq_n_reads_split_file( stats, results, &rec, jo );

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_sra_iter( iter );
    }
    else
        ErrMsg( "make_fastq_iter() -> %R", rc );
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

    rc = make_fastq_sra_iter( cp, opt, tbl_name, &iter ); /* fastq-iter.c */
    if ( rc == 0 )
    {
        fastq_rec rec;
        join_options local_opt = { jo -> rowid_as_name, true, jo -> print_frag_nr, jo -> min_read_len, jo -> filter_bases };
        while ( get_from_fastq_sra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                stats -> spots_read++;
                stats -> fragments_read += rec . num_read_len;
                
                if ( rec . num_read_len == 1 )
                    rc = print_fastq_1_read( stats, results, &rec, &local_opt, 0, 1 );
                else
                    rc = print_fastq_n_reads_split_3( stats, results, &rec, &local_opt );

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_sra_iter( iter );
    }
    else
        ErrMsg( "make_fastq_iter() -> %R", rc );
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

typedef struct join_thread_data
{
    char part_file[ 4096 ];
    
    join_stats stats;

    KDirectory * dir;
    const char * accession_path;
    const char * accession_short;
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
    
    if ( rc == 0 )
        rc = make_join_results( jtd -> dir,
                                &results,
                                jtd -> registry,
                                jtd -> part_file,
                                jtd -> accession_short,
                                jtd -> buf_size,
                                4096,
                                jtd -> join_options -> print_frag_nr,
                                jtd -> join_options -> filter_bases );
    
    if ( rc == 0 && results != NULL )
    {
        cmn_params cp = { jtd -> dir, jtd -> accession_path, jtd -> first_row, jtd -> row_count, jtd -> cur_cache };
        switch( jtd -> fmt )
        {
            case ft_fastq       : rc = perform_fastq_join( &cp,
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

            default : break;
        }
        destroy_join_results( results );
    }
    return rc;
}

static rc_t extract_sra_row_count( KDirectory * dir,
                                   const char * accession_path,
                                   const char * tbl_name,
                                   size_t cur_cache,
                                   uint64_t * res )
{
    cmn_params cp = { dir, accession_path, 0, 0, cur_cache };
    struct fastq_sra_iter * iter;
    fastq_iter_opt opt = { false, false, false };
    rc_t rc = make_fastq_sra_iter( &cp, opt, tbl_name, &iter ); /* fastq_iter.c */
    if ( rc == 0 )
    {
        *res = get_row_count_of_fastq_sra_iter( iter );
        destroy_fastq_sra_iter( iter );
    }
    return rc;
}

rc_t execute_tbl_join( KDirectory * dir,
                    const char * accession_path,
                    const char * accession_short,
                    join_stats * stats,
                    const char * tbl_name,
                    const tmp_id * tmp_id,
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
        rc = KOutMsg( "join   :" );

    if ( rc == 0 )
    {
        uint64_t row_count = 0;
        rc = extract_sra_row_count( dir, accession_path, tbl_name, cur_cache, &row_count ); /* above */
        if ( rc == 0 && row_count > 0 )
        {
            bool name_column_present;

            if ( tbl_name == NULL )
                rc = cmn_check_tbl_column( dir, accession_path, "NAME", &name_column_present );
            else
                rc = cmn_check_db_column( dir, accession_path, tbl_name, "NAME", &name_column_present );
            
            if ( rc == 0 )
            {
                Vector threads;
                int64_t row = 1;
                uint32_t thread_id;
                uint64_t rows_per_thread;
                struct bg_progress * progress = NULL;
                struct join_options corrected_join_options;
                
                VectorInit( &threads, 0, num_threads );
                
                corrected_join_options . rowid_as_name = name_column_present ? join_options -> rowid_as_name : true;
                corrected_join_options . skip_tech = join_options -> skip_tech;
                corrected_join_options . print_frag_nr = join_options -> print_frag_nr;
                corrected_join_options . min_read_len = join_options -> min_read_len;
                corrected_join_options . filter_bases = join_options -> filter_bases;
                
                if ( row_count < ( num_threads * 100 ) )
                {
                    num_threads = 1;
                    rows_per_thread = row_count;
                }
                else
                {
                    rows_per_thread = ( row_count / num_threads ) + 1;
                }
                
                if ( show_progress )
                    rc = bg_progress_make( &progress, row_count, 0, 0 ); /* progress_thread.c */
                
                for ( thread_id = 0; rc == 0 && thread_id < num_threads; ++thread_id )
                {
                    join_thread_data * jtd = calloc( 1, sizeof * jtd );
                    if ( jtd != NULL )
                    {
                        jtd -> dir              = dir;
                        jtd -> accession_path   = accession_path;
                        jtd -> accession_short  = accession_short;
                        jtd -> tbl_name         = tbl_name;
                        jtd -> first_row        = row;
                        jtd -> row_count        = rows_per_thread;
                        jtd -> cur_cache        = cur_cache;
                        jtd -> buf_size         = buf_size;
                        jtd -> progress         = progress;
                        jtd -> registry         = registry;
                        jtd -> fmt              = fmt;
                        jtd -> join_options     = &corrected_join_options;

                        rc = make_joined_filename( jtd -> part_file, sizeof jtd -> part_file,
                                    accession_short, tmp_id, thread_id ); /* helper.c */

                        if ( rc == 0 )
                        {
                            rc = KThreadMake( &jtd -> thread, cmn_thread_func, jtd );
                            if ( rc != 0 )
                                ErrMsg( "KThreadMake( fastq/special #%d ) -> %R", thread_id, rc );
                            else
                            {
                                rc = VectorAppend( &threads, NULL, jtd );
                                if ( rc != 0 )
                                    ErrMsg( "VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                            }
                            row += rows_per_thread;
                        }
                    }
                }
                
                {
                    /* collect the threads, and add the join_stats */
                    uint32_t i, n = VectorLength( &threads );
                    for ( i = VectorStart( &threads ); i < n; ++i )
                    {
                        join_thread_data * jtd = VectorGet( &threads, i );
                        if ( jtd != NULL )
                        {
                            KThreadWait( jtd -> thread, NULL );
                            KThreadRelease( jtd -> thread );
                            
                            add_join_stats( stats, &jtd -> stats );
                                
                            free( jtd );
                        }
                    }
                    VectorWhack ( &threads, NULL, NULL );
                }

                bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/
            }
        }
    }
    return rc;
}
