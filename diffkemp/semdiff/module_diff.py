"""
Comparing two kernel modules in LLVM IR for semantic equivalence w.r.t. some
global variable (parameter of modules). Each pair of corresponding functions
using the given parameter is compared individually.
"""
from __future__ import absolute_import

from diffkemp.semdiff.function_diff import functions_diff, Result, Statistics
from diffkemp.semdiff.function_coupling import FunctionCouplings
from llvmcpy.llvm import *


def _indices_correspond(gep, indices):
    indices_correspond = True
    for i in range(1, gep.get_num_operands()):
        if (i - 1) >= len(indices):
            break
        if (gep.get_operand(i).const_int_get_z_ext() !=
                indices[i - 1]):
            indices_correspond = False
    return indices_correspond


def _corresponding_index_unused(module, function, param):
    can_skip = True
    value = module.get_named_global(param.name)

    for use in value.iter_uses():
        user = use.get_user()
        if user.is_a_instruction():
            if (user.get_instruction_parent().get_parent().in_ptr() !=
                    function.in_ptr()):
                # The user is in a different function
                continue
            if user.is_a_get_element_ptr_inst():
                # Look whether the GEP references the desired index or not
                if _indices_correspond(user, param.indices):
                    can_skip = False
                    break
            else:
                # One of the users is not a GEP, do not skip the function
                can_skip = False
                break
        elif user.is_a_constant_expr():
            # Look whether the constant expression is used in the function at
            # least once.
            used_in_function = False
            for cexpr_use in user.iter_uses():
                cexpr_user = cexpr_use.get_user()
                if (cexpr_user.get_instruction_parent().get_parent().
                        in_ptr() == function.in_ptr()):
                    used_in_function = True
            if not used_in_function:
                # The user is in a different function
                continue
            if user.get_const_opcode() == Opcode['GetElementPtr']:
                # Look whether the GEP references the desired index or not
                if _indices_correspond(user, param.indices):
                    can_skip = False
                    break
            else:
                # One of the users is not a GEP, do not skip the function
                can_skip = False
                break
        else:
            # One of the users is not a GEP, do not skip the function
            can_skip = False
            break

    return can_skip


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
    print(param)
    stat = Statistics()
    couplings = FunctionCouplings(first.llvm, second.llvm)
    if param:
        couplings.infer_for_param(param.name)
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

        if param.indices is not None:
            # Do not compare function when all uses are of a part of the
            # variable with a different index than specified in the indices
            # parameter.
            llvm_context = get_global_context()
            module_left = llvm_context.parse_ir(
                create_memory_buffer_with_contents_of_file(first.llvm))
            module_right = llvm_context.parse_ir(
                create_memory_buffer_with_contents_of_file(second.llvm))
            function_left = module_left.get_named_function(c.first)
            function_right = module_right.get_named_function(c.second)

            if (_corresponding_index_unused(module_left, function_left,
                                            param) and
                _corresponding_index_unused(module_right, function_right,
                                            param)):
                continue

        result = functions_diff(first.llvm, second.llvm, c.first, c.second,
                                param.name, timeout, syntax_only,
                                control_flow_only, verbose)
        stat.log_result(result, c.first)

    return stat
