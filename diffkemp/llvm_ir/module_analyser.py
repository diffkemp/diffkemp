from llvmcpy.llvm import *
from subprocess import Popen, PIPE


class ParamNotFoundException(Exception):
    pass


def _has_param(module_file, param):
    buffer = create_memory_buffer_with_contents_of_file(module_file)
    context = get_global_context()
    module = context.parse_ir(buffer)

    for glob in module.iter_globals():
        if glob.get_name() == param:
            return True
    return False


def check_module(module, param):
    if not _has_param(module, param):
        raise ParamNotFoundException("Parameter not found in module")


def find_definitions_in_object(object_file, functions):
    defs = set()
    nm = Popen(["nm", object_file], stdout=PIPE, stderr=PIPE)
    nm_out, nm_err = nm.communicate()
    symbols = nm_out.splitlines()
    for sym_line in symbols:
        sym = sym_line.split()
        function = sym[-1]
        if (sym[-1] in functions and sym[-2] == "T"):
            defs.add(function)
    return defs

