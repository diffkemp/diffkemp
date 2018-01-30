from os import path
from subprocess import Popen, PIPE


class SlicerException(Exception):
    def __init__(self):
        pass

    def __str__(self):
        return "Slicing has failed"


def slice_module(file, parameter, verbose):
    print("Slicing %s" % file)

    name, ext = path.splitext(file)
    out_file = name + "-sliced" + ext

    stderr = None
    if not verbose:
        stderr = open('/dev/null', 'w')

    opt = Popen(["opt", "-S",
                 "-load", "build/slicer/libParDepSlicer.so",
                 "-paramdep-slicer", "-param-name=" + parameter,
                 "-o", "".join(out_file),
                 file],
                stdout=PIPE, stderr=stderr)
    opt.wait()
    if opt.returncode != 0:
        raise SlicerException()

    return out_file
