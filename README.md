![Build](https://github.com/viktormalik/diffkemp/actions/workflows/ci.yml/badge.svg?branch=master)

# DiffKemp

DiffKemp is a tool for automatic static analysis of semantic differences between
different versions of the Linux kernel.

## Installation

There are two options to install the project, either build from source or use a
prepared RPM package for Fedora.

### Install from source

Currently, DiffKemp runs on Linux and needs the following software installed:
* Clang and LLVM (supported versions are 5, 6, 7, 8, 9, 10, 11, and 12)
* Python 3 with CFFI (package `python3-cffi` in Fedora and Debian)
* Python packages from `requirements.txt` (run `pip install -r requirements.txt`)
* CScope

Additionally, to build manually, you need to install the following tools:
* CMake
* Ninja build system

Build can be done by running:

    mkdir build
    cd build
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
    ninja
    cd ..

    pip install -e .

### Install from RPM

Alternatively, you can use a prepared RPM package for Fedora that can be
installed from our Copr repository:
[https://copr.fedorainfracloud.org/coprs/viktormalik/diffkemp/](https://copr.fedorainfracloud.org/coprs/viktormalik/diffkemp/)

## Usage
DiffKemp assumes that the compared kernel versions are properly configured (by
running `make prepare`) and that all the tools necessary for building the
kernels are installed.

First, generate snapshots for each kernel version that you need to compare:

    bin/diffkemp generate KERNEL_DIR SNAPSHOT_DIR FUNCTION_LIST --kernel-with-builder

The command creates a DiffKemp snapshot for semantic diff of functions from
`FUNCTION_LIST` for the kernel located in `KERNEL_DIR`. The snapshot is stored
as a directory `SNAPSHOT_DIR`. Warning - if `SNAPSHOT_DIR` exists, it will be
rewritten.

After that, run the actual semantic comparison:

    bin/diffkemp compare SNAPSHOT_DIR_1 SNAPSHOT_DIR_2 --show-diff

The command compares functions from function lists stored inside the snapshots
pairwise and prints syntactic diffs (thanks to the `--syntax-diff` option) of
functions that are semantically different.

The diffs are stored in separate files (one file for each compared function that
is different) in a newly created directory. The name of the directory can be
specified using the `-o` option, otherwise it is generated automatically. Using
the `--stdout` option causes the diffs to be printed to standard output.

Note: if `FUNCTION_LIST` contains any symbols other than functions (e.g. global
variables), they will be ignored.

### Comparing sysctl options

Apart from comparing specific functions, DiffKemp supports comparison of
semantics of sysctl options. List of the options to compare can be passed as the
`FUNCTION_LIST` in the `generate` command. In such case, use `--sysctl` switch
to generate snapshot for sysctl parameter comparison. The `compare` command is
used in normal way.

Sysctl option comparison compares semantics of the proc handler function and
semantics of all functions using the data variable that the sysctl option sets.

It is possible to use patterns to specify a number of multiple sysctl options at
once such as:
* `kernel.*`
* `kernel.{sysctl-1|sysctl-2}`

Currently, these sysctl option groups are supported: `kernel.*`,
`vm.*`, `fs.*`, `net.core.*`, `net.ipv4.conf.*`.

## About
The tool uses static analysis methods to automatically determine how the effect
of a chosen kernel function or option (module parameter, sysctl) changed between
two different kernel versions.

The analysis is composed of multiple steps:
* Generate:
    * The source files containing definitions of the compared functions are
      compiled into the LLVM internal representation (LLVM IR).
    * The snapshot is created by copying the compiled LLVM IR files into the
      snapshot directory and by creating a YAML file with the list of functions
      to be compared.
* Compare:
    * The **SimpLL** component is used to compare the programs for semantic
      equality. The list of functions that are compared as not equal are
      returned.
    * For all functions and macros that are found to be semantically different,
      result of the standard `diff` command is shown.

## Components
* Kernel source builder: finding and building kernel source files into LLVM IR.
  * Sources with function definitions are found using CScope.
  * C sources are built into LLVM IR by checking the commands that are run by
    KBuild for building that file and by replacing GCC by Clang in the command.
* SimpLL: Comparison of programs for syntactic and simple semantic equivalence.
  Does the following steps:
  * Simplification of the compared programs. Multiple transformations are
    applied:
      * If comparing kernel options, slicing out the code that is not influenced
        by the value of the given option.
      * Dead code elimination.
      * Removal of code dependent on line numbers, file names, etc.
      * ... and many others.
  * Comparing programs for semantic equality. Programs are mostly compared
    instruction-by-instruction.

## Development

For a better developer experience, there is a development container image
prepared that can be retrieved from DockerHub:
[https://hub.docker.com/r/viktormalik/diffkemp-devel/](https://hub.docker.com/r/viktormalik/diffkemp-devel/)

After that, the container can be run using

    docker/diffkemp-devel/run-container.py [--llvm-version LLVM_VERSION]
                                           [--build-dir BUILD_DIR]
                                           [--diffkemp-dir DIFFKEMP_DIR]
                                           [--image IMAGE]

The script mounts the current directory as a volume inside the container.
Then it automatically builds SimpLL in `BUILD_DIR` (default is "build") using
`LLVM_VERSION` (default is the newest supported version) and installs DiffKemp.

If running multiple containers at the same time, you need to specify a unique
`BUILD_DIR` for each one.

If running the container from a different directory than the root DiffKemp
directory, you need to specify where DiffKemp is located using the
`--diffkemp-dir` option.

By default, the DockerHub image is used, but a custom image may be set using
the `--image` option.

### Tests

The project contains unit and regression testing using pytest that can be run
by:

    pytest tests

The tests require the sources of the following kernel versions to be stored and
configured in `kernel/linux-{version}` directories:
* 3.10 (upstream kernel)
* 4.11 (upstream kernel)
* 3.10.0-514.el7 (CentOS 7.3 kernel)
* 3.10.0-693.el7 (CentOS 7.4 kernel)
* 3.10.0-862.el7 (CentOS 7.5 kernel)
* 3.10.0-957.el7 (CentOS 7.6 kernel)

The required configuration of each kernel can be done by running:

    make prepare
    make modules_prepare

## Contributors

The list of code and non-code contributors to this project, in pseudo-random
order:
- Viktor Malík
- Tomáš Glozar
- Tomáš Vojnar
- Petr Šilling
- Tatiana Malecová
- Jakub Rozek

## Publications and talks

There is a number of publications and talks related to DiffKemp:
- ICST'21 [paper](https://ieeexplore.ieee.org/stamp/stamp.jsp?arnumber=9438578)
  and [talk](https://zenodo.org/record/4658966):
  Malík, V., Vojnar, T.: Automatically Checking Semantic Equivalence between
  Versions of Large-Scale C Projects + the related
- [DevConf.CZ'19 talk](https://www.youtube.com/watch?v=PUZSaLf9exg)
