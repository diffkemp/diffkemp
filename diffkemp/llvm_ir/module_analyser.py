from llvmcpy.llvm import *


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

