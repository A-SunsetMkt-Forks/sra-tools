# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

add_compile_definitions( __mod__="test/sra-sort" )

if ( NOT WIN32 )

    ToolsRequired(sra-sort kar bam-load sra-stat sam-factory)

    # if directory /export/home/TMP does not exist, the script will not run and exit with 0
    # if it does exist, the script requires env var TEST_DATA to be set, or it will return an error (1)
    # TEST_DATA is supposed to point to a directory with the inputs for this test
    if ( DEFINED ENV{TEST_DATA} )
        add_test( NAME SlowTest_Sra_sort
                  COMMAND test-sra-sort.sh ${DIRTOTEST}
                  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    if ( "linux" STREQUAL ${OS} )
            add_test( NAME Test_sra_sort_meta_copy
                COMMAND ./sra_sort_meta_copy.sh ${DIRTOTEST} "${CMAKE_SOURCE_DIR}/libs/schema:${VDB_INCDIR}" ${BINDIR}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

endif()
