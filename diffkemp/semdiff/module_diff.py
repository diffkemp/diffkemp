"""
Comparing two kernel modules in LLVM IR for semantic equivalence w.r.t. some
global variable (parameter of modules). Each pair of corresponding functions
using the given parameter is compared individually.
"""
from __future__ import absolute_import

from diffkemp.semdiff.function_diff import functions_diff, Result, Statistics
from diffkemp.semdiff.function_coupling import FunctionCouplings
from llvmcpy.llvm import *


def modules_diff(first, second, param, timeout, function, syntax_only=False,
                 control_flow_only=False, verbose=False, indices = None):
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

        if indices:
            # Do not compare function when all uses are of a part of the
            # variable with a different index than specified in the indices
            # parameter.
            can_skip = True
            llvm_context = get_global_context()
            module_left = llvm_context.parse_ir(
                create_memory_buffer_with_contents_of_file(first.llvm))
            module_right = llvm_context.parse_ir(
                create_memory_buffer_with_contents_of_file(second.llvm))
            function_left = module_left.get_named_function(c.first)
            function_right = module_right.get_named_function(c.second)
            value_left = module_left.get_named_global(param)
            value_right = module_right.get_named_global(param)

            for use in value_left.iter_uses():
                user = use.get_user()
                if not user.is_a_get_element_ptr_inst():
                    # One of the users is not a GEP, do not skip the function
                    can_skip = False
                    break
                indices_correspond = True
                for i in range(1, user.get_num_operands()):
                    if data.get_operand(i) != indices[i]:
                        indices_correspond = False
                if not indices_correspond:
                    can_skip = False
                    break

            if can_skip:
                continue


        result = functions_diff(first.llvm, second.llvm, c.first, c.second,
                                param, timeout, syntax_only, control_flow_only,
                                verbose)
        stat.log_result(result, c.first)

    return stat
