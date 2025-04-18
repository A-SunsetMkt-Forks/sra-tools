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

AddExecutableTest( Test_SraSchema_Unit "test-schema.cpp" "sra;kapp;${COMMON_LINK_LIBRARIES};${COMMON_LIBS_WRITE}" "" )

if( NOT DEFINED VDB_SRCDIR )
    set( VDB_SRCDIR "${VDB_INCDIR}/.." )
endif()

if (Python3_EXECUTABLE)
    add_test( NAME Test_SraSchema_454_db
        COMMAND ${Python3_EXECUTABLE} test_VDB-5250.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_SraSchema_454_db
        PROPERTIES
        ENVIRONMENT "VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR};VDB_SRCDIR=${VDB_SRCDIR}"
    )

    add_test( NAME Test_SraSchema_IonTorrent_db
        COMMAND ${Python3_EXECUTABLE} test_VDB-5251.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    set_tests_properties( Test_SraSchema_IonTorrent_db
        PROPERTIES
        ENVIRONMENT "VDB_LIBRARY_PATH=${NCBI_VDB_LIBDIR};VDB_SRCDIR=${VDB_SRCDIR}"
    )
endif()
