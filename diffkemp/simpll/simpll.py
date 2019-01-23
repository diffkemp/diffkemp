"""
Simplifying LLVM modules with the SimpLL tool.
"""
import os
from diffkemp.semdiff.result import Result
from subprocess import check_call, check_output, CalledProcessError


class SimpLLException(Exception):
    pass


def add_suffix(file, suffix):
    """Add suffix to the file name."""
    name, ext = os.path.splitext(file)
    return "{}-{}{}".format(name, suffix, ext)


def simplify_modules_diff(first, second, fun_first, fun_second, var,
                          suffix=None, control_flow_only=False, verbose=False):
    """
    Simplify modules to ease their semantic difference. Uses the SimpLL tool.
    """
    stderr = None
    if not verbose:
        stderr = open(os.devnull, "w")

    first_out = add_suffix(first, suffix) if suffix else first
    second_out = add_suffix(second, suffix) if suffix else second

    try:
        simpll_command = ["build/diffkemp/simpll/simpll", first, second,
                          "--print-callstacks"]
        # Main (analysed) functions
        simpll_command.append("--fun")
        if fun_first != fun_second:
            simpll_command.append("{},{}".format(fun_first, fun_second))
        else:
            simpll_command.append(fun_first)
        # Analysed variable
        if var:
            simpll_command.extend(["--var", var])
        # Suffix for output files
        if suffix:
            simpll_command.extend(["--suffix", suffix])

        if control_flow_only:
            simpll_command.append("--control-flow")

        if verbose:
            print " ".join(simpll_command)

        simpll_out = check_output(simpll_command, stderr=stderr)
        check_call(["opt", "-S", "-deadargelim", "-o", first_out, first_out],
                   stderr=stderr)
        check_call(["opt", "-S", "-deadargelim", "-o", second_out, second_out],
                   stderr=stderr)

        funs_to_compare = []
        # Nonequal function pairs are separated by "----------"
        for fun_pair_out in simpll_out.split("----------"):
            if not fun_pair_out or fun_pair_out.isspace():
                continue
            # Entries for functions in a pair are separated by an empty line.
            # Each function is represented by a Result.Entity object.
            # Each entity contains:
            #  - the function name (first line of the function entry)
            #  - the file (second line of the function entry, if present)
            #  - the callstack (rest of lines of the function entry)
            fun_pair_split = [s.strip() for s in fun_pair_out.split("\n\n")]
            fun_pair = tuple([
                Result.Entity(
                    lines[0],
                    lines[1] if len(lines) > 1 else "",
                    "\n".join(lines[2:]) if len(lines) > 2 else ""
                )
                for lines in
                (fun_pair_split[i].splitlines() for i in range(0, 2))])
            funs_to_compare.append(fun_pair)

        return first_out, second_out, funs_to_compare
    except CalledProcessError:
        raise SimpLLException("Simplifying files failed")
