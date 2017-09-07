#!/bin/bash
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

echo $0 $*

#
#  Download and test SRA Toolkit tarballs (see VDB-1345)
#  Errors are reported to the specified email
#
# Parameters:
# $1 - working dir (will contain a copy of the latest md5sum.txt file)
#
# return codes:
# 0 - tests passed
# 1 - wget failed
# 2 - gunzip failed
# 3 - tar failed
# 4 - one of the tools failed

WORKDIR=$1
if [ "${WORKDIR}" == "" ]
then
    WORKDIR="./temp"
fi

TOOLS="abi-dump abi-load align-info bam-load blastn_vdb cache-mgr cg-load fastq-dump fastq-load helicos-load illumina-dump \
illumina-load kar kdbmeta latf-load prefetch rcexplain sam-dump sff-dump sff-load sra-pileup \
sra-sort sra-stat srapath srf-load tblastn_vdb test-sra vdb-config vdb-copy vdb-decrypt vdb-dump vdb-encrypt vdb-lock \
vdb-unlock vdb-validate"

# vdb-passwd is obsolete but still in the package

case $(uname) in
Linux)
    python -mplatform | grep Ubuntu && OS=ubuntu64 || OS=centos_linux64
    TOOLS="${TOOLS} pacbio-load remote-fuser"
    ;;
Darwin)
    OS=mac64
    realpath() {
        [[ $1 = /* ]] && echo "$1" || echo "$PWD/${1#./}"
    }
    ;;
esac
HOMEDIR=$(dirname $(realpath $0))

TARBALLS_URL=https://ftp-trace.ncbi.nlm.nih.gov/sra/sdk/current/
TARGET=sratoolkit.current-${OS}

mkdir -p ${WORKDIR}
OLDDIR=$(pwd)
cd ${WORKDIR}

wget --no-check-certificate ${TARBALLS_URL}${TARGET}.tar.gz || exit 1
gunzip -f ${TARGET}.tar.gz || exit 2
PACKAGE=$(tar tf ${TARGET}.tar | head -n 1)
rm -rf ${PACKAGE}
tar xf ${TARGET}.tar || exit 3

$HOMEDIR/smoke-test.sh ./${PACKAGE} 2.8.2
RC=$?

# FAILED=""
# for tool in ${TOOLS}
# do
#     echo $tool
#     ${PACKAGE}/bin/$tool -h
#     if [ "$?" != "0" ]
#     then
#         echo "$(pwd)/${PACKAGE}/bin/$tool failed"
#         FAILED="${FAILED} $tool"
#     fi
# done

if [ "${RC}" != "0" ]
then
    echo "Smoke test returned ${RC}"
    exit 4
fi

rm -rf ${PACKAGE} ${TARGET}.tar
cd ${OLDDIR}
