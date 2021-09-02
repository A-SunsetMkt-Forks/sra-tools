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

#include "sorter.h"
#include "lookup_writer.h"
#include "lookup_reader.h"
#include "raw_read_iter.h"
#include "merge_sorter.h"
#include "progress_thread.h"
#include "helper.h"

#include <atomic64.h>
#include <kproc/thread.h>

#include <klib/vector.h>
#include <klib/printf.h>
#include <klib/progressbar.h>
#include <klib/out.h>
#include <kproc/thread.h>

/* 
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */
#include <atomic.h>

typedef struct lookup_producer_t
{
    struct raw_read_iter * iter; /* raw_read_iter.h */
    KVector * store;
    struct bg_progress_t * progress; /* progress_thread.h */
    struct background_vector_merger_t * merger; /* merge_sorter.h */
    SBuffer_t buf; /* helper.h */
    uint64_t bytes_in_store;
    atomic64_t * processed_row_count;
    uint32_t chunk_id, sub_file_id;
    size_t buf_size, mem_limit;
    bool single;
} lookup_producer_t;


static void release_producer( lookup_producer_t * self )
{
    if ( NULL != self )
    {
        release_SBuffer( &( self -> buf ) ); /* helper.c */
        if ( NULL != self -> iter )
        {
            destroy_raw_read_iter( self -> iter ); /* raw_read_iter.c */
        }
        if ( NULL != self -> store )
        {
            KVectorRelease( self -> store );
        }
        free( ( void * ) self );
    }
}

static rc_t init_multi_producer( lookup_producer_t * self,
                                 cmn_iter_params_t * cmn, /* helper.h */
                                 struct background_vector_merger_t * merger, /* merge_sorter.h */
                                 size_t buf_size,
                                 size_t mem_limit,
                                 struct bg_progress_t * progress, /* progress_thread.h */
                                 uint32_t chunk_id,
                                 int64_t first_row,
                                 uint64_t row_count,
                                 atomic64_t * processed_row_count )
{
    rc_t rc = KVectorMake( &self -> store );
    if ( 0 != rc )
    {
        ErrMsg( "sorter.c init_multi_producer().KVectorMake() -> %R", rc );
    }
    else
    {
        rc = make_SBuffer( &( self -> buf ), 4096 ); /* helper.c */
        if ( 0 == rc )
        {
            cmn_iter_params_t cp;

            self -> iter            = NULL;
            self -> progress        = progress;
            self -> merger          = merger;
            self -> bytes_in_store  = 0;
            self -> chunk_id        = chunk_id;
            self -> sub_file_id     = 0;
            self -> buf_size        = buf_size;
            self -> mem_limit       = mem_limit;
            self -> single          = false;
            self -> processed_row_count = processed_row_count;

            cp . dir                = cmn -> dir;
            cp . vdb_mgr            = cmn -> vdb_mgr;
            cp . accession_short    = cmn -> accession_short;
            cp . accession_path     = cmn -> accession_path;
            cp . first_row          = first_row;
            cp . row_count          = row_count;
            cp . cursor_cache       = cmn -> cursor_cache;

            rc = make_raw_read_iter( &cp, &( self -> iter ) );
        }
    }
    return rc;
}

static rc_t push_store_to_merger( lookup_producer_t * self, bool last )
{
    rc_t rc = 0;
    if ( self -> bytes_in_store > 0 )
    {
        rc = push_to_background_vector_merger( self -> merger, self -> store ); /* this might block! merge_sorter.c */
        if ( 0 == rc )
        {
            self -> store = NULL;
            self -> bytes_in_store = 0;
            if ( !last )
            {
                rc = KVectorMake( &self -> store );
                if ( 0 != rc )
                {
                    ErrMsg( "sorter.c push_store_to_merger().KVectorMake() -> %R", rc );
                }
            }
        }
    }
    return rc;
}

static rc_t write_to_store( lookup_producer_t * self,
                            uint64_t key,
                            const String * read )
{
    /* we write it to the store...*/
    rc_t rc = pack_read_2_4na( read, &( self -> buf ) ); /* helper.c */
    if ( 0 != rc )
    {
        ErrMsg( "sorter.c write_to_store().pack_read_2_4na() failed %R", rc );
    }
    else
    {
        const String * to_store;
        rc = StringCopy( &to_store, &( self -> buf . S ) );
        if ( 0 != rc )
        {
            ErrMsg( "sorter.c write_to_store().StringCopy() -> %R", rc );
        }
        else
        {
            rc = KVectorSetPtr( self -> store, key, ( const void * )to_store );
            if ( 0 != rc )
            {
                ErrMsg( "sorter.c write_to_store().KVectorSetPtr() -> %R", rc );
            }
            else
            {
                size_t item_size = ( sizeof key ) + ( sizeof *to_store ) + to_store -> size;
                self -> bytes_in_store += item_size;
            }
        }
        
        if ( 0 == rc &&
             self -> mem_limit > 0 &&
             self -> bytes_in_store >= self -> mem_limit )
        {
            rc = push_store_to_merger( self, false ); /* this might block ! */
        }
    }
    return rc;
}

static rc_t CC producer_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    rc_t rc1;
    lookup_producer_t * producer = data;
    raw_read_rec rec;
    uint64_t row_count = 0;
    struct bg_progress_t * progress = producer -> progress;

    while ( 0 == rc && get_from_raw_read_iter( producer -> iter, &rec, &rc1 ) ) /* raw_read_iter.c */
    {
        rc_t rc2 = get_quitting(); /* helper.c */
        if ( 0 == rc2 )
        {
            if ( 0 == rc1 )
            {
                if ( rec . read . len < 1 )
                {
                    rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcNull );
                    ErrMsg( "sorter.c producer_thread_func: rec.read.len = %d", rec . read . len );
                }
                else
                {
                    uint64_t key = make_key( rec . seq_spot_id, rec . seq_read_id ); /* helper.c */
                    /* the keys are allowed to be out of order here */
                    rc = write_to_store( producer, key, &rec . read ); /* above! */
                    if ( 0 == rc )
                    {
                        bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
                        row_count++;
                    }
                }
            }
            else
            {
                ErrMsg( "sorter.c get_from_raw_read_iter( %lu ) -> %R", rec . seq_spot_id, rc1 );
                rc = rc1;
            }
        }
        else
        {
            rc = rc2;
        }
    }
    
    if ( 0 == rc )
    {
        /* now we have to push out / write out what is left in the last store */
        rc = push_store_to_merger( producer, true ); /* this might block ! */
    }
    else
    {
        set_quitting(); /* helper.c */
    }

    if ( 0 == rc && 0 != producer -> processed_row_count )
    {
        atomic64_read_and_add( producer -> processed_row_count, row_count );
    }
    release_producer( producer ); /* above */

    return rc;
}

/* -------------------------------------------------------------------------------------------- */

static uint64_t find_out_row_count( cmn_iter_params_t * cmn )
{
    rc_t rc;
    uint64_t res = 0;
    struct raw_read_iter * iter; /* raw_read_iter.c */
    cmn_iter_params_t cp; /* cmn_iter.h */
    
    cp . dir             = cmn -> dir;
    cp . vdb_mgr         = cmn -> vdb_mgr;
    cp . accession_short = cmn -> accession_short;
    cp . accession_path  = cmn -> accession_path;
    cp . first_row       = 0;
    cp . row_count       = 0;
    cp . cursor_cache    = cmn -> cursor_cache;

    rc = make_raw_read_iter( &cp, &iter ); /* raw_read_iter.c */
    if ( 0 == rc )
    {
        res = get_row_count_of_raw_read( iter ); /* raw_read_iter.c */
        destroy_raw_read_iter( iter ); /* raw_read_iter.c */
    }
    return res;
}

static rc_t run_producer_pool( cmn_iter_params_t * cmn, /* helper.h */
                               struct background_vector_merger_t * merger, /* merge_sorter.h */
                               size_t buf_size,
                               size_t mem_limit,
                               uint32_t num_threads,
                               bool show_progress )
{
    rc_t rc = 0;
    uint64_t total_row_count = find_out_row_count( cmn );
    if ( 0 == total_row_count )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "sorter.c run_producer_pool(): row_count == 0!" );
    }
    else
    {
        Vector threads;
        uint32_t chunk_id = 1;
        int64_t row = 1;
        uint64_t rows_per_thread = ( total_row_count / num_threads ) + 1;
        struct bg_progress_t * progress = NULL;
        atomic64_t processed_row_count;

        tell_total_rowcount_to_vector_merger( merger, total_row_count ); /* merge_sorter.h */
        atomic64_set( &processed_row_count, 0 );
        VectorInit( &threads, 0, num_threads );
        if ( show_progress )
        {
            rc = bg_progress_make( &progress, total_row_count, 0, 0 ); /* progress_thread.c */
        }

        while ( 0 == rc && ( row < ( int64_t )total_row_count ) )
        {
            lookup_producer_t * producer = calloc( 1, sizeof *producer );
            if ( NULL != producer )
            {
                rc = init_multi_producer( producer,
                                          cmn,
                                          merger,
                                          buf_size,
                                          mem_limit,
                                          progress,
                                          chunk_id,
                                          row,
                                          rows_per_thread,
                                          &processed_row_count ); /* above */
                if ( 0 == rc )
                {
                    KThread * thread;
                    rc = helper_make_thread( &thread, producer_thread_func, producer, THREAD_BIG_STACK_SIZE );
                    if ( 0 != rc )
                    {
                        ErrMsg( "sorter.c run_producer_pool().helper_make_thread( sort-thread #%d ) -> %R", chunk_id - 1, rc );
                    }
                    else
                    {
                        rc = VectorAppend( &threads, NULL, thread );
                        if ( 0 != rc )
                        {
                            ErrMsg( "sorter.c run_producer_pool().VectorAppend( sort-thread #%d ) -> %R", chunk_id - 1, rc );
                        }
                        else
                        {
                            row  += rows_per_thread;
                            chunk_id++;
                        }
                    }
                }
                else
                {
                    release_producer( producer ); /* above */
                }
            }
            else
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            }
        }

        /* collect all the sorter-threads */
        rc = join_and_release_threads( &threads ); /* helper.c */
        if ( 0 != rc )
        {
            ErrMsg( "sorter.c run_producer_pool().join_and_release_threads -> %R", rc );
        }

        /* all sorter-threads are done now, tell the progress-thread to terminate! */
        bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/

        if ( 0 == rc )
        {
            uint64_t value = atomic64_read( &processed_row_count );
            if ( value != total_row_count )
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSize, rcInvalid );
                ErrMsg( "sorter.c run_producer_pool() : processed lookup rows: %lu of %lu", value, total_row_count );
            }
        }
    }
    return rc;
}


rc_t execute_lookup_production( KDirectory * dir,
                                const VDBManager * vdb_mgr,
                                const char * accession_short,
                                const char * accession_path,
                                struct background_vector_merger_t * merger,
                                size_t cursor_cache,
                                size_t buf_size,
                                size_t mem_limit,
                                uint32_t num_threads,
                                bool show_progress )
{
    rc_t rc = 0;
    if ( show_progress )
    {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "lookup :" );
        KOutHandlerSetStdOut();
    }
    
    if ( rc == 0 )
    {
        cmn_iter_params_t cmn = { dir, vdb_mgr, accession_short, accession_path, 0, 0, cursor_cache };
        rc = run_producer_pool( &cmn,
                                merger,
                                buf_size,
                                mem_limit,
                                num_threads,
                                show_progress ); /* above */
    }
    
    /* signal to the receiver-end of the job-queue that nothing will be put into the
       queue any more... */
    if ( rc == 0 && merger != NULL )
        rc = seal_background_vector_merger( merger ); /* merge_sorter.c */
        
    if ( rc != 0 )
        ErrMsg( "sorter.c execute_lookup_production() -> %R", rc );

    return rc;
}
