"""
Simplifying LLVM modules with the SimpLL tool.
"""
import os
from subprocess import check_call, check_output, CalledProcessError


class SimpLLException(Exception):
    pass


def add_suffix(file, suffix):
    """Add suffix to the file name."""
    name, ext = os.path.splitext(file)
    return "{}-{}{}".format(name, suffix, ext)


def simplify_modules_diff(first, second, fun_first, fun_second, var,
                          suffix=None, verbose=False):
    """
    Simplify modules to ease their semantic difference. Uses the SimpLL tool.
    """
    stderr = None
    if not verbose:
        stderr = open(os.devnull, "w")

    first_out = add_suffix(first, suffix) if suffix else first
    second_out = add_suffix(second, suffix) if suffix else second

    try:
        simpll_command = ["build/diffkemp/simpll/simpll", first, second]
        # Main (analysed) functions
        simpll_command.append("--fun")
        if fun_first != fun_second:
            simpll_command.append("{},{}".format(fun_first, fun_second))
        else:
            simpll_command.append(fun_first)
        # Analysed variable
        simpll_command.extend(["--var", var])
        # Suffix for output files
        if suffix:
            simpll_command.extend(["--suffix", suffix])

        if verbose:
            print " ".join(simpll_command)

        simpll_out = check_output(simpll_command, stderr=stderr)
        check_call(["opt", "-S", "-deadargelim", "-o", first_out, first_out],
                   stderr=stderr)
        check_call(["opt", "-S", "-deadargelim", "-o", second_out, second_out],
                   stderr=stderr)

        funs_to_compare = [line.split(",") for line in simpll_out.splitlines()]

        return first_out, second_out, funs_to_compare
    except CalledProcessError:
        raise SimpLLException("Simplifying files failed")
