#!/bin/bash

cd "$(dirname "$0")"

if [ "$1" = "all" ]
then
    rm -rf kernel_modules/*
else
    for arg in "$@"
    do
        rm -rf kernel_modules/$arg
    done
fi

cd $OLDPWD

