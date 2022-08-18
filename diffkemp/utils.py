import os
import subprocess


def get_simpll_build_dir():
    """
    Return the current SimpLL build directory as specified
    in the `SIMPLL_BUILD_DIR` environment variable.
    """
    build_dir_var = "SIMPLL_BUILD_DIR"
    if build_dir_var in os.environ:
        return os.environ[build_dir_var]
    return "build"


def get_llvm_version():
    """
    Return the current LLVM major version number.
    """
    return int(subprocess.check_output(
        ["llvm-config", "--version"]).decode().rstrip().split(".")[0])
