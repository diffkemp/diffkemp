#!/bin/bash

# Prerequisity is an existing docker image called 'diffkemp' built from
# the provided Dockerfile
docker run \
	-ti --security-opt seccomp=unconfined \
	-v $PWD:/diffkemp:Z \
	-w /diffkemp \
	diffkemp \
	/bin/bash
