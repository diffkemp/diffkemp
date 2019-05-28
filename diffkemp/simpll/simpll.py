"""
Simplifying LLVM modules with the SimpLL tool.
"""
from diffkemp.semdiff.result import Result
import os
from subprocess import check_call, check_output, CalledProcessError
import yaml


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
            simpll_command.append("--verbose")
            print(" ".join(simpll_command))

        simpll_out = check_output(simpll_command)
        check_call(["opt", "-S", "-deadargelim", "-o", first_out, first_out],
                   stderr=stderr)
        check_call(["opt", "-S", "-deadargelim", "-o", second_out, second_out],
                   stderr=stderr)

        objects_to_compare = []
        missing_defs = None
        macro_defs = None
        try:
            simpll_result = yaml.safe_load(simpll_out)
            if simpll_result is not None:
                if "diff-functions" in simpll_result:
                    for fun_pair_yaml in simpll_result["diff-functions"]:
                        fun_pair = [
                            Result.Entity(
                                fun["function"],
                                fun["file"] if "file" in fun else "",
                                fun["line"] if "line" in fun else None,
                                "\n".join(
                                    ["{} at {}:{}".format(call["function"],
                                                          call["file"],
                                                          call["line"])
                                     for call in fun["callstack"]])
                                if "callstack" in fun else "",
                                fun["is-macro"]
                            )
                            for fun in [fun_pair_yaml["first"],
                                        fun_pair_yaml["second"]]
                        ]

                        objects_to_compare.append(tuple(fun_pair))
                missing_defs = simpll_result["missing-defs"] \
                    if "missing-defs" in simpll_result else None
                macro_defs = simpll_result["macro-defs"] \
                    if "macro-defs" in simpll_result else None
        except yaml.YAMLError:
            pass

        return first_out, second_out, objects_to_compare, missing_defs, \
            macro_defs
    except CalledProcessError:
        raise SimpLLException("Simplifying files failed")
