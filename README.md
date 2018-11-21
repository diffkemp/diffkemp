# DiffKemp

Tool for semantic **Diff**erence of **Ke**rnel **m**odule **p**arameters.

## About
The tool uses static analysis methods to automatically determine how the effect
of a chosen parameter of a kernel module changed between different kernel
versions

The analysis is composed of multiple steps:
* The compared versions of the analysed sources are compiled into the LLVM
  internal representation (LLVM IR). 
* The SimpLL component is used to simplify the programs and to compare them
  syntactically. Only those functions that have different syntax are returned
  from SimpLL.
* The llreve tool is used to generate a first order logic formula expressing the
  fact that the remaining programs are semantically equal.
* The generated formula is solved using an automatic SMT solver Z3 and the
  result determines whether programs are semantically equal or not.

Currently, the main goal is a proof of concept.

## Components
* SimpLL: Simplification of the compared programs for a subsequent comparison of
  their semantic equivalence. Mainly the following transformations are done:
  * Slicing out the code that is not influenced by the value of the given kernel
    parameter.
  * Removing the bodies of functions that are syntactically equal. The syntax
    comparison is based on the LLVM's FunctionComparator with multiple custom
    modifications and enhancements.

* llreve: A tool for regression verification of LLVM programs. It uses symbolic
  execution and an external SMT solver (currently Z3) to prove that two
  functions have the same semantics.
  Since llreve is maintained as a separate project, it is included as a GIT
  submodule in this repo. Clone either with `--recurse-submodules` or run after
  clone:

        git submodule init
        git submodule update

## Running environment

There is a prepared Docker image with the built DiffKemp. The image can be
retrieved from DockerHub:
[https://hub.docker.com/r/viktormalik/diffkemp/](https://hub.docker.com/r/viktormalik/diffkemp/)

The tool can be invoked as follows:

    docker run -t -w /diffkemp viktormalik:diffkemp bin/diffkemp <params>

The tool parameters are described below.

## Run
    bin/diffkemp modules-dir [-m module] [-p param] [--modules-with-params] 
                 old-version new-version

Checks semantic equivalence of all parameters of all modules in the given
directory between the two kernel versions. Optionally, single module or single
parameter can be chosen.
The modules-dir is given relatively to the kernel directory.
The tool is able to download necessary kernel sources.

# Development

Since the tool uses some custom modifications of LLVM, it is recommended to use
the prepared development container. The container image can be retrieved from
DockerHub:
[https://hub.docker.com/r/viktormalik/diffkemp-devel/](https://hub.docker.com/r/viktormalik/diffkemp-devel/)

After that, the container can be run using

    docker/diffkemp-devel/run-container.sh

The script mounts the current directory (the top DiffKemp directory) as a volume
inside the container.

## Build
	mkdir build
	cd build
	cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
    ninja
    cd ..

## Tests

The project contains unit and regression testing using pytest that can be run by:

    pytest tests
