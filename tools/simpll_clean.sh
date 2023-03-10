#!/bin/bash
# A script which deletes the SimpLL shared library if it's linked against
# a different LLVM version than the one currently installed.
#
# Usage:
# ./simpll_clean.sh [DIFFKEMP_ROOT]
#
# When DIFFKEMP_ROOT is not provided, the script assumes that
# the current working directory is the DiffKemp root directory.

if [[ -z $1 ]]
    then
        DIFFKEMP_ROOT=$PWD
    else
        DIFFKEMP_ROOT=$1
fi

if [[ ! -d ${DIFFKEMP_ROOT}/diffkemp/simpll ]]
    then
        echo "Error: ${DIFFKEMP_ROOT} is not a DiffKemp root directory." >&2
        exit 1
fi

if [[ ! -f ${DIFFKEMP_ROOT}/diffkemp/simpll/_simpll.abi3.so ]]
    then
        exit 0
fi

LLVM_SYSTEM_VERSION=`llvm-config --version | sed -n 's/\([0-9]*\)\..*/\1/p'`
LLVM_SIMPLL_VERSION=`ldd ${DIFFKEMP_ROOT}/diffkemp/simpll/_simpll.abi3.so | sed -n 's/^.*LLVM-\([0-9]*\)\.so.*$/\\1/p'`

if [[ $LLVM_SIMPLL_VERSION -ne $LLVM_SYSTEM_VERSION ]]
    then
        echo "Deleting SimpLL shared library linked to LLVM ${LLVM_SIMPLL_VERSION}."
        rm -f ${DIFFKEMP_ROOT}/diffkemp/simpll/_simpll.abi3.so
        rm -rf ${DIFFKEMP_ROOT}/build/lib.linux* ${DIFFKEMP_ROOT}/build/temp.linux*
fi

exit 0
