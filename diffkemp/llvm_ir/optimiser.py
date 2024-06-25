"""Functions for optimizations of LLVM IR."""
import os
from subprocess import CalledProcessError, check_call

from diffkemp.utils import get_opt_command


class BuildException(Exception):
    pass


def opt_llvm(llvm_file):
    """
    Optimize LLVM IR using 'opt' tool.
    Run basic simplification passes and -constmerge to remove
    duplicate constants that might have come from linked files.
    """
    passes = [("lowerswitch", "function"),
              ("mem2reg", "function"),
              ("loop-simplify", "function"),
              ("simplifycfg", "function"),
              ("gvn", "function"),
              ("dce", "function"),
              ("constmerge", "module"),
              ("mergereturn", "function"),
              ("simplifycfg", "function")]
    opt_command = get_opt_command(passes, llvm_file)
    try:
        with open(os.devnull, "w") as devnull:
            check_call(opt_command, stderr=devnull)
    except CalledProcessError:
        raise BuildException("Running opt failed")
