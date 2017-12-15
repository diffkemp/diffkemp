from os import path
from subprocess import Popen, PIPE


def slice_module(file, parameter, verbose):
    print("Slicing %s" % file)

    name, ext = path.splitext(file)
    outFile = name + "-sliced" + ext

    stderr=None
    if (not verbose):
        stderr = open('/dev/null', 'w')

    opt = Popen(["opt", "-S",
                 "-load", "build/slicer/libParDepSlicer.so",
                 "-paramdep-slicer", "-param-name=" + parameter,
                 "-o", "".join(outFile),
                 file],
                stdout=PIPE, stderr=stderr)
    opt.wait()
    return outFile

