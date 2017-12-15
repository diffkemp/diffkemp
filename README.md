# DiffKemp

Tool for semantic **Diff**erence of **Ke**rnel **m**odule **p**arameters.

## About
The tool uses static analysis to automatically determine how the effect of
a chosen parameter on a kernel module changed between different kernel versions.

Currently, the main goal is a proof of concept.

Built over LLVM infrastructure.

## Components
* llreve: Tool for regression verification of LLVM programs. It uses symbolic
  execution and an external SMT solver (currently Z3) to prove that two
  functions have the same semantics.

* slicer: Slicing of programs to remove code that is not affected by the module
  parameter.

## Build
	mkdir build
	cd build
	cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
    ninja
    cd ..

## Run
    ./diffkemp.py file-1 file-2 parameter-name function

Checks equivalence of function in modules from file-1 and file-2 with respect to
the given parameter.

