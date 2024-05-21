#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

for filename in ${SCRIPT_DIR}/*{old,new}.c; do
    BASENAME=$( basename $filename )
    DIR_NAME=${BASENAME%".c"}
    DIR_PATH=${SCRIPT_DIR}/${DIR_NAME}
    bin/diffkemp build $filename $DIR_PATH
done
