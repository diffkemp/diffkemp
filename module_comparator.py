from llvmcpy.llvm import *
from function_comparator import compare_function, Result


class Statistics():
    def __init__(self):
        self.equal = list()
        self.not_equal = list()
        self.unknown = list()
        self.errors = list()

    def log_result(self, result, function):
        if result == Result.EQUAL:
            self.equal.append(function)
        elif result == Result.NOT_EQUAL:
            self.not_equal.append(function)
        elif result == Result.UNKNOWN:
            self.unknown.append(function)
        else:
            self.errors.append(function)

    def report(self):
        print "Equal:     ", str(len(self.equal))
        print "Not equal: ", str(len(self.not_equal))
        print "Unknown:   ", str(len(self.unknown))
        print "Errors:    ", str(len(self.errors))

    def overall_result(self):
        if len(self.errors) > 0:
            return Result.ERROR
        if len(self.not_equal) > 0:
            return Result.NOT_EQUAL
        if len(self.unknown) > 0:
            return Result.UNKNOWN
        return Result.ERROR


def _dependent_functions(module_file, param):
    buffer = create_memory_buffer_with_contents_of_file(module_file)
    context = get_global_context()
    module = context.parse_ir(buffer)
    glob = module.get_named_global(param)
    result = set()
    for use in glob.iter_uses():
        if use.user.get_kind() != InstructionValueKind:
            continue
        bb = use.user.instruction_parent
        func = bb.parent
        result.add(func.name)
    return result


def compare_modules(first, second, parameter, verbose=False):
    first_functions = _dependent_functions(first, parameter)
    second_functions = _dependent_functions(second, parameter)
    stat = Statistics()
    for fun in first_functions & second_functions:
        result = compare_function(first, second, fun)
        stat.log_result(result, fun)
    return stat

