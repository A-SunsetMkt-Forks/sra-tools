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

include_directories ("..")

# command line tests
if ( NOT WIN32)
	set ( DUMP_REF_FASTA  ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/dump-ref-fasta )

    # NB. Slow (run time > 1m) tests should names that start with "SlowTest_"; run without: "ctest -E SlowTest_", run only: "ctest -R SlowTest_"

	# help
	add_test ( NAME dump-ref-fasta-1.0-h WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
	            COMMAND runtestcase.sh ${DUMP_REF_FASTA} 1.0-h --args "-h" )
	add_test ( NAME dump-ref-fasta-1.1-help WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} 
	            COMMAND runtestcase.sh ${DUMP_REF_FASTA} 1.1-help --args "--help" )
	# version
    add_test ( NAME dump-ref-fasta-1.2-V WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ./execandgrep.sh "${DUMP_REF_FASTA} -V" ${VERSION} )
    add_test ( NAME dump-ref-fasta-1.3-v WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ./execandgrep.sh "${DUMP_REF_FASTA} -v" ${VERSION} )
    add_test ( NAME dump-ref-fasta-1.4-version WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND ./execandgrep.sh "${DUMP_REF_FASTA} --version" ${VERSION} )
	# regular run (includes non-local references starting at around 365000 bytes)
	add_test ( NAME dump-ref-fasta-2.0 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND runtestcase.sh ${DUMP_REF_FASTA} 2.0 --args "SRR496123  | head -n 370000" )
	# full ref
    add_test ( NAME dump-ref-fasta-3.0 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND runtestcase.sh ${DUMP_REF_FASTA} 3.0 --args "SRR496123 NW_003336815.1" )
	# slice
    add_test ( NAME dump-ref-fasta-3.1 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND runtestcase.sh ${DUMP_REF_FASTA} 3.1 --args "SRR496123 NW_003336815.1:0-100" )
	# open slice
    add_test ( NAME dump-ref-fasta-3.2 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND runtestcase.sh ${DUMP_REF_FASTA} 3.2 --args "SRR496123 NW_003336815.1:100" )
	# multiple slices on the same ref
    add_test ( NAME dump-ref-fasta-3.3 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND runtestcase.sh ${DUMP_REF_FASTA} 3.3 --args "SRR496123 NW_003336837.1:100-200 NW_003336815.1:0-100" )
	# local references only (non-local references start at around 365000 bytes)
    add_test ( NAME dump-ref-fasta-4.0 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND runtestcase.sh ${DUMP_REF_FASTA} 4.0 --args "--localref SRR496123  | head -n 370000" )
	# local references only, slice (NW_001884453.3 is non-local and will not be included)
    add_test ( NAME dump-ref-fasta-4.1 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMMAND runtestcase.sh ${DUMP_REF_FASTA} 4.1 --args "-l SRR496123 NW_003336837.1:100-200 NW_001884453.3:0-100" )

endif ()
