# DiffKemp architecture

```mermaid
---
# This code renders an image, that does not show on the GitHub app, use
# a browser to see the image.
title: Simplified DiffKemp architecture
---
flowchart TB
  subgraph dk["DiffKemp"]
    sg -- "snapshots" --> sc -- "semantic differences" --> rv
    sg["1.Snapshot generation"]
    sc["2.Snapshot comparison"]
    rv["3.Result visualisation"]
    subgraph simpll["SimpLL"]
      ma("ModuleAnalysis")
      dfc("DifferentialFunctionComparator")
      cc("CustomPatternComparator")
      ma -.-> dfc -.-> cc
    end
  end
  subgraph llvm["LLVM project"]
    direction TB
    clang("clang -emit-llvm")
    opt("opt")
    fc("FunctionComparator")
    %% invisible edges helps top to bottom orientation
    clang ~~~ opt ~~~ fc
  end
  sg -.-> clang
  sg -.-> opt
  sc -.-> simpll
  dfc -.-> fc

%% style
classDef mono font-family:monospace
class ma,dfc,fc,cc,clang,opt mono
```

DiffKemp consists of several components:
- **Python component** (located in the `diffkemp/` directory): Responsible for
  the processing of user inputs, compiling projects into snapshots, aggregating
  results of semantic comparison, and generating reports.
- **SimpLL library** (located in the `diffkemp/simpll/` directory): The core of
  DiffKemp, written in C++ for performance reasons. It simplifies and
  semantically compares two versions of the project symbols.
- **Result viewer** (located in the `view/` directory): A web application,
  written in React and JavaScript, used to visualise the differences found
  during the comparison.

DiffKemp uses  [`CMake`](https://cmake.org/) as its build system and relies on
the [LLVM project](https://llvm.org/). Specifically, it uses
[LLVM intermediate representation](https://llvm.org/docs/LangRef.html) for
representing and comparing different versions of the analysed projects.

## Phases of DiffKemp

DiffKemp runs in phases, in which the different parts of DiffKemp play their
roles:
1. [**Snapshot generation**](#1-snapshot-generation):
   - The source code of the analysed project is compiled into LLVM IR using
     the [`clang`](https://clang.llvm.org/docs/ClangCommandLineReference.html)
     compiler.
   - After compilation, optimisation passes are run (using
     [`opt`](https://llvm.org/docs/CommandGuide/opt.html)) to simplify the LLVM
     IR.
   - The compiled project is saved to a directory, which we call a **snapshot**.
2. [**Snapshot comparison**](#2-snapshot-comparison):
   - Two snapshots (corresponding to different versions of the analysed project)
     are compared using the SimpLL library.
   - For each snapshot, the library simplifies (by applying multiple code
     transformations) and analyses the LLVM modules in which are located
     definitions of the analysed symbols. This is done using the
     `ModuleAnalysis` class.
   - The core comparison is handled by the `DifferentialFunctionComparator`
     class, which extends LLVM's [`FunctionComparator`](https://llvm.org/doxygen/classllvm_1_1FunctionComparator.html)
     class. The `FunctionComparator` class handles instruction-by-instruction
     comparison. DiffKemp extends this functionality by handling
     semantics-preserving changes (implemented as **built-in patterns**),
     allowing it to manage more complex changes/refactorings.
   - Additional **(custom) patterns** can be specified manually and passed to
     the comparison phase. These patterns are used if both the
     instruction-by-instruction comparison and the built-in patterns fail to
     detect semantic equality. In such cases, the `DifferentialFunctionComparator`
     uses the `CustomPatternComparator` class to attempt to match the changes
     against the provided patterns.
   - The results of the comparison of individual symbols are aggregated, and
     the found differences are reported to the user.
3. [**Result visualisation**](#3-result-visualisation):
   - The result viewer enables the user to interactively explore the found
     differences (the analysed symbols that were evaluated as semantically
     non-equal). It shows the relation between the analysed symbols and the
     location causing the semantic non-equality, allowing the user to display
     the source code of the analysed project versions with highlighted syntactic
     differences that are likely causing the semantic non-equality.

## 1. Snapshot generation

The snapshot generation phase consists of the following steps:

1. **Identifying relevant source files**:
   - Determine which parts of the project's code need to be compared. This
     typically means finding source files that contain definitions of the
     compared symbols specified by the user.
2. **Compiling source files to LLVM IR**:
   - The identified source files are compiled into LLVM modules containing
     human-readable LLVM IR using the `clang` compiler with the command
     `clang -S -emit-llvm ...`.
   - To make the oncoming comparison easier, [several optimisation passes](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/llvm_ir/optimiser.py)
     (`dce` - dead code elimination, `simplifycfg` - simplifying control flow
     graph, ...) are run on the compiled LLVM modules. The passes are run using
     the `opt` utility.
3. **Creating and saving a snapshot**:
   - A snapshot, representing one version of the program prepared for
     comparison, is created from the compiled files and saved in the specified
     directory.

```mermaid
---
# This code renders an image, that does not show on the GitHub app, use
# a browser to see the image.
title: Classes involved in snapshot generation
---
classDiagram
  direction LR
  Snapshot --> SourceTree
  SourceTree --> LlvmSourceFinder
  SourceTree <|-- KernelSourceTree
  LlvmSourceFinder <|-- SingleLlvmFinder
  LlvmSourceFinder <|-- WrapperBuildFinder
  LlvmSourceFinder <|-- KernelLlvmSourceFinder
  SingleLlvmFinder <|-- SingleCBuilder
  SourceTree
  <<abstract>> LlvmSourceFinder
```

Classes involved in snapshot generation:
- [`LlvmSourceFinder`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/llvm_ir/llvm_source_finder.py):
  - An abstract class with a concrete implementation based on the command used
    for snapshot generation.
  - It is responsible for finding LLVM modules containing specific symbol. For
    some commands, it also handles finding necessary project source files and
    their compilation to LLVM IR.
- [`SourceTree`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/llvm_ir/source_tree.py):
  - Represents the source tree of analysed project.
  - This class wraps the `LlvmSourceFinder` class and provides its functionality.
  - The derived class, [`KernelSourceTree`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/llvm_ir/kernel_source_tree.py),
    extends its functionality by enabling to retrieve modules containing
    definitions of sysctl options and kernel modules.
- [`Snapshot`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/snapshot.py)
  - Represents the snapshot, which contains:
    - Relevant source files, including those containing definitions of the
      compared symbols.
    - The source files compiled into LLVM IR, which are used for the actual
      comparison.
    - Metadata, saved in the snapshot directory as a `snapshot.yaml` file with
      the following structure:
      ```yaml
      # For more details, refer to the implementation
      - created_time: Date and time of snapshot creation (YYYY-MM-DD hh:mm:ss.sTZ)
        diffkemp_version: x.y.z
        list: # List of compared symbols (functions or sysctl options) and their metadata
        # In cases of function comparison, contains directly the list from the `functions` field`.
        - functions:                                                           
          - glob_var: null # Name of the global variable whose usage is analysed within the function (only for sysctl).
            name: Name of function
            llvm: Relative path to module containing the function
            tag: null # Only for sysctl, describes if the function is proc handler or uses the sysctl data variable
          # Only for sysctl
          sysctl: Sysctl option name
        list_kind: Type of comparison (function/sysctl)
        llvm_source_finder:
          kind: Used LlvmSourceFinder class
        llvm_version: XX
        source_dir: Absolute path to the project's source directory
      ```

### a) Snapshot generation of `make`-based projects (`build`)

The `build` command is used to create snapshots from `make`-based projects.
The process includes the following steps:

1. The `make` command is executed on the project with [`cc_wrapper`](#cc-wrapper)
   used as the compiler instead of a standard compiler like `gcc`.
2. By running `make`, the project's source files are compiled to LLVM modules by
   the `cc_wrapper` script.
3. After `make` completes the compilation, the resulting LLVM modules are
   optimised using `opt` tool.
4. The `WrapperBuildFinder` class is used to identify the LLVM modules
   containing definitions of symbols specified by the user.
5. A snapshot is generated from the identified LLVM modules.

```mermaid
---
# This code renders an image, that does not show on the GitHub app, use
# a browser to see the image.
title: Simplified sequence diagram for creating a snapshot from make-based project
config:
  sequence:
    mirrorActors: false
---
sequenceDiagram
  actor User
  User->>+build: build(project source directory)
  build->>+make: make CC=cc_wrapper ...
  loop
    make->>+cc_wrapper: *.c + options ...
    cc_wrapper->>+clang: clang -emit-llvm -S *.c + options ...
    clang-->>-cc_wrapper: *.ll
    cc_wrapper-->>-make: *.ll
  end
  make-->>-build: *.ll
  participant O as opt
  loop for each LLVM module
    build->>+O: *.ll
    O-->>-build: optimised *.ll
  end
  build->>build: snapshot = generate_from_function_list()
  build-->>-User: snapshot directory
```

#### `cc_wrapper`

The [`cc_wrapper`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/building/cc_wrapper.py)
is a Python script that wraps the `gcc` compiler. If [RPython](https://rpython.readthedocs.io/en/latest/)
is installed on the user's system, the `cc_wrapper` is compiled into a binary
form during DiffKemp's build process using RPython. This binary form speeds up
the snapshot generation process.

The `cc_wrapper` operates as follows:
- The `cc_wrapper` is invoked by `make` receiving the source file and options
  which would be used for compilation.
- The wrapper executes the original command using `gcc`, and additionally it
  compiles the C source file into LLVM module using `clang` and the received
  options.
- The user can optionally specify options to be added or removed when compiling
  the file to an LLVM module. These options are passed from the main DiffKemp
  script to the wrapper via system environment variables (the variables are
  prefixed with `DIFFKEMP_WRAPPER_`).
- The paths to the generated LLVM modules, along with flags indicating how they
  were created, are recorded in a file named `diffkemp-wdb`. The `f` flag
  indicates that the module was created by linking already compiled LLVM
  modules. The file format looks like this:
  ```
  o:/abs/path/to/module.ll
  f:/abs/path/to/linked_module.ll
  ...
  ```
  The `diffkemp-wdb` file is later used by the `WrapperBuildFinder` to locate
  modules containing the specified functions, which are then included in the
  final snapshot.

### aa) Snapshot generation of single C file (`build`)

The `build` command can also be used to create snapshots from a single C file.
It uses the [`SingleCBuilder`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/llvm_ir/single_c_builder.py)
class as the `LlvmSourceFinder`. The class also manages the compilation of the
C file into an LLVM module.

### b) Snapshot generation from the Linux kernel (`build-kernel`)

The `build-kernel` command is used to create snapshots from the Linux kernel's
source code. It compiles only the necessary files based on the symbols specified
by the user for comparison. The snapshot can be created using a list of
functions or sysctl parameters. The command uses
[`KernelLlvmSourceBuilder`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/llvm_ir/kernel_llvm_source_builder.py)
class as the `LlvmSourceFinder`. The process includes the following steps:

1. **Building a symbol cross-reference database** for the kernel using the
   [`cscope`](https://cscope.sourceforge.net/) tool. This database is then used
   to locate symbol definitions and usages within the kernel's source code.
2. **Identifying necessary files for the comparison** based on the provided
   symbol list.
   - If a **list of functions** is provided, the C source file containing the
     function definition is located using `cscope` for each function.
   - In case **list of sysctl parameters** is provided, the process is slightly
     different:
     1. The source file containing a sysctl table describing the parameter is
        identified and compiled into LLVM module.
     2. The proc handler function name (responsible for reading/writing the
        parameter value) and data variable (a global variable updated by the
        parameter) are extracted from the sysctl table.
     3. Source files containing the proc handler function and functions that
        interact with the data variable are identified using `cscope`.
3. The identified source **files are compiled into LLVM modules** using the
   following steps for each file:
   - The `make` tool is used to determine the compilation options required for
     the source file.
   - The source file is compiled into an LLVM module using `clang` with the
     discovered options. The module is then optimized using `opt` tool.
4. **A snapshot is created** from the compiled LLVM modules.

```mermaid
---
# This code renders an image, that does not show on the GitHub app, use
# a browser to see the image.
title: Simplified sequence diagram of kernel compilation to LLVM modules
config:
  sequence:
    mirrorActors: false
---
sequenceDiagram
  KernelLlvmSourceBuilder->>cscope: build cscope database
  loop for each symbol
    KernelLlvmSourceBuilder->>+cscope: get source file for symbol
    cscope-->>-KernelLlvmSourceBuilder: *.c
    KernelLlvmSourceBuilder->>+make: get command for compiling *.c file to object file
    make-->>-KernelLlvmSourceBuilder: command
    KernelLlvmSourceBuilder->>+clang: clang -emit-llvm -S *.c -o *.ll + command's options
    clang->>-KernelLlvmSourceBuilder: *.ll
    participant O as opt
    KernelLlvmSourceBuilder->>+O: *.ll
    O-->>-KernelLlvmSourceBuilder: optimised *.ll
  end
```

### c) Snapshot generation from a single LLVM IR file (`llvm-to-snapshot`)

The `llvm-to-snapshot` command is used to create snapshots from a single
existing LLVM module. It uses the
[`SingleLlvmFinder`](https://github.com/diffkemp/diffkemp/blob/master/diffkemp/llvm_ir/single_llvm_finder.py)
class as the `LlvmSourceFinder`.

## 2. Snapshot comparison

WIP

## 3. Result visualisation

The result visualisation phase is invoked via `view` subcommand and consists of
the following steps:

1. Preparation for visualisation:
   - Syntactic differences of the analysed functions are created. The result
     viewer later uses them.
   - Project source files and the differences are copied to a directory
     accessible by the result viewer application.
2. Execution of [the result viewer](#result-viewer):
   - A result viewer is a web application accessible at
     <http://localhost:3000/>. It enables users to browse and view information
     about the found semantic differences.

### Result viewer

The result viewer is a client-side web application developed in JavaScript using
the [React](https://react.dev/) framework and bootstrapped with
[Create React App](https://create-react-app.dev/). The application runs locally
on the user's computer using a Python HTTP server.

The application loads a file describing the results of the project's comparison
(`diffkemp-out.yaml`) and displays a list of the compared symbols evaluated to
contain semantic differences. Upon selecting a specific symbol, the application
lists places (functions, types, macros) in which semantic differences were
located. For a selected place (difference), it shows:

- Call stack containing symbols leading from the compared symbol to the symbol
  where the semantic difference was located. The call stack is interactive,
  allowing users to select a symbol. By default, the symbol with the semantic
  differences is selected.
- Source code view displaying the selected symbol's definition in both versions
  of the project source code. If the selected symbol is where the semantic
  difference was located, the syntax differences are highlighted. For other
  symbols, the usage location of the next symbol from the call stack is
  highlighted.

The application implementation is located in `/view/` subdirectory. The main
components of the application are:

- [`ResultViewer`](https://github.com/diffkemp/diffkemp/blob/master/view/src/components/ResultViewer.jsx):
  The main component that loads the comparison results and represents them using
  the [Result](https://github.com/diffkemp/diffkemp/blob/master/view/src/Result.js)
  class. It renders components for selecting functions or a visualising the
  selected difference.
- [`ResultNavigation`](https://github.com/diffkemp/diffkemp/blob/master/view/src/components/ResultNavigation.jsx):
  Informs the user about the currently shown difference and allows navigation
  between found differences.
- [`FunctionListing`](https://github.com/diffkemp/diffkemp/blob/master/view/src/components/FunctionListing.jsx):
  Rendered when no specific place with semantic difference is selected. It lists
  the compared symbols (functions) evaluated as semantically different or
  symbols/places where semantic difference were located for a selected compared
  symbol.
- [`Difference`](https://github.com/diffkemp/diffkemp/blob/master/view/src/components/Difference.jsx):
  Rendered when a difference is selected. It manages the rendering of the
  `Callstack` and `Code` components.
- [`Callstack`](https://github.com/diffkemp/diffkemp/blob/master/view/src/components/Callstack.jsx):
  Renders the call stack for the selected difference, allowing users to select a
  symbol from the call stack, which updates the displayed source code in the
  `Code` component.
- [`Code`](https://github.com/diffkemp/diffkemp/blob/master/view/src/components/Code.jsx):
  Fetches the files necessary for code visualisation. This includes source files
  with the selected symbol's definition and a diff file with syntax differences
  between versions of the symbol's definitions.
- [`DiffViewWrapper`](https://github.com/diffkemp/diffkemp/blob/master/view/src/components/DiffViewWrapper.jsx):
  Displays the source code from both project versions containing the definition
  of the selected symbol. It uses the [react-diff-view](https://github.com/otakustay/react-diff-view)
  package.

```mermaid
---
title: Simplified component tree of the result viewer
# This code renders an image, which does not show on the GitHub app, use
# a browser to see the image.
---
flowchart TD
  App --> ResultViewer
  ResultViewer --> ResultNavigation
  ResultViewer -- [difference is not selected] --> FunctionListing
  ResultViewer -- [difference is selected] --> Difference
  Difference --> Callstack
  Difference --> Code
  Code --> DiffViewWrapper
```
