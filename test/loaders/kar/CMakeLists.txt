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

if ( NOT WIN32 )
    ToolsRequired(kar prefetch srapath)

    add_test( NAME Test_Kar
        COMMAND
            ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/ NCBI_VDB_QUALITY=R
            sh -c "./kar-ntest.sh ${DIRTOTEST}/kar ${DIRTOTEST}/prefetch"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

    if( TARGET kar-asan )
        add_test( NAME Test_Kar-asan
            COMMAND
                ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
                sh -c "./kar-ntest.sh ${DIRTOTEST}/kar-asan ${DIRTOTEST}/prefetch"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

    if( TARGET kar-tsan )
        add_test( NAME Test_Kar-tsan
            COMMAND
                ${CMAKE_COMMAND} -E env ${CONFIGTOUSE}=/
                sh -c "./kar-ntest.sh ${DIRTOTEST}/kar-tsan ${DIRTOTEST}/prefetch"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
    endif()

else()
#TODO: make run on Windows
endif()
