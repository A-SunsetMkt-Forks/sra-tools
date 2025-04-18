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

add_compile_definitions( __mod__="test/internal/vdb-copy" )

ToolsRequired(vdb-copy)

if( NOT DEFINED VDB_SRCDIR )
    set( VDB_SRCDIR "${VDB_INCDIR}/.." )
endif()

if( Python3_EXECUTABLE )

	add_test( NAME Test_VDB_Copy_Check_exit_code
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-copy${EXE}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	if( TARGET vdb-copy-asan )
		add_test( NAME Test_VDB_Copy_Check_exit_code-asan
			COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-copy-asan${EXE}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()
	if( TARGET vdb-copy-tsan )
		add_test( NAME Test_VDB_Copy_Check_exit_code-tsan
			COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/build/check-exit-code.py ${DIRTOTEST}/vdb-copy-tsan${EXE}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
	endif()

	if ( NOT WIN32 )
		add_test( NAME Test_VDB_Copy_On_Short_Accession
			COMMAND sh -c "./copy_and_compare.sh ${DIRTOTEST} SRR000123 ${BINDIR}"
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

		add_test( NAME Test_VDB_Copy_Should_Fail_On_Invalid_Accession
			COMMAND sh -c "./return_code_on_error.sh ${DIRTOTEST}"
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

        add_test( NAME Test_VDB_Copy_Redaction
            COMMAND sh -c "./test-redaction.sh '${DIRTOTEST}' '${VDB_LIBDIR}' '${SRC_INTERFACES_DIR}:${VDB_INCDIR}' '${VDB_SRCDIR}'"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
	endif()

else()
    message(WARNING "${DIRTOTEST}/vdb-copy${EXE} is not found. The corresponding tests are skipped." )
endif()
