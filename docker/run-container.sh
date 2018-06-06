#!/bin/bash

# Prerequisity is an existing docker image called 'diffkemp' built from
# the provided Dockerfile
docker run \
	-ti --security-opt seccomp=unconfined \
    -m 8g \
    --cpus 3 \
	-v $PWD:/diffkemp:Z \
	-w /diffkemp \
	viktormalik/diffkemp \
	/bin/bash
