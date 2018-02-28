from llvmcpy.llvm import *
from function_comparator import compare_functions, Result
from function_coupling import FunctionCouplings


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
        return Result.EQUAL


def compare_modules(first, second, parameter, verbose=False):
    stat = Statistics()
    couplings = FunctionCouplings(first, second)
    couplings.infer_for_param(parameter)
    for (fun_first, fun_second) in couplings.main:
        result = compare_functions(first, second, fun_first, fun_second,
                                   verbose)
        stat.log_result(result, fun_first)
    return stat

