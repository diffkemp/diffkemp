#!/usr/bin/env bash

# TODO - integrate handwritten tests into pytest framework

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

echo "Testing basic patterns"
for PATTERN_FILE in ${SCRIPT_DIR}/*.pattern.c; do
    PATTERN_PATH=${PATTERN_FILE%".pattern.c"}
    PATTERN_NAME=$( basename $PATTERN_FILE .pattern.c )
    echo "Testing ${PATTERN_NAME}"
    bin/diffkemp compare ${PATTERN_PATH}.old ${PATTERN_PATH}.new -p ${PATTERN_FILE} --stdout
done
