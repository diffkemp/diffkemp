# DiffKemp

Tool for semantic **Diff**erence of **Ke**rnel **m**odule **p**arameters.

## About
The tool uses static analysis methods to automatically determine how the effect
of a chosen kernel option (module parameter, sysctl) or even of a single
kernel function changed between two different kernel versions.

The analysis is composed of multiple steps:
* The compared versions of the analysed sources are compiled into the LLVM
  internal representation (LLVM IR). 
* The **SimpLL** component is used to simplify the programs and to compare them
  syntactically. The list of functions that have different syntax are returned
  from SimpLL.
* The external **llreve** tool is used to generate a first order logic formula
  expressing the fact that the remaining programs are semantically equal.
* The generated formula is solved using an automatic SMT solver *Z3* and the
  result determines whether the programs are semantically equal or not.

## Components
* SimpLL: Simplification of the compared programs for a subsequent comparison of
  their semantic equivalence. Mainly the following transformations are done:
  * Slicing out the code that is not influenced by the value of the given kernel
    parameter.
  * Removing bodies of functions that are syntactically equal. The syntax
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

    docker run -t -w /diffkemp viktormalik:diffkemp <running-command>

For different running commands, see below.

## Tools

Currently, DiffKemp is a group of tools designed to analyse different parts of
the kernel. Each tool takes at least 2 mandatory arguments determining kernel
versions to compare. Corresponding kernel sources are automatically downloaded
if necessary.

### DiffKemp

Checks semantic equivalence of kernel module parameters (defined by the
`module_param` macro). The tool can be invoked as follows:

    bin/diffkemp old-version new-version modules-dir [-m module] [-p param] 
                 
Checks semantic equivalence of all parameters of all modules in the given
directory. Optionally, a single module or a single parameter can be chosen.
The `modules-dir` is given relatively to the kernel directory.

### DiffKabi

Checks semantic equivalence of functions on the KABI whitelist. Requires the
KABI whitelist file to be present or a single function to be specified (in fact,
the tool can be used to compare semantic difference of any function). The tool
can be invoked as follows:

    bin/diffkabi old-version new-version [-f function] [--control-flow-only] [--syntax-diff]

By default, all functions from the `kabi_whitelist_x86_64` file are compared.
`--control-flow-only` ignores changes in assignments, only checks for changes in
the control flow - conditions, loops, function calls, goto.
`--syntax-diff` displays the `diff` command result for functions that are
semantically different.

### DiffSysctl

Checks semantic equivalence of `sysctl` options. The tool can be invoked as
follows:

    bin/diffsysctl old-version new-version sysctl 

Supports using some patterns to analyse multiple sysctls at once (e.g.
`kernel.*`, `kernel.{sysctl-1|sysctl-2}`).
Currently, only supports a limited number of sysctl groups (`kernel.*`, `vm.*`,
`fs.*`, `net.ipv4.conf.*`, `net.core.*`).

## Development

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
