![Build](https://github.com/viktormalik/diffkemp/actions/workflows/ci.yml/badge.svg?branch=master)

# DiffKemp

DiffKemp is a framework for automatic static analysis of semantic differences
between different versions of projects written in C, with main focus on the
Linux kernel.

The main use-case of DiffKemp is to compare selected functions and configuration
options in two versions of a project and to report any discovered semantic
differences.

> [!WARNING]
> DiffKemp is incomplete in its nature, hence may provide false positive
results (i.e. claiming that functions are not equivalent even though they are).
This especially happens with complex refactorings.

## Installation

You can install DiffKemp:

- By building it manually [from source](docs/installation.md)
- From a prepared RPM package that can be installed from our
  [Copr repository](https://copr.fedorainfracloud.org/coprs/viktormalik/diffkemp/):

  ```sh
  # Enabling the DiffKemp repository
  dnf install -y dnf-plugins-core
  dnf copr enable -y viktormalik/diffkemp
  # Installing DiffKemp
  dnf install -y diffkemp
  ```

## Usage
DiffKemp runs in two phases:

- **Snapshot generation** compiles the compared project versions into LLVM IR
  and creates so-called *snapshots* which contain the relevant LLVM IR files and
  additional metadata. (DiffKemp needs the analysed project to be compiled with
  debugging information in order to work properly.)

  There are several options for snapshot generation:
  - ```
    diffkemp build PROJ_DIR SNAPSHOT_DIR [SYMBOL_LIST]
    ```
    is the default snapshot generation command for `make`-based projects. It
    takes the project located in `PROJ_DIR`, builds it into LLVM IR, and creates
    a snapshot for comparing semantics of functions from `SYMBOL_LIST` (if no
    list is given, all exported functions from the project are considered). The
    snapshot is stored in `SNAPSHOT_DIR`. Warning: if `SNAPSHOT_DIR` exists, it
    will be rewritten.

    The command should be run twice, once for each of the compared versions.  It
    also has additional options to configure the project build, see `diffkemp
    build --help` for the complete list.

    The command can be also used to generate a snapshot from a single C file.
    In this case, the path to the file should be given in place of `PROJ_DIR`.

  - ```
    diffkemp build-kernel KERNEL_DIR SNAPSHOT_DIR SYMBOL_LIST
    ```
    is a command similar to `build` which is specialized for building snapshots
    from the Linux kernel. Its main advantage is that it does not build the
    entire kernel, only the files containing functions from `SYMBOL_LIST`. The
    kernel source to build must be properly configured (by `make prepare`) and
    all the tools necessary for building kernel must be installed.

  - ```
    diffkemp llvm-to-snapshot PROJ_DIR LLVM_FILE SNAPSHOT_DIR SYMBOL_LIST
    ```
    can be used if the project is already compiled into a single LLVM IR file.
    The file name is given in `LLVM_FILE` and must be relative to `PROJ_DIR`.
    The remaining options are the same as for the other commands.

- **Semantic comparison** takes two snapshots and compares them for semantic
  equality. It is invoked via:
  ```
  diffkemp compare SNAPSHOT_DIR_1 SNAPSHOT_DIR_2
  ```

  To show syntactic diffs of the discovered differences, use the `--syntax-diff`
  option. The diffs are stored in separate files (one file for each compared
  function that is different) in a newly created directory. The name of the
  directory can be specified using the `-o` option, otherwise it is generated
  automatically. The `--stdout` option causes the diffs to be printed to
  standard output.

Additionally, you can run **result viewer** to get a visualisation of the found
differences.

- **Result viewer** takes the directory with the output of the compare phase. 
  It is invoked via:
  ```
  diffkemp view COMPARE_OUTPUT_DIR
  ```

  It prepares the necessary files and runs a static server. The command displays the URL that you can use to access the result viewer.

### Comparing sysctl options

Apart from comparing specific functions, DiffKemp supports comparison of
semantics of sysctl options. The list of the options to compare can be passed
via `SYMBOL_LIST` to the `build-kernel` command. In such case, use `--sysctl`
switch to generate snapshot for sysctl parameter comparison. The `compare`
command is used in normal way.

Sysctl option comparison compares semantics of the proc handler function and
semantics of all functions using the data variable that the sysctl option sets.

It is possible to use patterns to specify a number of multiple sysctl options at
once such as:
* `kernel.*`
* `kernel.{sysctl-1|sysctl-2}`

Currently, these sysctl option groups are supported: `kernel.*`,
`vm.*`, `fs.*`, `net.core.*`, `net.ipv4.conf.*`.

## How does it work?

The main focus of DiffKemp is high scalability, such that it can be applied to
large-scale projects containing a lot of code. To achieve that, the analysed
functions are first compiled into LLVM IR, then several code transformations are
applied, and finally the comparison itself is performed.

Wherever possible, DiffKemp tries to compare instruction-by-instruction (at the
level of LLVM IR) which is typically sufficient for most of the code. When not
sufficient, DiffKemp tries to apply one of the built-in or user-supplied
*semantics-preserving patterns*. If no pattern can be applied,
the relevant diffs are reported to the user.

## Development

- [Development guide](docs/development.md)

## Contributors

The list of code and non-code contributors to this project, in pseudo-random
order:
- Viktor Malík
- Tomáš Glozar
- Tomáš Vojnar
- Petr Šilling
- Pavol Žáčik
- Lukáš Petr
- Tatiana Malecová
- Jakub Rozek

## Publications and talks

There is a number of publications and talks related to DiffKemp:

- ICST'21 [paper](https://ieeexplore.ieee.org/document/9438578)
  and [talk](https://zenodo.org/record/4658966):  
  V. Malík and T. Vojnar, "Automatically Checking Semantic Equivalence between
  Versions of Large-Scale C Projects," 2021 14th IEEE Conference on Software
  Testing, Verification and Validation (ICST), 2021, pp. 329-339.
- [DevConf.CZ'19 talk](https://www.youtube.com/watch?v=PUZSaLf9exg).
- NETYS'22
  [paper](https://link.springer.com/chapter/10.1007/978-3-031-17436-0_18) and
  [talk](https://www.youtube.com/watch?v=FPOUfgorF8s):  
  Malík, V., Šilling, P., Vojnar, T. (2022). Applying Custom Patterns in
  Semantic Equality Analysis. In: Koulali, MA., Mezini, M. (eds) Networked
  Systems. NETYS 2022.
