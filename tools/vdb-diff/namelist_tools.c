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
#include "namelist_tools.h"

#include <klib/log.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/printf.h>

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>


rc_t nlt_make_namelist_from_string( const KNamelist **list, const char * src )
{
    VNamelist *v_names;
    rc_t rc = VNamelistMake ( &v_names, 5 );
    if ( rc == 0 )
    {
        char * s = string_dup_measure ( src, NULL );
        if ( s )
        {
            uint32_t str_begin = 0;
            uint32_t str_end = 0;
            char c;
            do
            {
                c = s[ str_end ];
                if ( c == ',' || c == 0 )
                {
                    if ( str_begin < str_end )
                    {
                        char c_temp = c;
                        s[ str_end ] = 0;
                        rc = VNamelistAppend ( v_names, &(s[str_begin]) );
                        s[ str_end ] = c_temp;
                    }
                    str_begin = str_end + 1;
                }
                str_end++;
            } while ( c != 0 && rc == 0 );
            free( s );
        }
		if ( rc == 0 )
			rc = VNamelistToConstNamelist ( v_names, list );
        VNamelistRelease( v_names );
    }
    return rc;
}


int nlt_strcmp( const char* s1, const char* s2 )
{
    size_t n1 = string_size ( s1 );
    size_t n2 = string_size ( s2 );
    return string_cmp
        ( s1, n1, s2, n2, ( n1 < n2 ) ? ( uint32_t ) n2 : ( uint32_t ) n1 );
}


bool nlt_is_name_in_namelist( const KNamelist *list, const char *name_to_find )
{
    uint32_t count, idx;
    bool res = false;
    if ( list == NULL || name_to_find == NULL )
        return res;
    if ( KNamelistCount( list, &count ) == 0 )
    {
        for ( idx = 0; idx < count && res == false; ++idx )
        {
            const char *item_name;
            if ( KNamelistGet( list, idx, &item_name ) == 0 )
            {
                if ( nlt_strcmp( item_name, name_to_find ) == 0 )
                    res = true;
            }
        }
    }
    return res;
}

/*
    - list1 and list2 containts strings
    - if one of the strings in list2 is contained ( partial match, strstr() )
      in one of the strings in list1 the function returns true...
*/
bool nlt_namelist_intersect( const KNamelist *list1, const KNamelist *list2 )
{
    uint32_t count1;
    bool res = false;
    if ( list1 == NULL || list2 == NULL )
        return res;
    if ( KNamelistCount( list1, &count1 ) == 0 )
    {
        uint32_t idx1;
        for ( idx1 = 0; idx1 < count1 && res == false; ++idx1 )
        {
            const char *string1;
            if ( KNamelistGet( list1, idx1, &string1 ) == 0 )
            {
                uint32_t count2;
                if ( KNamelistCount( list2, &count2 ) == 0 )
                {
                    uint32_t idx2;
                    for ( idx2 = 0; idx2 < count2 && res == false; ++idx2 )
                    {
                        const char *string2;
                        if ( KNamelistGet( list2, idx2, &string2 ) == 0 )
                        {
                            if ( strstr( string1, string2 ) != NULL )
                                res = true;
                        }
                    }
                }
            }
        }
    }
    return res;
}

/* -------------------------------------------------------------------
 for each entry in the source-list:
    if the item is NOT in the to-remove-list add it to the temp. list
    return the temp. list as dst ( transformed to const KNamelist )
 ------------------------------------------------------------------- */
rc_t nlt_remove_names_from_namelist( const KNamelist *source,
            const KNamelist **dest, const KNamelist *to_remove )
{
    rc_t rc = 0;
    uint32_t count;
    
    if ( source == NULL || dest == NULL || to_remove == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    *dest = NULL;
    rc = KNamelistCount( source, &count );
    if ( rc == 0 && count > 0 ) {
        VNamelist *cleaned;
        rc = VNamelistMake ( &cleaned, count );
        if ( rc == 0 ) {
            uint32_t idx;
            for ( idx = 0; idx < count && rc == 0; ++idx ) {
                const char *s;
                rc = KNamelistGet( source, idx, &s );
                if ( rc == 0 ) {
                    if ( !nlt_is_name_in_namelist( to_remove, s ) ) {
                        rc = VNamelistAppend ( cleaned, s );
                    }
                }
            }
            if ( 0 == rc ) {
                rc = VNamelistToConstNamelist ( cleaned, dest );
            }
        }
    }
    return rc;
}

rc_t nlt_remove_strings_from_namelist( const KNamelist *source,
            const KNamelist **dest, const char *items_to_remove )
{
    rc_t rc;
    if ( source == NULL || dest == NULL || items_to_remove == NULL ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        const KNamelist *to_remove;        
        rc = nlt_make_namelist_from_string( &to_remove, items_to_remove );
        if ( rc == 0 ) {
            rc = nlt_remove_names_from_namelist( source, dest, to_remove );
            KNamelistRelease( to_remove );
        }
    }
    return rc;
}

/* -------------------------------------------------------------------
 for each entry in the source-list:
    if the item is NOT in the to-remove-list add it to the temp. list
    return the temp. list as dst ( transformed to const KNamelist )
 ------------------------------------------------------------------- */
rc_t nlt_remove_prefixed_strings_from_namelist( const KNamelist *source,
            const KNamelist **dest, const char *items_to_remove, const char * prefix ) {

    rc_t rc;
    if ( source == NULL || dest == NULL || items_to_remove == NULL ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        const KNamelist *to_remove;

        *dest = NULL;
        rc = nlt_make_namelist_from_string( &to_remove, items_to_remove );
        if ( rc == 0 ) {
            uint32_t count;
            rc = KNamelistCount( source, &count );
            if ( 0 == rc && count > 0 ) {
                VNamelist *cleaned;
                rc = VNamelistMake ( &cleaned, count );
                if ( rc == 0 ) {
                    uint32_t idx;
                    size_t prefix_len = NULL != prefix ? string_size( prefix ) : 0;
                    for ( idx = 0; idx < count && rc == 0; ++idx ) {
                        const char *s;
                        rc = KNamelistGet( source, idx, &s );
                        if ( rc == 0 ) {
                            bool append = !nlt_is_name_in_namelist( to_remove, s );
                            if ( append && NULL != prefix ) {
                                size_t l = string_size ( s ) + prefix_len + 2;
                                char * s2 = malloc( l );
                                if ( s2 != NULL ) {
                                    size_t num_writ;
                                    rc_t rc1 = string_printf ( s2, l, &num_writ, "%s.%s", prefix, s );
                                    if ( rc1 == 0 ) { append = !nlt_is_name_in_namelist( to_remove, s2 ); }
                                    free( s2 );
                                }
                            }
                            if ( append ) { rc = VNamelistAppend ( cleaned, s ); }
                        }
                    }
                    if ( 0 == rc ) {
                        rc = VNamelistToConstNamelist ( cleaned, dest );
                    }
                }
            }
            KNamelistRelease( to_remove );
        }
    }
    return rc;
}


bool nlt_compare_namelists( const KNamelist *nl1, const KNamelist *nl2, uint32_t * found )
{
	bool res = false;
    if ( nl1 != NULL && nl2 != NULL )
	{
		uint32_t count_1;
		rc_t rc = KNamelistCount( nl1, &count_1 );
		if ( rc == 0 )
		{
			uint32_t count_2;
			rc = KNamelistCount( nl2, &count_2 );
			if ( rc == 0 && count_1 == count_2 )
			{
				uint32_t idx;
				uint32_t in_nl2 = 0;
				for ( idx = 0; idx < count_1 && rc == 0; ++idx )
				{
					const char *s;
					rc = KNamelistGet( nl1, idx, &s );
					if ( rc == 0 && s != NULL && nlt_is_name_in_namelist( nl2, s ) )
						in_nl2++;
				}
				res = ( rc == 0 && in_nl2 == count_1 );
				if ( found != NULL ) *found = in_nl2;
			}
		}
	}
	return res;
}


rc_t nlt_copy_namelist( const KNamelist *src, const KNamelist ** dst )
{
    VNamelist *v_names;
    rc_t rc = VNamelistMake ( &v_names, 5 );
	if ( rc == 0 )
	{
		uint32_t count;
		rc = KNamelistCount( src, &count );
		if ( rc == 0 )
		{
            uint32_t idx;
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char *s;
                rc = KNamelistGet( src, idx, &s );
				if ( rc == 0 && s != NULL )
					rc = VNamelistAppend ( v_names, s );
			}
		}
		if ( rc == 0 )
			rc = VNamelistToConstNamelist ( v_names, dst );
        VNamelistRelease( v_names );
	}
	return rc;
}


rc_t nlt_build_intersect( const KNamelist *nl1, const KNamelist *nl2, const KNamelist ** dst )
{
    VNamelist *v_names;
    rc_t rc = VNamelistMake ( &v_names, 5 );
	if ( rc == 0 )
	{
		/* loop through nl1: if a entry is found in nl2 -> add it to dst */
		uint32_t count;
		rc = KNamelistCount( nl1, &count );
		if ( rc == 0 )
		{
            uint32_t idx;
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char *s;
                rc = KNamelistGet( nl1, idx, &s );
				if ( rc == 0 && s != NULL )
				{
					if ( nlt_is_name_in_namelist( nl2, s ) )
						rc = VNamelistAppend ( v_names, s );
				}
			}
		}
		if ( rc == 0 )
			rc = VNamelistToConstNamelist ( v_names, dst );
        VNamelistRelease( v_names );

	}
	return rc;	
}

bool nlt_namelist_is_sub_set_in_full_set( const KNamelist * sub_set, const KNamelist * full_set )
{
	bool res = false;
	uint32_t count;
	rc_t rc = KNamelistCount( sub_set, &count );
	if ( rc == 0 )
	{
		uint32_t idx;
		uint32_t found = 0;
		for ( idx = 0; idx < count && rc == 0; ++idx )
		{
			const char *s;
			rc = KNamelistGet( sub_set, idx, &s );
			if ( rc == 0 && s != NULL && nlt_is_name_in_namelist( full_set, s ) )
				found++;
		}
		res = ( rc == 0 && count == found );
	}
	return res;
}

rc_t compare_2_namelists( const KNamelist * cols_1, const KNamelist * cols_2, const KNamelist ** cols_to_diff, bool intersect )
{
	uint32_t count_1;
	rc_t rc = KNamelistCount( cols_1, &count_1 );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "KNamelistCount() failed" );
	}
	else
	{
		uint32_t count_2;
		rc = KNamelistCount( cols_2, &count_2 );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "KNamelistCount() failed" );
		}
		else if ( count_1 != count_2 )
		{
			if ( intersect )
			{
				rc = nlt_build_intersect( cols_1, cols_2, cols_to_diff );
				if ( rc != 0 )
				{
					LOGERR ( klogInt, rc, "nlt_build_intersect() failed" );	
				}
			}
			else
			{
				rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
				PLOGERR( klogInt, ( klogInt, rc,
						 "the accessions have not the same number of columns! ( $(count1) != $(count2) )\n",
						 "count1=%u,count2=%u", count_1, count_2 ) );
				*cols_to_diff = NULL;
			}
		}
		else
		{
			uint32_t found_in_both;
			bool equal = nlt_compare_namelists( cols_1, cols_2, &found_in_both );
			if ( equal )
			{
				rc = nlt_copy_namelist( cols_1, cols_to_diff );
				if ( rc != 0 )
				{
					LOGERR ( klogInt, rc, "nlt_copy_namelist() failed" );
				}
			}
			else
			{
				if ( intersect )
				{
					rc = nlt_build_intersect( cols_1, cols_2, cols_to_diff );
					if ( rc != 0 )
					{
						LOGERR ( klogInt, rc, "nlt_build_intersect() failed" );	
					}
				}
				else
				{
					rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
					KOutMsg( "the 2 accessions have not the same set of columns! ( %u found in both )\n", found_in_both );
				}
			}
		}
	}
	return rc;
}

rc_t extract_columns_from_2_namelists( const KNamelist * cols_1, const KNamelist * cols_2, const char * sub_set,
											  const KNamelist ** cols_to_diff )
{
	rc_t rc = nlt_make_namelist_from_string( cols_to_diff, sub_set );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "cannot parse set of requested columns" );
	}
	else
	{
		if ( nlt_namelist_is_sub_set_in_full_set( *cols_to_diff, cols_1 ) )
		{
			if ( nlt_namelist_is_sub_set_in_full_set( *cols_to_diff, cols_2 ) )
			{
				rc = 0;
			}
			else
			{
				rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
				LOGERR ( klogInt, rc, "accession #2 is missing some of the requested columns" );
			}
		}
		else
		{
			rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
			LOGERR ( klogInt, rc, "accession #1 is missing some of the requested columns" );
		}
	}
	return rc;
}
