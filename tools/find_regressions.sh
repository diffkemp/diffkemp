#!/bin/bash
# Script for finding regressions fixed by some commit(s)
#
# Usage:
#     tools/find_regressions.sh [REVISION]
#
# Compares output of DiffKemp between the current revision and REVISION.
# If not specified, REVISION is equal to HEAD^.

if [[ ! -d .git ]]; then
    echo Run script from the repo root directory
    exit 1
fi

function build() {
    echo -n "Compiling $1-patch version ($(git rev-parse --short HEAD)), do not switch git branches..."
    builddir=build-regression-$1
    mkdir -p $builddir
    export SIMPLL_BUILD_DIR=$builddir
    cmake -B $builddir -S . -GNinja -DCMAKE_BUILD_TYPE=Debug > /dev/null
    ninja -C $builddir > /dev/null
    pip install -e .
    echo done
}

curref=$(git rev-parse --abbrev-ref HEAD)
lastref=${1:-$curref^}

build post
git checkout -q $lastref
build pre
git checkout -q $curref

logdir=logs/regression-$(date +%F-%T)
mkdir -p $logdir

versions=(80 147 193 240 305 348)

for i in "${!versions[@]}"; do
    if [[ $(($i + 1)) == ${#versions[@]} ]]; then break; fi

    old=linux-4.18.0-${versions[$i]}.el8
    new=linux-4.18.0-${versions[$i + 1]}.el8

    echo -n Comparing $old and $new...

    log_pre=$logdir/$old-$new-pre
    log_post=$logdir/$old-$new-post
    log_diff=$logdir/$old-$new-diff

    export SIMPLL_BUILD_DIR=build-regression-pre
    bin/diffkemp compare snapshots/$old snapshots/$new --stdout --show-diff > $log_pre

    export SIMPLL_BUILD_DIR=build-regression-post
    bin/diffkemp compare snapshots/$old snapshots/$new --stdout --show-diff > $log_post

    diff $log_pre $log_post > $log_diff
    # If all lines in a diff end with a number or "---", the changes are very
    # likely only in callstacks (i.e. no real regression).
    if [[ -s $log_diff ]] && egrep -vq '[0-9]$|---' $log_diff; then
        echo regression found
    else
        echo no regression found
        rm $log_pre $log_post $log_diff
    fi
done

if [[ -n "$(ls -A $logdir)" ]]; then
    echo Possible regressions found!
    echo See contents of $logdir/ for details.
else
    rm -r $logdir
fi

