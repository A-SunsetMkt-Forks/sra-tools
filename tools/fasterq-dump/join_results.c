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
#include "join_results.h"
#include "helper.h"
#include <klib/vector.h>
#include <klib/printf.h>
#include <kfs/buffile.h>

typedef struct join_printer
{
    struct KFile * f;
    uint64_t file_pos;
} join_printer;

typedef rc_t ( * print_v1 )( struct join_results * self,
                             int64_t row_id,
                             uint32_t dst_id,
                             uint32_t read_id,
                             const String * name,
                             const String * read,
                             const String * quality );

typedef rc_t ( * print_v2 )( struct join_results * self,
                             int64_t row_id,
                             uint32_t dst_id,
                             uint32_t read_id,
                             const String * name,
                             const String * read_1,
                             const String * read_2,
                             const String * quality );

typedef struct join_results
{
    KDirectory * dir;
    struct temp_registry * registry;
    const char * output_base;
    const char * accession_short;
    struct Buf2NA * buf2na;
    print_v1 v1_print_name_null;
    print_v1 v1_print_name_not_null;
    print_v2 v2_print_name_null;
    print_v2 v2_print_name_not_null;    
    SBuffer print_buffer;   /* we have only one print_buffer... */
    Vector printers;
    size_t buffer_size;
    bool print_frag_nr, print_name;
} join_results;

static void CC destroy_join_printer( void * item, void * data )
{
    if ( NULL != item )
    {
        join_printer * p = item;
        if ( NULL != p -> f )
        {
            rc_t rc = KFileRelease( p -> f );
            if ( 0 != rc )
            {
                ErrMsg( "destroy_join_printer().KFileRelease() -> %R", rc );
            }
        }
        free( item );
    }
}

void destroy_join_results( join_results * self )
{
    if ( NULL != self )
    {
        VectorWhack ( &self -> printers, destroy_join_printer, NULL );
        release_SBuffer( &self -> print_buffer );
        if ( NULL != self -> buf2na )
        {
            release_Buf2NA( self -> buf2na );
        }
        free( ( void * ) self );
    }
}

static const char * fmt_fastq_v1_no_name_no_frag_nr = "@%s.%ld length=%u\n%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v1_no_name_no_frag_nr = ">%s.%ld length=%u\n%S\n";
static rc_t print_v1_no_name_no_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_no_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_no_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_no_name_frag_nr = "@%s.%ld/%u length=%u\n%S\n+%s.%ld/%u length=%u\n%S\n";
static const char * fmt_fasta_v1_no_name_frag_nr = ">%s.%ld/%u length=%u\n%S\n";
static rc_t print_v1_no_name_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_no_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_no_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_syn_name_no_frag_nr = "@%s.%ld %ld length=%u\n%S\n+%s.%ld %ld length=%u\n%S\n";
static const char * fmt_fasta_v1_syn_name_no_frag_nr = ">%s.%ld %ld length=%u\n%S\n";
static rc_t print_v1_syn_name_no_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_syn_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                row_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_syn_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_syn_name_frag_nr = "@%s.%ld/%u %ld length=%u\n%S\n+%s.%ld/%u %ld length=%u\n%S\n";
static const char * fmt_fasta_v1_syn_name_frag_nr = ">%s.%ld/%u %ld length=%u\n%S\n";
static rc_t print_v1_syn_name_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_syn_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_syn_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_real_name_no_frag_nr  = "@%s.%ld %S length=%u\n%S\n+%s.%ld %S length=%u\n%S\n";
static const char * fmt_fastq_v1_empty_name_no_frag_nr = "@%s.%ld length=%u\n%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v1_real_name_no_frag_nr  = ">%s.%ld %S length=%u\n%S\n";
static const char * fmt_fasta_v1_empty_name_no_frag_nr = ">%s.%ld length=%u\n%S\n";
static rc_t print_v1_real_name_no_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality )
{
    if ( NULL != quality )
    {
        if ( name -> len > 0 )
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v1_real_name_no_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id,
                                    name,
                                    read -> len, read,
                                    /* QUALITY... */
                                    self -> accession_short, row_id,
                                    name,
                                    quality -> len, quality );
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_empty_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    }
    if ( name -> len > 0 )
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v1_real_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                name,
                                read -> len, read );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_empty_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_real_name_frag_nr  = "@%s.%ld/%u %S length=%u\n%S\n+%s.%ld/%u %S length=%u\n%S\n";
static const char * fmt_fastq_v1_empty_name_frag_nr = "@%s.%ld/%u length=%u\n%S\n+%s.%ld/%u length=%u\n%S\n";
static const char * fmt_fasta_v1_real_name_frag_nr  = ">%s.%ld/%u %S length=%u\n%S\n";
static const char * fmt_fasta_v1_empty_name_frag_nr = ">%s.%ld/%u length=%u\n%S\n";
static rc_t print_v1_real_name_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality )
{
    if ( NULL != quality )
    {
        if ( name -> len > 0 )
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v1_real_name_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    read -> len, read,
                                    /* QUALITY... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    quality -> len, quality );
        return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v1_empty_name_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id, read_id,
                                    read -> len, read,
                                    /* QUALITY... */
                                    self -> accession_short, row_id, read_id,
                                    quality -> len, quality );
    }
    if ( name -> len > 0 )
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v1_real_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                name,
                                read -> len, read );
    return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v1_empty_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read -> len, read );
}

static const char * fmt_fastq_v2_no_name_no_frag_nr = "@%s.%ld length=%u\n%S%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v2_no_name_no_frag_nr = ">%s.%ld length=%u\n%S%S\n";
static rc_t print_v2_no_name_no_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_no_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_no_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_no_name_frag_nr    = "@%s.%ld/%u length=%u\n%S%S\n+%s.%ld/%u %length=%u\n%S\n";
static const char * fmt_fasta_v2_no_name_frag_nr    = ">%s.%ld/%u length=%u\n%S%S\n";
static rc_t print_v2_no_name_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_no_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_no_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_syn_name_no_frag_nr = "@%s.%ld %ld length=%u\n%S%S\n+%s.%ld %ld length=%u\n%S\n";
static const char * fmt_fasta_v2_syn_name_no_frag_nr = ">%s.%ld %ld length=%u\n%S%S\n";
static rc_t print_v2_syn_name_no_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_syn_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                row_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_syn_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_syn_name_frag_nr = "@%s.%ld/%u %ld length=%u\n%S%S\n+%s.%ld/%u %ld length=%u\n%S\n";
static const char * fmt_fasta_v2_syn_name_frag_nr = ">%s.%ld/%u %ld length=%u\n%S%S\n";
static rc_t print_v2_syn_name_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality )
{
    if ( NULL != quality )
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_syn_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                quality -> len, quality );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_syn_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_real_name_no_frag_nr  = "@%s.%ld %S length=%u\n%S%S\n+%s.%ld %S length=%u\n%S\n";
static const char * fmt_fastq_v2_empty_name_no_frag_nr = "@%s.%ld length=%u\n%S%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v2_real_name_no_frag_nr  = ">%s.%ld %S length=%u\n%S%S\n";
static const char * fmt_fasta_v2_empty_name_no_frag_nr = ">%s.%ld length=%u\n%S%S\n";
static rc_t print_v2_real_name_no_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality )
{
    if ( NULL != quality )
    {
        if ( name -> len > 0 )
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v2_real_name_no_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id,
                                    name,
                                    read1 -> len + read2 -> len, read1, read2,
                                    /* QUALITY... */
                                    self -> accession_short, row_id,
                                    name,
                                    quality -> len, quality );
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_empty_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    }
    if ( name -> len > 0 )
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v2_real_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                name,
                                read1 -> len + read2 -> len, read1, read2 );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_empty_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_real_name_frag_nr  = "@%s.%ld/%u %S length=%u\n%S%S\n+%s.%ld/%u %S length=%u\n%S\n";
static const char * fmt_fastq_v2_empty_name_frag_nr = "@%s.%ld/%u length=%u\n%S%S\n+%s.%ld/%u length=%u\n%S\n";
static const char * fmt_fasta_v2_real_name_frag_nr  = ">%s.%ld/%u %S length=%u\n%S%S\n";
static const char * fmt_fasta_v2_empty_name_frag_nr = ">%s.%ld/%u length=%u\n%S%S\n";
static rc_t print_v2_real_name_frag_nr( join_results * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality )
{
    if ( NULL != quality )
    {
        if ( name -> len > 0 )
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v2_real_name_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    read1 -> len + read2 -> len, read1, read2,
                                    /* QUALITY... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    quality -> len, quality );
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_empty_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                quality -> len, quality );
    }
    if ( name -> len > 0 )
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v2_real_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                name,
                                read1 -> len + read2 -> len, read1, read2 );
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_empty_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

rc_t make_join_results( struct KDirectory * dir,
                        join_results ** results,
                        struct temp_registry * registry,
                        const char * output_base,
                        const char * accession_short,
                        size_t file_buffer_size,
                        size_t print_buffer_size,
                        bool print_frag_nr,
                        bool print_name,
                        const char * filter_bases )
{
    rc_t rc = 0;
    struct Buf2NA * buf2na = NULL;
    if ( filter_bases != NULL )
    {
        rc = make_Buf2NA( &buf2na, 512, filter_bases );
        if ( 0 != rc )
        {
            ErrMsg( "make_join_results().error creating nucstrstr-filter from ( %s ) -> %R", filter_bases, rc );
        }
    }
    if ( rc == 0 )
    {
        join_results * p = calloc( 1, sizeof * p );
        *results = NULL;
        if ( NULL == p )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_join_results().calloc( %d ) -> %R", ( sizeof * p ), rc );
        }
        else
        {
            p -> dir = dir;
            p -> output_base = output_base;
            p -> accession_short = accession_short;
            p -> buffer_size = file_buffer_size;
            p -> registry = registry;
            p -> print_frag_nr = print_frag_nr;
            p -> print_name = print_name;
            p -> buf2na = buf2na;
            
            /* available:
                print_v1_no_name_no_frag_nr()       print_v2_no_name_no_frag_nr()
                print_v1_no_name_frag_nr()          print_v2_no_name_frag_nr()
                print_v1_syn_name_no_frag_nr()      print_v2_syn_name_no_frag_nr()
                print_v1_syn_name_frag_nr()         print_v2_syn_name_frag_nr()
                print_v1_real_name_no_frag_nr()     print_v2_real_name_no_frag_nr()
                print_v1_real_name_frag_nr()        print_v2_real_name_frag_nr()
            */
            if ( print_frag_nr )
            {
                if ( print_name )
                {
                    p -> v1_print_name_null     = print_v1_syn_name_frag_nr;
                    p -> v1_print_name_not_null = print_v1_real_name_frag_nr;
                    p -> v2_print_name_null     = print_v2_syn_name_frag_nr;
                    p -> v2_print_name_not_null = print_v2_real_name_frag_nr;
                }
                else
                {
                    p -> v1_print_name_null     = print_v1_no_name_frag_nr;
                    p -> v1_print_name_not_null = print_v1_no_name_frag_nr;
                    p -> v2_print_name_null     = print_v2_no_name_frag_nr;
                    p -> v2_print_name_not_null = print_v2_no_name_frag_nr;
                }
            }
            else
            {
                if ( print_name )
                {
                    p -> v1_print_name_null     = print_v1_syn_name_no_frag_nr;
                    p -> v1_print_name_not_null = print_v1_real_name_no_frag_nr;
                    p -> v2_print_name_null     = print_v2_syn_name_no_frag_nr;
                    p -> v2_print_name_not_null = print_v2_real_name_no_frag_nr;
                }
                else
                {
                    p -> v1_print_name_null     = print_v1_no_name_no_frag_nr;
                    p -> v1_print_name_not_null = print_v1_no_name_no_frag_nr;
                    p -> v2_print_name_null     = print_v2_no_name_no_frag_nr;
                    p -> v2_print_name_not_null = print_v2_no_name_no_frag_nr;
                }
            }

            rc = make_SBuffer( &( p -> print_buffer ), print_buffer_size ); /* helper.c */
            if ( 0 == rc )
            {
                VectorInit ( &p -> printers, 0, 4 );
                *results = p;
            }
        }
    }
    if ( 0 != rc && NULL != buf2na )
    {
        release_Buf2NA( buf2na );
    }
    return rc;
}

bool join_results_match( join_results * self, const String * bases )
{
    bool res = true;
    if ( NULL != self && NULL != bases && NULL != self -> buf2na )
    {
        res = match_Buf2NA( self -> buf2na, bases ); /* helper.c */
    }
    return res;
}

bool join_results_match2( struct join_results * self, const String * bases1, const String * bases2 )
{
    bool res = true;
    if ( NULL != self && NULL != bases1 && NULL != bases2 && NULL != self -> buf2na )
    {
        res = ( match_Buf2NA( self -> buf2na, bases1 ) || match_Buf2NA( self -> buf2na, bases2 ) ); /* helper.c */
    }
    return res;
}

static rc_t make_join_printer( join_results * self, uint32_t read_id, join_printer ** printer )
{
    char filename[ 4096 ];
    size_t num_writ;
    
    rc_t rc = string_printf( filename, sizeof filename, &num_writ, "%s.%u", self -> output_base, read_id );
    *printer = NULL;
    if ( 0 != rc )
    {
        ErrMsg( "make_join_printer().string_vprintf() -> %R", rc );
    }
    else
    {
        struct KFile * f;
        rc = KDirectoryCreateFile( self -> dir, &f, false, 0664, kcmInit, "%s", filename );
        if ( 0 != rc )
        {
            ErrMsg( "make_join_printer().KDirectoryVCreateFile() -> %R", rc );
        }
        else
        {
            if ( self -> buffer_size > 0 )
            {
                struct KFile * temp_file = f;
                rc = KBufFileMakeWrite( &temp_file, f, false, self -> buffer_size );
                if ( 0 != rc )
                {
                    ErrMsg( "make_join_printer().KBufFileMakeWrite() -> %R", rc );
                }
                {
                    rc_t rc2 = KFileRelease( f );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "make_join_printer().KFileRelease().1 -> %R", rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
                f = temp_file;
            }
            if ( 0 == rc )
            {
                join_printer * p = calloc( 1, sizeof * p );
                if ( NULL == p )
                {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                    ErrMsg( "make_join_printer().calloc( %d ) -> %R", ( sizeof * p ), rc );
                    {
                        rc_t rc2 = KFileRelease( f );
                        if ( 0 != rc2 )
                        {
                            ErrMsg( "make_join_printer().KFileRelease().2 -> %R", rc2 );
                        }
                    }
                }
                else
                {
                    rc = register_temp_file( self -> registry, read_id, filename );
                    if ( rc != 0 )
                    {
                        free( p );
                        {
                            rc_t rc2 = KFileRelease( f );
                            if ( 0 != rc2 )
                            {
                                ErrMsg( "make_join_printer().KFileRelease().3 -> %R", rc2 );
                            }
                        }
                    }
                    else
                    {
                        p -> f = f;
                        *printer = p;
                    }
                }
            }
        }
    }
    return rc;
}

rc_t join_results_print( struct join_results * self, uint32_t read_id, const char * fmt, ... )
{
    rc_t rc = 0;
    if ( NULL == self )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcSelf, rcNull );
        ErrMsg( "join_results_print() -> %R", rc );
    }
    else if ( NULL == fmt )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "join_results_print() -> %R", rc );
    }
    else
    {
        join_printer * p = VectorGet ( &self -> printers, read_id );
        if ( NULL == p )
        {
            rc = make_join_printer( self, read_id, &p );
            if ( 0 == rc )
            {
                rc = VectorSet ( &self -> printers, read_id, p );
                if ( 0 != rc )
                {
                    destroy_join_printer( p, NULL );
                }
            }
        }

        if ( 0 == rc && NULL != p )
        {
            bool done = false;
            uint32_t cnt = 4;

            while ( 0 == rc && !done && cnt-- > 0 )
            {
                va_list args;
                va_start ( args, fmt );
                rc = print_to_SBufferV( & self -> print_buffer, fmt, args );
                /* do not print failed rc, because it is used to increase the buffer */
                va_end ( args );

                done = ( 0 == rc );
                if ( !done )
                {
                    rc = try_to_enlarge_SBuffer( & self -> print_buffer, rc );
                }
            }

            if ( 0 != rc )
            {
                ErrMsg( "join_results_print().failed to enlarge buffer -> %R", rc );
            }
            else
            {
                size_t num_writ, to_write;
                to_write = self -> print_buffer . S . size;
                const char * src = self -> print_buffer . S . addr;
                rc = KFileWriteAll( p -> f, p -> file_pos, src, to_write, &num_writ );
                if ( 0 != rc )
                {
                    ErrMsg( "join_results_print().KFileWriteAll( at %lu ) -> %R", p -> file_pos, rc );
                }
                else if ( num_writ != to_write )
                {
                    rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
                    ErrMsg( "join_results_print().KFileWriteAll( at %lu ) ( %d vs %d ) -> %R", p -> file_pos, to_write, num_writ, rc );
                }
                else
                {
                    p -> file_pos += num_writ;
                }
            }
        }
    }
    return rc;
}

rc_t join_results_print_fastq_v1( join_results * self,
                                  int64_t row_id,
                                  uint32_t dst_id,
                                  uint32_t read_id,
                                  const String * name,
                                  const String * read,
                                  const String * quality )
{
    if ( NULL == name )
    {
        return self -> v1_print_name_null( self, row_id, dst_id, read_id, name, read, quality );
    }
    return self -> v1_print_name_not_null( self, row_id, dst_id, read_id, name, read, quality );
}

rc_t join_results_print_fastq_v2( join_results * self,
                                  int64_t row_id,
                                  uint32_t dst_id,
                                  uint32_t read_id,
                                  const String * name,
                                  const String * read1,
                                  const String * read2,
                                  const String * quality )
{
    if ( NULL == name )
    {
        return self -> v2_print_name_null( self, row_id, dst_id, read_id, name, read1, read2, quality );
    }
    return self -> v2_print_name_not_null( self, row_id, dst_id, read_id, name, read1, read2, quality );
}
