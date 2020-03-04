#!/bin/bash

COMMAND="docker"

# Use podman if docker is not available.
if [ ! -x "$(command -v docker)" ]; then
    COMMAND="podman"
fi

# Prerequisity is an existing docker image called 'diffkemp-devel' built from
# the provided Dockerfile
"$COMMAND" run \
    -ti --security-opt seccomp=unconfined \
    -m 8g \
    --cpus 3 \
    -v $PWD:/diffkemp:Z \
    -w /diffkemp \
    viktormalik/diffkemp-devel

