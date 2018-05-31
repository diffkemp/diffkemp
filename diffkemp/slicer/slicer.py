"""
Slicing LLVM of kernel modules with respect to usage of some kernel module
parameter (global variable).
"""

import os
from subprocess import check_call


class SlicerException(Exception):
    def __init__(self):
        pass

    def __str__(self):
        return "Slicing has failed"


def sliced_name(file, param):
    """Name of the sliced file"""
    name, ext = os.path.splitext(file)
    return "{}-{}{}".format(name, param, ext)


def slice_module(file, parameter, verbose=False):
    """
    Slice the given module w.r.t. to the parameter.
    The actual slicer is implemented as an LLVM pass in C++ that gets the
    parameter name as argument.

    :param file: File with LLVM module to be sliced
    :param parameter: Kernel module parameter (i.e. global variable) that
                      guides the slicing.
    :param out_file: Name of the output file. If not provided, it is determined
                     from the name of the input file.
    :param verbose: Verbosity option.
    """
    print("    [slice] {}".format(file))
    out_file = sliced_name(file, parameter)

    stderr = None
    if not verbose:
        stderr = open(os.devnull, "w")

    check_call(["opt", "-S",
                "-load", "build/diffkemp/slicer/libParDepSlicer.so",
                "-paramdep-slicer", "-param-name=" + parameter,
                "-deadargelim",
                "-o", "".join(out_file), file], stderr=stderr)

    return out_file
