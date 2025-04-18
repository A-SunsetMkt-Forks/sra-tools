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

add_compile_definitions( __mod__="test/external/align-info" )

if ( NOT WIN32 )
ToolsRequired(align-info)

    # # make sure align-info -h does not crash
    add_test( NAME Test_Align_Info_H
        COMMAND ${DIRTOTEST}/align-info -h
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    add_test( NAME Test_Align_Info_Refseq_404
        COMMAND
         env
          NC_000005.8={\"NCBI-PHID\":\"aHh\",\"result\":[{\"bundle\":\"NC_000005.8\",\"status\":404,\"msg\":\"CannotResolveAccession\"}]}
          NCBI_SETTINGS=/
          NCBI_VDB_NO_ETC_NCBI_KFG=1
          VDB_CONFIG=
          bash test-refseq-404.sh ${BINDIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

else()
#TODO: make run on Windows
endif()
