# Comparing Linux kernels

The main focus of DiffKemp is on Red Hat Enterprise Linux (RHEL), whose kernel
includes a list of functions (KABI -- Kernel Application Binary Interface)
whose semantics should remain the same across the lifetime of a single major
release. In this example, we will show how to compare two versions of the
kernel.

## 1. Downloading and preparing the kernels

Because getting and comparing the kernels requires additional dependencies
we will use [Nix development environment](../development.md#nix-as-development-environment).
After entering the development shell using `nix develop` command, we need to
build DiffKemp. Then we can use [`rhel-kernel-get`](https://github.com/viktormalik/rhel-kernel-get)
script for downloading and preparing the kernels. We will compare RHEL/CentOS
8.2 (`4.18.0-193.el8`) and 8.3 (`4.18.0-240.el8`) versions, which we can get by:

```sh
mkdir -p kernel
rhel-kernel-get --kabi 4.18.0-193.el8 -o kernel
rhel-kernel-get --kabi 4.18.0-240.el8 -o kernel
```

The `--kabi` option specifies that we want to download the list of functions
whose semantics should remain the same, and the `-o kernel` specifies the folder
to which we want to download the kernels. After the kernels are downloaded and
prepared, we can create *snapshots* from them and compare them.

## 2. Creating snapshots of the kernels

For creating snapshots of the kernels we won't be using `diffkemp build`
but `diffkemp build-kernel` command which is specifically made for the kernel:

```sh
mkdir -p snapshots
diffkemp build-kernel kernel/linux-4.18.0-193.el8/ snapshots/linux-4.18.0-193.el8/ kernel/linux-4.18.0-193.el8/kabi_whitelist_x86_64
diffkemp build-kernel kernel/linux-4.18.0-240.el8/ snapshots/linux-4.18.0-240.el8/ kernel/linux-4.18.0-240.el8/kabi_whitelist_x86_64
```

The `kernel/linux-4.18.0-XXX.el8/` directories are directories with
the downloaded and prepared 8.2 and 8.3 kernels,
the `snapshots/linux-4.18.0-XXX.el8/` are directories where the created
snapshots will be saved and `kernel/linux-4.18.0-XXX.el8/kabi_whitelist_x86_64`
files are files that contain the list of KABI symbols.

When running the command, we are informed which symbols were already found
and compiled into LLVM IR (LLVM IR is used for the comparison). For some
symbols we can get `source not found` note, this is normal, so do not worry.

> [!NOTE]
> If you get `source not found` note for all symbols, there is a problem.
> It could be caused by cancelling the `build-kernel` command prematurely
> which could result in the `cscope` cross-referencer, used by DiffKemp, not
> being run for all symbols. Removing the `kernel/linux-4.18.0-XXX.el8/cscope.files`
> file (replace `XXX` with `193` or `240` based on what is the problematic
> kernel) should fix it, but we will need to rerun the `build-kernel` command.

## 3. Comparing the kernels

Now we can use `compare` command to compare the created snapshots (the versions
of the kernel). It will compare functions from the KABI list.

```sh
diffkemp compare snapshots/linux-4.18.0-193.el8 snapshots/linux-4.18.0-240.el8 -o diff-linux
```

The command will notify us on standard output about functions for which it was
unable to determine semantic equivalency (e.g. because the compared symbol was
found only in one compared version). The command creates
`diff-linux` directory containing
information about the found semantic differences. The structure of the
directory is the same as was described in [the library example](musl_library.md#4-going-through-the-results).

## 4. Viewing the found differences

We can also visualise the differences with the result viewer using the following
command:

```sh
diffkemp view diff-linux
```

The command will launch a web application that we can access in our browser.
The viewer is more described in [the library example](musl_library.md#5-different-visualizations-of-the-differences)

## 5. Summary

That's all for the example of the kernel comparison. We learned, that:

- We can use [Nix development environment](../development.md#nix-as-development-environment),
  which contains the necessary dependencies for preparing and comparing the
  kernels.
- The environment contains [`rhel-kernel-get`](https://github.com/viktormalik/rhel-kernel-get)
  script, which can be used to download and prepare the kernels.
- For creating snapshots of the kernels we need to use `build-kernel`
  command.
- We can compare the snapshots with the `compare` command and visualise the
  differences with `view` command.
