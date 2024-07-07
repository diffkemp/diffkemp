# DiffKemp development

This document will guide you through DiffKemp development. It contains
information about:

- [How to set up development environment](#development-environment)
- [How to build DiffKemp](#build)
- [Where the tests are located and how to run them](#tests)
- [Links to tools for performing experiments](#tools-for-performing-experiments)
- [Useful links to learn more](#useful-links)

## Development environment

For developing DiffKemp, you can use:

- [Nix Flake](#nix)
- Your [local environment](#local-development-environment) (installing
  dependencies directly to your system)
- [(Docker container)](#docker)

### Nix

We provide [Nix flakes](https://nixos.wiki/wiki/Flakes) for building and testing
DiffKemp.

First, it is necessary to [install Nix](https://nixos.org/download.html) and
enable flakes:

```sh
mkdir -p ~/.config/nix
echo "experimental-features = nix-command flakes" >> ~/.config/nix/nix.conf
```

Then, building DiffKemp is as simple as running one of the following commands:

```sh
nix build                    # <- uses latest LLVM
nix build .#diffkemp-llvm14
```

This will create a new folder `result/` containing the pre-built DiffKemp which
can be then executed by:

```sh
result/bin/diffkemp ...
```

> [!TIP]
> Rather than building DiffKemp with Nix, it is better to use Nix development
> environment described below, because the DiffKemp build created by Nix has
> following drawbacks:
>
> - Currently it is not possible to run `view` subcommand (the result viewer).
> - You still need to have some dependencies installed on your system to be able
>   to use it (`clang`, `llvm`, and dependencies specific to the project you
>   want to analyse).

#### Nix as development environment

It is also possible to use Nix as a development environment:

```sh
nix develop                   # <- uses latest LLVM
nix develop .#diffkemp-llvm14
```

This will enter a development shell with all DiffKemp dependencies
pre-installed. You can then follow the [standard build
instructions](#build) to build and install DiffKemp. The only
difference is that it is not possible to run `pip install` inside Nix shell
(because of the way Nix works) and it is necessary to use the built-in
`setuptoolsShellHook` function instead.

We also provide a special Nix environment for retrieving and preparing kernel
versions necessary for running [regression tests](#python-tests)
(`nix develop .#test-kernel-buildenv`).

### Local development environment

You can also develop DiffKemp directly. For this you need to install the
necessary [dependencies](installation.md#dependencies) to your system and then
[build DiffKemp](#build).

### Docker

> [!CAUTION]
> The docker container is not currently maintained and it is recommended to use
> [Nix flake](#nix) instead.

We also provide development container image prepared that can be retrieved from
DockerHub:
[https://hub.docker.com/r/viktormalik/diffkemp-devel/](https://hub.docker.com/r/viktormalik/diffkemp-devel/)

After that, the container can be run using

```txt
docker/diffkemp-devel/run-container.py [--llvm-version LLVM_VERSION]
                                       [--build-dir BUILD_DIR]
                                       [--diffkemp-dir DIFFKEMP_DIR]
                                       [--image IMAGE]
```

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

## Build

To build DiffKemp, use the following commands:

```sh
cmake -S . -B build -GNinja -DBUILD_VIEWER=On
ninja -C build
pip install -e . # In case of Nix use setuptoolsShellHook instead
```

- `-DBUILD_VIEWER=On`: This flag will install packages and build the result
  viewer. It will try to install the packages on every `cmake` run, so you may
  want to use `-DBUILD_VIEWER=Off` instead.

If you make changes to the SimpLL library, you will need to rebuild it by
running `ninja -C build`. We are using [CFFI](https://cffi.readthedocs.io/en/stable/)
for accessing the library from the Python. The CFFI is not rebuilt by default,
so even if you rebuild the library, the changes will not be visible when running
`diffkemp compare ...`. Currently, you have multiple options to overcome this:

1. Use `diffkemp compare --disable-simpll-ffi ...` which will run SimpLL
  through a binary instead of the CFFI.
2. On some OS distributions, it is enough to run `pip install -e .` again.
3. Another option is to to run `rm build/_simpll.abi3.so` and then run `cmake`
  again with `-DSIMPLL_REBUILD_BINDINGS=On` flag. After this, when you make
  changes to the SimpLL library, running `ninja -C build` should be enough and
  it should rebuild the CFFI, and the changes should be visible when running
  `diffkemp compare ...`.
  (Note: This does not seems to be working in the [Nix development environment](#nix-as-development-environment),
  so use option 1 instead.)

## Tests

The project contains multiple tests:

- [Python tests](#python-tests),
- [Tests for SimpLL library](#tests-for-the-simpll-library),
- [Tests for the result viewer](#tests-for-the-result-viewer).

### Python tests

The tests use pytest and can be run by:

```sh
pytest tests
```

There are 2 types of tests:

- Unit tests (located in `tests/unit_tests/`)
- Regression tests (located in `tests/regression/`)

The tests require the sources of the following kernel versions to be stored and
configured in `kernel/linux-{version}` directories:

- 3.10 (upstream kernel)
- 4.11 (upstream kernel)
- 3.10.0-514.el7 (CentOS 7.3 kernel)
- 3.10.0-693.el7 (CentOS 7.4 kernel)
- 3.10.0-862.el7 (CentOS 7.5 kernel)
- 3.10.0-957.el7 (CentOS 7.6 kernel)
- 4.18.0-80.el8 (RHEL 8.0 kernel)
- 4.18.0-147.el8 (RHEL 8.1 kernel)
- 4.18.0-193.el8 (RHEL 8.2 kernel)
- 4.18.0-240.el8 (RHEL 8.3 kernel)

The required configuration of each kernel can be done by running:

```sh
make prepare
make modules_prepare
```

The [rhel-kernel-get](https://github.com/viktormalik/rhel-kernel-get) script can
also be used to download and configure the aforementioned kernels.

> [!TIP]
> Since CentOS 7 kernels require a rather old GCC 7, the most convenient way to
> download the kernels is to use the prepared [Nix environment](#nix-as-development-environment)
> by running
>
> ```sh
> nix develop .#test-kernel-buildenv
> ```
>
> and using `rhel-kernel-get` inside the environment to retrieve the above
> kernels.

### Tests for the SimpLL library

Tests are located in `tests/unit_tests/simpll/` directory and they can be run
by:

```sh
ninja -C build test
```

### Tests for the result viewer

The result viewer contains unit tests and integration tests located in
`view/src/tests/` directory and they can be run by:

```sh
npm --prefix view test -- --watchAll
npm --prefix view run cypress:run
```

## Tools for performing experiments

We have some tools which can be handy when developing DiffKemp:

- [Tool for building and subsequently comparing multiple versions of a provided
   C project](https://github.com/zacikpa/diffkemp-analysis)
- [Tool for running DiffKemp with EqBench dataset of equivalent and
   non-equivalent program pairs](https://github.com/diffkemp/EqBench-workflow):
   You can use this tool if you make bigger changes to the SimpLL library to
   ensure that the evaluation results are not worse than before.

## Useful links

- [LLVM Language Reference Manual](https://llvm.org/docs/LangRef.html)
- [LLVM API documentation](https://llvm.org/doxygen/index.html)
