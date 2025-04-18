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

# common flags
include_directories ( ${CMAKE_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/libs/inc )

link_libraries (
    tk-version
    kapp
    ngs-vdb
    ncbi-ngs
    loader
	${COMMON_LIBS_READ}
    ${COMMON_LINK_LIBRARIES}
)

if (WIN32)
    set ( CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:wmainCRTStartup")
endif()

### ref-variation

GenerateExecutableWithDefs( ref-variation "common.cpp;helper.cpp;ref-variation.cpp" "__mod__=\"tools/ref-variation\"" "" "ksrch" )

MakeLinksExe ( ref-variation false )

### var-expand

GenerateExecutableWithDefs( var-expand "common.cpp;helper.cpp;var-expand.cpp" "__mod__=\"tools/var-expand\"" "" "ksrch" )
MakeLinksExe ( var-expand false )

### common

if ( NOT TOOLS_ONLY )
    add_subdirectory( test )
endif()


