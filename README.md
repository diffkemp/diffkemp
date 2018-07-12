# DiffKemp

Tool for semantic **Diff**erence of **Ke**rnel **m**odule **p**arameters.

## About
The tool uses static analysis based on formal methods to automatically determine
how the effect of a chosen parameter on a kernel module changed between
different kernel versions.

The analysis is composed of multiple steps:
* The compared versions of the analysed program are compiled into the LLVM
  internal representation (LLVM IR). 
* All code that is not influenced by the chosen parameter is removed (i.e.
  program is sliced). 
* The llreve tool is used to generate a first order logic formula expressing the
  fact that the remaining programs are semantically equal
* The generated formula is solved using an automatic SMT solver Z3 and the
  result determines whether programs are semantically equal or not.

Currently, the main goal is a proof of concept.

## Components
* llreve: Tool for regression verification of LLVM programs. It uses symbolic
  execution and an external SMT solver (currently Z3) to prove that two
  functions have the same semantics.

* slicer: Slicing of programs to remove the code that is not affected by the
  analysed parameter.

## Running environment

Since the tool uses some custom modifications to LLVM, it is recommended to run
the tool in the prepared development container. The container image can be
retrieved from DockerHub:
[https://hub.docker.com/r/viktormalik/diffkemp/](https://hub.docker.com/r/viktormalik/diffkemp/)

After that, the container can be run using

    docker/run-container.sh

## Build
	mkdir build
	cd build
	cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
    ninja
    cd ..

## Run
    bin/diffkemp modules-dir [-m module] [-p param] old-version new-version

Check semantic equivalence of all parameters of all modules in the given
directory between the two kernel versions. Optionally, single module or single
parameter can be chosen.
The modules-dir is given relatively to the kernel directory.
The tool is able to download necessary kernel sources.

## Tests

The project contains unit and regression testing using pytest that can be run by:

    pytest tests
