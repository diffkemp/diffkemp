# Usage

To compare two versions of a program for semantic differences using DiffKemp
use the following commands:

1. [Snapshot generation command](#1-snapshot-generation-commands)  
   a) [For `make`-based projects](#a-build-snapshot-generation-of-make-based-projects)  
   b) [For the Linux kernel](#b-build-kernel-snapshot-generation-from-the-linux-kernel)  
   c) [For a single LLVM IR file](#c-llvm-to-snapshot-snapshot-generation-from-a-single-llvm-ir-file)
2. [Semantic comparison command](#2-semantic-comparison-command)
3. [Command for visualisation of the found differences](#3-visualisation-of-the-found-differences)

## 1. Snapshot generation commands

These commands compile the compared project versions into LLVM IR and create
so-called *snapshots* which contain the relevant LLVM IR files and additional
metadata. There are several options for snapshot generation, choose the one
which fits your needs. The chosen command should be run twice, once for each of
the compared versions.

### a) `build`: snapshot generation of `make`-based projects

```sh
diffkemp build PROJ_DIR SNAPSHOT_DIR [SYMBOL_LIST]
```

This is the default snapshot generation command for `make`-based projects. It
takes the project located in `PROJ_DIR`, builds it into LLVM IR, and creates
a snapshot for comparing the semantics of functions from `SYMBOL_LIST` (if no
list is given, all exported functions from the project are considered).
The snapshot is stored in `SNAPSHOT_DIR`. Warning: if `SNAPSHOT_DIR` exists,
it will be overwritten.

It also has additional options to configure the project build, see `diffkemp
build --help` for the complete list.

The command can be also used to generate a snapshot from a single C file.
In this case, the path to the file should be given in place of `PROJ_DIR`.

### b) `build-kernel`: snapshot generation from the Linux kernel

```sh
diffkemp build-kernel KERNEL_DIR SNAPSHOT_DIR SYMBOL_LIST
```

A command similar to `build` which is specialized for building snapshots
from the Linux kernel. Its main advantage is that it does not build the
entire kernel, only the files containing functions from `SYMBOL_LIST`. The
kernel source to build must be properly configured (by `make prepare`) and
all the tools necessary for building kernel must be installed. It also
requires the `cscope` tool to be installed.

#### Comparing sysctl options

DiffKemp supports also a comparison of semantics of sysctl options. The list
of the options to compare can be passed via `SYMBOL_LIST` to the `build-kernel`
command. In such case, use `--sysctl` switch to generate snapshot for sysctl
parameter comparison. The `compare` command is used in normal way.

Sysctl option comparison compares the semantics of the proc handler function and
semantics of all functions using the data variable that the sysctl option sets.

It is possible to use patterns to specify a number of multiple sysctl options at
once such as:

- `kernel.*`
- `kernel.{sysctl-1|sysctl-2}`

Currently, these sysctl option groups are supported: `kernel.*`,
`vm.*`, `fs.*`, `net.core.*`, `net.ipv4.conf.*`.

### c) `llvm-to-snapshot`: snapshot generation from a single LLVM IR file

```sh
diffkemp llvm-to-snapshot PROJ_DIR LLVM_FILE SNAPSHOT_DIR SYMBOL_LIST
```

This command can be used if the project is already compiled into a single
LLVM IR file.
The file name is given in `LLVM_FILE` and must be relative to `PROJ_DIR`.
The remaining options are the same as for the other commands.

## 2. Semantic comparison command

```sh
diffkemp compare SNAPSHOT_DIR_1 SNAPSHOT_DIR_2
```

This command takes two snapshots and compares them for semantic equality.

Syntactic diffs of the discovered differences are stored in separate files
(one file for each compared function that is different) in a newly created
directory. The name of the directory can be specified using the `-o` option,
otherwise it is generated automatically. The `--stdout` option causes the diffs
to be printed to standard output.

## 3. Visualisation of the found differences

```sh
diffkemp view COMPARE_OUTPUT_DIR
```

Additionally, you can run the **result viewer** to get a visualisation of the
found differences. The command takes the directory with the output of the
compare command. It prepares the necessary files and runs a static server.
The command displays the URL that you can use to access the result viewer.
