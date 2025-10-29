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

The command can be also used to generate a snapshot from a single C file.
In this case, the path to the file should be given in place of `PROJ_DIR`.

#### Options

- `PROJ_DIR`: Path to a project's root directory or a path to a single C file.
- `SNAPSHOT_DIR`: Output directory for storing the created snapshot.
- `SYMBOL_LIST`: Path to a file containing a list of symbols (each symbol on
   a single line) which should be prepared for comparison.
- `--reconfigure`: Reconfigures autotools-based project with `CC=<DiffKemp
  compiler wrapper>`.
- `--target TARGET`: Allows specifying `Makefile` targets which should be used
  to build the snapshot from the project.
- `--build-program BUILD_PROGRAM`: `make` tool to be used for building
  (default `make`).
- `--build-file BUILD_FILE`: Filename of the project's `Makefile` to be used
  for the build.

##### cc_wrapper options

- `--no-opt-override`: Uses optimisation options provided in the project's
  `Makefile` or specified with `--clang-append="-OX"`. With this option,
  DiffKemp can potentially handle more complex refactoring (report fewer false
  positives). However, this may reduce precision in identifying the exact
  location (e.g. function or macro) of a semantic difference.
- `--clang-append CLANG_APPEND`: Allows specifying options that will be
  appended to `clang` when compiling source files to LLVM IR (e.g. optimisation
  options).
- `--clang-drop CLANG_DROP`: Allows specifying options that will be dropped
  from `clang` when compiling source files to LLVM IR. Useful when
  `--no-opt-override` option is used to drop some compiler options specified
  in the project's `Makefile` which would be otherwise used by DiffKemp (e.g.
  options not supported by `clang` which could break generation of the
  snapshot).
- `--clang CLANG`: `clang` compiler to be used for building the project to
  LLVM IR (default `clang`).
- `--llvm-link LLVM_LINK`: `llvm-link` to be used for linking of LLVM IR files
  (default `llvm-link`).
- `--llvm-dis LLVM_DIS`: `llvm-dis` to be used for `bc` file disassembly
  (default `llvm-dis`).

### b) `build-kernel`: snapshot generation from the Linux kernel

```sh
diffkemp build-kernel KERNEL_DIR SNAPSHOT_DIR SYMBOL_LIST
```

A command similar to `build` which is specialized for building snapshots
from the Linux kernel. Its main advantage is that it does not build the
entire kernel, only the files containing functions from `SYMBOL_LIST`. The
kernel source to build must be properly configured (by `make prepare`) and
all the tools necessary for building kernel must be installed. It also
requires the `cscope` tool to be installed. At the moment, the command only
supports the x86 architecture.

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

#### Options

- `KERNEL_DIR`: Path to a kernel's root directory.
- `SNAPSHOT_DIR`: Output directory for storing the created snapshot.
- `SYMBOL_LIST`: Path to a file containing a list of symbols (each symbol on
  a single line) to be prepared for comparison.
  In case `--sysctl` is used, the list is interpreted as a list of sysctl
  parameters.
- `--sysctl`: Compares sysctl option.
- `--no-source-dir`: Does not store the path to the source kernel directory in
  the snapshot. This is useful if the comparison is done on a different system
  than building the snapshot (i.e. the path to the original kernel tree does
  not exist anymore). May lead to generating more false positives in some
  situations.

### c) `llvm-to-snapshot`: snapshot generation from a single LLVM IR file

```sh
diffkemp llvm-to-snapshot PROJ_DIR LLVM_FILE SNAPSHOT_DIR SYMBOL_LIST
```

This command can be used if the project is already compiled into a single
LLVM IR file.
The file name is given in `LLVM_FILE` and must be relative to `PROJ_DIR`.

#### Options

- `PROJ_DIR`: Path to a project's root directory.
- `LLVM_FILE`: Path to the LLVM IR file relative to the project's root
  directory.
- `SNAPSHOT_DIR`: Output directory for storing the created snapshot.
- `SYMBOL_LIST`: Path to a file containing a list of symbols (each symbol on
  a single line) which should be prepared for comparison.

## 2. Semantic comparison command

```sh
diffkemp compare SNAPSHOT_DIR_1 SNAPSHOT_DIR_2
```

This command takes two snapshots and compares them for semantic equality.

Syntactic diffs of the discovered differences are stored in separate files
(one file for each compared function that is different) in a newly created
directory. The name of the directory can be specified using the `-o` option,
otherwise it is generated automatically.

### Options

- `SNAPSHOT_DIR_1`, `SNAPSHOT_DIR_2`: Paths to directories containing snapshots
  of the project's version to be compared.
- `-o`, `--output-dir OUTPUT_DIR`: Name of the output directory.
- `-p`, `--custom-patterns CUSTOM_PATTERNS`: Path to a custom pattern file or
  a configuration.
- `--no-show-diff`: Do not show/create syntactic diffs for symbols evaluated as
  semantically different.
- `--full-diff`: Shows syntactic diff for all functions (even semantically
  equivalent ones).
- `--enable-pattern`, `--disable-pattern`: Enables/disables specified built-in
  pattern(s).
- `--enable-all-patterns`,`--disable-all-patterns`: Enables/disables all
  supported built-in patterns. Be careful, `--enable-all-patterns` also enables
  patterns which are not on by default and may not be semantics preserving.
- `--report-stat`: Reports basic statics of the analysis:
  - `equal`: Number of compared symbols evaluated as equal.
  - `not equal`: Number of compared symbols evaluated as not equal.
  - `empty diff`: Reports for how many not-equal symbols were found
    differences in symbols with no syntax difference.
  - `unknown`: Represents how many symbols DiffKemp could not evaluate
    (mainly caused by the symbol occurrence only in one version of
    the program),
  - `errors`: Represents symbols for which the comparison failed.
- `--extended-stat`: Reports extended statistics -- total number of compared
  functions (including called ones) and other information, such as the number
  of compared instructions and the total number of found differences. Beware
  that this may increase the analysis time.
- `--show-errors`: Show functions that are either unknown or ended with an
  error in statistics.
- `--stdout`: Prints results to standard output instead of saving them to the
  directory.
- `-f`, `--function FUNCTION`: Compares only the specified function.
- `--disable-simpll-ffi`: For development, calls SimpLL library through binary
  instead of FFI.
- `--regex-filter REGEX_FILTER`: Filters function diffs by the given regex.
- `--source-dirs SOURCE_DIRS SOURCE_DIRS`: Allows specifying of root
  directories for the compared projects.
- `--output-llvm-ir`: Outputs each simplified module to a file.
- `--print-asm-diffs`: Prints raw inline assembly differences (does not apply
  to macros).
- `--enable-module-cache`: Loads frequently used modules to memory and uses
  them in SimpLL.


## 3. Visualisation of the found differences

```sh
diffkemp view COMPARE_OUTPUT_DIR
```

Additionally, you can run the **result viewer** to get a visualisation of the
found differences. The command takes the directory with the output of the
compare command. It prepares the necessary files and runs a static server.
The command displays the URL that you can use to access the result viewer.

### Options

- `COMPARE_OUTPUT_DIR`: Path to the output directory of `diffkemp compare`
  command.
- `--devel`: Runs the viewer using a development server (useful for
  development/debugging).
