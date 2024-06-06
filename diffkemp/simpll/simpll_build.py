#!/usr/bin/env python3
from cffi import FFI
from subprocess import check_output
import os
import sys


def get_c_declarations(header_filename):
    """
    Extracts C declarations important for the cFFI module from the SimpLL FFI
    header file.
    :param filename Name of the SimpLL header file
    """
    cdef_start = "// CFFI_DECLARATIONS_START\n"
    cdef_end = "// CFFI_DECLARATIONS_END\n"

    with open(header_filename, "r") as header_file:
        lines = header_file.readlines()
        start = lines.index(cdef_start) + 1
        end = lines.index(cdef_end)
        return "".join(lines[start:end])


def get_root_dir(path, is_develop_build):
    # The build script is located in CMAKE_BINARY_DIR/diffkemp/simpll, hence
    # we have to move out of it
    if is_develop_build:
        return os.path.abspath(f"{path}/../../..")
    return os.path.abspath(f"{path}/../..")


ffibuilder = FFI()
location = os.path.dirname(os.path.abspath(__file__))
# We can get the project root directory by recognizing whether the script is
# ran manually or by the `setuptools`. The `setuptools` build calls this script
# with additional arguments (_in_process.py bdist_wheel --dist-info-dir ...).
root_dir = get_root_dir(location, is_develop_build=(len(sys.argv) == 1))
path_to_ffi_header = "diffkemp/simpll/library/FFI.h"

ffibuilder.cdef(get_c_declarations(f"{root_dir}/{path_to_ffi_header}"))

llvm_components = ["irreader", "passes", "support"]
llvm_cflags = check_output(["llvm-config", "--cflags"])
llvm_ldflags = check_output(["llvm-config", "--ldflags"])
llvm_libs = check_output(["llvm-config", "--libs"] + llvm_components +
                         ["--system-libs"])

llvm_cflags = list(filter(lambda x: x != "",
                          llvm_cflags.decode("ascii").strip().split()))
llvm_ldflags = list(filter(lambda x: x != "",
                           llvm_ldflags.decode("ascii").strip().split()))
llvm_libs = list(filter(lambda x: x != "",
                        llvm_libs.decode("ascii").strip().split()))

env_var = "SIMPLL_BUILD_DIR"
build_dir = os.environ[env_var] if env_var in os.environ else "build"

simpll_link_arg = f"-L{build_dir}/diffkemp/simpll"

include_path = f"{root_dir}/diffkemp/simpll"

ffibuilder.set_source(
    "_simpll", "#include <library/FFI.h>",
    language="c++",
    libraries=["simpll-lib"],
    extra_compile_args=[f"-I{include_path}"] + llvm_cflags,
    extra_link_args=[simpll_link_arg, "-lstdc++"] + llvm_ldflags + llvm_libs)

if __name__ == "__main__":
    ffibuilder.compile()
