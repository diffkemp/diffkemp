"""
Comparing two kernel modules in LLVM IR for semantic equivalence w.r.t. some
global variable (parameter of modules). Each pair of corresponding functions
using the given parameter is compared individually.
"""
from __future__ import absolute_import

from diffkemp.llvm_ir.build_llvm import BuildException
from diffkemp.semdiff.function_diff import functions_diff, Result, Statistics
from diffkemp.semdiff.function_coupling import FunctionCouplings


def diff_all_modules_using_global(first_builder, second_builder,
                                  glob_first, glob_second,
                                  timeout,
                                  syntax_only=False, control_flow_only=False,
                                  verbose=False):
    """
    Compare semantics of all modules using given global variable.
    Finds all source files that use the given globals and compare all of them.
    :param first_builder: Builder for the first kernel version
    :param second_builder: Builder for the second kernel version
    :param glob_first: First global to compare
    :param glob_second: Second global to compare
    """
    result = Statistics()
    if glob_first != glob_second:
        # Variables with different names are treated as unequal
        result.log_result(Result.NOT_EQUAL, glob_first)
    else:
        srcs_first = first_builder.source.find_srcs_using_symbol(glob_first)
        srcs_second = second_builder.source.find_srcs_using_symbol(glob_second)
        # Compare all sources containing functions using the variable
        for src in srcs_first:
            if src not in srcs_second:
                result.log_result(Result.NOT_EQUAL, src)
            else:
                try:
                    mod_first = first_builder.build_file(src)
                    mod_second = second_builder.build_file(src)
                    mod_first.parse_module()
                    mod_second.parse_module()
                    if (mod_first.has_global(glob_first) and
                            mod_second.has_global(glob_second)):
                        stat = modules_diff(first=mod_first, second=mod_second,
                                            glob_var=glob_first, function=None,
                                            timeout=timeout,
                                            syntax_only=syntax_only,
                                            control_flow_only=control_flow_only,
                                            verbose=verbose)
                        result.log_result(stat.overall_result(), src)
                    mod_first.clean_module()
                    mod_second.clean_module()
                except BuildException:
                    result.log_result(Result.ERROR, src)
        return result


def modules_diff(first, second,
                 glob_var, function,
                 timeout,
                 syntax_only=False, control_flow_only=False, verbose=False):
    """
    Analyse semantic difference of two LLVM IR modules w.r.t. some parameter
    :param first: File with LLVM IR of the first module
    :param second: File with LLVM IR of the second module
    :param glob_var: Parameter (global variable) to compare (if specified, all
                  functions using this variable are compared).
    :param function: Function to be compared.
    """
    stat = Statistics()
    couplings = FunctionCouplings(first.llvm, second.llvm)
    if glob_var:
        couplings.infer_for_param(glob_var)
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

        result = functions_diff(first=first.llvm, second=second.llvm,
                                funFirst=c.first, funSecond=c.second,
                                glob_var=glob_var,
                                timeout=timeout,
                                syntax_only=syntax_only,
                                control_flow_only=control_flow_only,
                                verbose=verbose)
        stat.log_result(result, c.first)

    return stat
