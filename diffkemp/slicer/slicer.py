"""
Slicing LLVM of kernel modules with respect to usage of some kernel module
parameter (global variable).
"""

from os import path
from subprocess import Popen, PIPE


class SlicerException(Exception):
    def __init__(self):
        pass

    def __str__(self):
        return "Slicing has failed"


def sliced_name(file):
    """Name of the sliced file"""
    name, ext = path.splitext(file)
    return name + "-sliced" + ext


def slice_module(file, parameter, out_file=None, verbose=False):
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
    print("Slicing %s" % file)

    if not out_file:
        out_file = sliced_name(file)

    stderr = None
    if not verbose:
        stderr = open('/dev/null', 'w')

    opt = Popen(["opt", "-S",
                 "-load", "build/diffkemp/slicer/libParDepSlicer.so",
                 "-paramdep-slicer", "-param-name=" + parameter,
                 "-deadargelim",
                 "-o", "".join(out_file),
                 file],
                stdout=PIPE, stderr=stderr)
    opt.wait()
    if opt.returncode != 0:
        raise SlicerException()

    return out_file
