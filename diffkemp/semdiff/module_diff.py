"""
Comparing two kernel modules in LLVM IR for semantic equivalence w.r.t. some
global variable (parameter of modules). Each pair of corresponding functions
using the given parameter is compared individually.
"""
from __future__ import absolute_import

from __future__ import division
from diffkemp.semdiff.function_diff import functions_diff, Result
from diffkemp.semdiff.function_coupling import FunctionCouplings
from diffkemp.simpll.simpll import simplify_modules_diff, SimpLLException


class Statistics():
    """
    Statistics of the analysis.
    Captures numbers of equal and not equal functions, as well as number of
    uncertain or error results.
    """
    def __init__(self):
        self.equal = list()
        self.not_equal = list()
        self.unknown = list()
        self.errors = list()

    def log_result(self, result, function):
        """Add result of analysis (comparison) of some function."""
        if result == Result.EQUAL or result == Result.EQUAL_UNDER_ASSUMPTIONS:
            self.equal.append(function)
        elif result == Result.NOT_EQUAL:
            self.not_equal.append(function)
        elif result == Result.UNKNOWN:
            self.unknown.append(function)
        else:
            self.errors.append(function)

    def report(self):
        """Report results."""
        eq = len(self.equal)
        neq = len(self.not_equal)
        unkwn = len(self.unknown)
        errs = len(self.errors)
        total = eq + neq + unkwn + errs
        print "Total params: {}".format(total)
        print "Equal:        {0} ({1:.0f}%)".format(eq, eq / total * 100)
        print "Not equal:    {0} ({1:.0f}%)".format(neq, neq / total * 100)
        print "Unknown:      {0} ({1:.0f}%)".format(unkwn, unkwn / total * 100)
        print "Errors:       {0} ({1:.0f}%)".format(errs, errs / total * 100)

    def overall_result(self):
        """Aggregate results for individual functions into one result."""
        if len(self.errors) > 0:
            return Result.ERROR
        if len(self.not_equal) > 0:
            return Result.NOT_EQUAL
        if len(self.unknown) > 0:
            return Result.UNKNOWN
        if len(self.equal) > 0:
            return Result.EQUAL
        return Result.UNKNOWN


def modules_diff(first, second, param, timeout, function, verbose=False):
    """
    Analyse semantic difference of two LLVM IR modules w.r.t. some parameter
    :param first: File with LLVM IR of the first module
    :param second: File with LLVM IR of the second module
    :param param: Parameter (global variable) to compare (only functions
                  using this variable are compared)
    """
    stat = Statistics()
    couplings = FunctionCouplings(first.llvm, second.llvm)
    couplings.infer_for_param(param)
    for c in couplings.main:
        if function and not function == c.first and not function == c.second:
            continue
        try:
            # Simplify modules
            first_simpl, second_simpl = simplify_modules_diff(first.llvm,
                                                              second.llvm,
                                                              c.first,
                                                              c.second,
                                                              param,
                                                              param,
                                                              verbose)
            # Find couplings of funcions called by the compared functions
            called_couplings = FunctionCouplings(first_simpl, second_simpl)
            called_couplings.infer_called_by(c.first, c.second)
            # Do semantic difference of modules
            result = functions_diff(first_simpl, second_simpl, c.first,
                                    c.second, called_couplings.called, timeout,
                                    verbose)
            stat.log_result(result, c.first)
        except SimpLLException:
            print "    Simplifying has failed"
            stat.log_result(Result.ERROR, "")
    return stat
