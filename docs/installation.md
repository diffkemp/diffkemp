# Installation from source

Besides installation from the [RPM package](https://copr.fedorainfracloud.org/coprs/viktormalik/diffkemp/)
it is possible to build and install DiffKemp also from source.

## Dependencies

Currently, DiffKemp runs on Linux and needs the following software installed:

- Clang and LLVM (supported versions are 12 - 18)
* Z3 SMT solver and its C++ bindings (packages `z3` and `z3-devel` in Fedora)
- Python 3 with CFFI (package `python3-cffi` in Fedora and Debian)
- Python packages from `packages` field in `pyproject.toml` (automatically installed when running `pip install .`)
- CScope (when comparing versions of the Linux kernel)
- GoogleTest (gtest) for running the C++ tests (can be vendored by using
  `-DVENDOR_GTEST=On` in the `cmake` command)
- CMake and Ninja build system for building
- Node.js (>= 20.x) and npm for running the result viewer

> [!NOTE]
> Optionally, to create *snapshots* of projects faster when running `diffkemp build`
> command, you need to have [RPython](https://rpython.readthedocs.io/en/latest/)
> installed. However, it can be problematic to install it on modern OS
> distributions because it depends on Python 2.
> (RPython is used to compile a part of Python code to binary, which accelerates
> the creation of snapshots).

### On Ubuntu

```sh
# Installation of dependencies
apt install -y cmake ninja-build llvm clang g++ libgtest-dev python3-pip python-is-python3 z3 libz3-dev python3-cffi
# Installation of the result viewer dependencies (node, npm)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
nvm install 20
```

### On Fedora

```sh
# Installation of dependencies
dnf install -y cmake ninja-build llvm-devel clang g++ python3-devel diffutils python3-pip gtest-devel z3 z3-devel python3-cffi python3-setuptools
# Installation of the result viewer dependencies (node, npm)
dnf install nodejs
```

> [!NOTE]
> Your system might come with LLVM version that is not yet supported by
> DiffKemp. In such case, you will need to install the supported LLVM
> version (by e.g. `dnf install llvmXX-devel` where XX is the version) and
> ensure DiffKemp uses correct version (e.g. by prefixing the PATH with path
> to the LLVM binaries - `PATH="/usr/lib64/llvmXX/bin/:$PATH"`).

## Building and installation

Build with installation can be done by running:

```sh
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DBUILD_VIEWER=On
ninja -C build install
pip install .
```

You can omit `-DBUILD_VIEWER=On` flag in `cmake` if you do not want to use the
result viewer (dependencies won't be installed for that and the viewer won't be
built).

Then you can use the `diffkemp` command from any location.
