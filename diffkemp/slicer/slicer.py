from os import path
from subprocess import Popen, PIPE


class SlicerException(Exception):
    def __init__(self):
        pass

    def __str__(self):
        return "Slicing has failed"


def sliced_name(file):
    name, ext = path.splitext(file)
    return name + "-sliced" + ext


def slice_module(file, parameter, out_file=None, verbose=False):
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
