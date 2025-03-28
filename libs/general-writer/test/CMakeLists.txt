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

# this is where test executables will be created
# to access tools themselves, use ${BINDIR}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${TESTBINDIR} )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TESTBINDIR_DEBUG} )
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TESTBINDIR_RELEASE} )

add_executable ( test-general-writer test-general-writer.cpp )
add_dependencies ( test-general-writer general-writer )

target_link_libraries ( test-general-writer
    general-writer
    ${CMAKE_STATIC_LIBRARY_PREFIX}ktst${CMAKE_STATIC_LIBRARY_SUFFIX}
	${COMMON_LIBS_READ}
)

ToolsRequired(gw-dumper)
make_directory( ${CMAKE_CURRENT_SOURCE_DIR}/actual )

add_test ( NAME Test_GeneralWriter_RoundTrip
            COMMAND
            sh -c "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test-general-writer && ${BINDIR}/gw-dumper -v ./actual/test-general-writer.gw | grep --text -v timestamp | grep --text -v pid >./actual/test-general-writer.stdout && diff ./actual/test-general-writer.stdout ./expected/test-general-writer.stdout"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
