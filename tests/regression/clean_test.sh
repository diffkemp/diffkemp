#!/bin/bash

cd "$(dirname "$0")"

if [ "$2" = "all" ]
then
    rm -rf $1/kernel_modules/*
else
    for arg in "${@:2}"
    do
        rm -rf $1/kernel_modules/$arg
    done
fi

cd $OLDPWD

