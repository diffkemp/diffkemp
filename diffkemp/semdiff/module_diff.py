"""
Comparing two kernel modules in LLVM IR for semantic equivalence w.r.t. some
global variable (parameter of modules). Each pair of corresponding functions
using the given parameter is compared individually.
"""
from __future__ import absolute_import

from diffkemp.semdiff.function_diff import functions_diff, Result, Statistics
from diffkemp.semdiff.function_coupling import FunctionCouplings


def modules_diff(first, second, param, timeout, function, syntax_only=False,
                 control_flow_only=False, verbose=False):
    """
    Analyse semantic difference of two LLVM IR modules w.r.t. some parameter
    :param first: File with LLVM IR of the first module
    :param second: File with LLVM IR of the second module
    :param param: Parameter (global variable) to compare (if specified, all
                  functions using this variable are compared).
    :param function: Function to be compared.
    """
    stat = Statistics()
    couplings = FunctionCouplings(first.llvm, second.llvm)
    if param:
        couplings.infer_for_param(param)
    else:
        couplings.set_main(function, function)
    for c in couplings.main:
        if function:
            if not function == c.first and not function == c.second:
                continue
            if (not first.has_function(function) or not
                    second.has_function(function)):
                print "    Given function not found in module"
                stat.log_result(Result.ERROR, "")
                return stat

        result = functions_diff(first.llvm, second.llvm, c.first, c.second,
                                param, timeout, syntax_only, control_flow_only,
                                verbose)
        stat.log_result(result, c.first)

    return stat
