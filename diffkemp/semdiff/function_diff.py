"""Semantic difference of two functions using llreve and Z3 SMT solver."""
from __future__ import division
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
from diffkemp.simpll.simpll import simplify_modules_diff, SimpLLException
from diffkemp.semdiff.function_coupling import FunctionCouplings
from diffkemp.syndiff.function_syntax_diff import syntax_diff
from subprocess import Popen, PIPE
from threading import Timer
from enum import Enum
import os
import sys


class Result(Enum):
    """Enumeration type for possible kinds of analysis results."""
    EQUAL = 0
    NOT_EQUAL = 1
    EQUAL_UNDER_ASSUMPTIONS = 2
    EQUAL_SYNTAX = 3
    UNKNOWN = 5
    ERROR = -1
    TIMEOUT = -2

    def __str__(self):
        return self.name.lower().replace("_", " ")


class Statistics():
    """
    Statistics of the analysis.
    Captures numbers of equal and not equal functions, as well as number of
    uncertain or error results.
    """
    def __init__(self):
        self.equal = list()
        self.equal_syntax = list()
        self.not_equal = list()
        self.unknown = list()
        self.errors = list()

    def log_result(self, result, function):
        """Add result of analysis (comparison) of some function."""
        if result == Result.EQUAL or result == Result.EQUAL_UNDER_ASSUMPTIONS:
            self.equal.append(function)
        elif result == Result.EQUAL_SYNTAX:
            self.equal_syntax.append(function)
        elif result == Result.NOT_EQUAL:
            self.not_equal.append(function)
        elif result == Result.UNKNOWN:
            self.unknown.append(function)
        else:
            self.errors.append(function)

    def report(self):
        """Report results."""
        eq_syn = len(self.equal_syntax)
        eq = len(self.equal) + eq_syn
        neq = len(self.not_equal)
        unkwn = len(self.unknown)
        errs = len(self.errors)
        total = eq + neq + unkwn + errs
        print "Total params: {}".format(total)
        print "Equal:        {0} ({1:.0f}%)".format(eq, eq / total * 100)
        print " same syntax: {0}".format(eq_syn)
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
        if len(self.equal_syntax) > 0:
            return Result.EQUAL_SYNTAX
        if len(self.equal) > 0:
            return Result.EQUAL
        return Result.UNKNOWN


def _kill(processes):
    """ Kill each process of the list. """
    for p in processes:
        p.kill()


def _run_llreve_z3(first, second, funFirst, funSecond, coupled, timeout,
                   verbose):
    """
    Run the comparison of semantics of two functions using the llreve tool and
    the Z3 SMT solver. The llreve tool takes compared functions in LLVM IR and
    generates a formula in first-order predicate logic. The formula is then
    solved using the Z3 solver. If it is unsatisfiable, the compared functions
    are semantically the same, otherwise, they are different.

    The generated formula is in the theory of bitvectors.

    :param first: File with the first LLVM module
    :param second: File with the second LLVM module
    :param funFirst: Function from the first module to be compared
    :param funSecond: Function from the second module to be compared
    :param coupled: List of coupled functions (functions that are supposed to
                    correspond to each other in both modules). These are needed
                    for functions not having definintions.
    :param timeout: Timeout for the analysis in seconds
    :param verbose: Verbosity option
    """

    stderr = None
    if not verbose:
        stderr = open('/dev/null', 'w')

    # Commands for running llreve and Z3 (output of llreve is piped into Z3)
    command = ["build/diffkemp/llreve/reve/reve/llreve",
               first, second,
               "--fun=" + funFirst + "," + funSecond,
               "-muz", "--ir-input", "--bitvect", "--infer-marks",
               "--disable-auto-coupling"]
    for c in coupled:
        command.append("--couple-functions={},{}".format(c.first, c.second))

    if verbose:
        sys.stderr.write(" ".join(command) + "\n")

    llreve_process = Popen(command, stdout=PIPE, stderr=stderr)

    z3_process = Popen(["z3", "fixedpoint.engine=duality", "-in"],
                       stdin=llreve_process.stdout,
                       stdout=PIPE, stderr=stderr)

    # Set timeout for both tools
    timer = Timer(timeout, _kill, [[llreve_process, z3_process]])
    try:
        timer.start()

        z3_process.wait()
        result = Result.ERROR
        # Processing the output
        for line in z3_process.stdout:
            line = line.strip()
            if line == b"sat":
                result = Result.NOT_EQUAL
            elif line == b"unsat":
                result = Result.EQUAL
            elif line == b"unknown":
                result = Result.UNKNOWN

        if z3_process.returncode != 0:
            result = Result.ERROR
    finally:
        if not timer.is_alive():
            result = Result.TIMEOUT
        timer.cancel()

    return result


def functions_semdiff(first, second, funFirst, funSecond, coupled, timeout,
                      verbose=False):
    """
    Compare two functions for semantic equality.

    Functions are compared under various assumptions, each having some
    'assumption level'. The higher the level is, the more strong assumption has
    been made. Level 0 indicates no assumption. These levels are determined
    from differences of coupled functions that are set as a parameter of the
    analysis. The analysis tries all assumption levels in increasing manner
    until functions are proven to be equal or no assumptions remain.

    :param first: File with the first LLVM module
    :param second: File with the second LLVM module
    :param funFirst: Function from the first module to be compared
    :param funSecond: Function from the second module to be compared
    :param coupled: List of coupled functions (functions that are supposed to
                    correspond to each other in both modules)
    :param timeout: Timeout for analysis of a function pair (default is 40s)
    :param verbose: Verbosity option
    """
    if funFirst == funSecond:
        fun_str = funFirst
    else:
        fun_str = funSecond
    sys.stdout.write("      Semantic diff of {}".format(fun_str))
    sys.stdout.write("...")
    sys.stdout.flush()

    # Get assumption levels from differences of coupled pairs of functions
    assumption_levels = sorted(set([c.diff for c in coupled]))
    if not assumption_levels:
        assumption_levels = [0]

    for assume_level in assumption_levels:
        # Use all assumptions with the same or lower level
        assumptions = [c for c in coupled if c.diff <= assume_level]
        # Run the actual analysis
        result = _run_llreve_z3(first, second, funFirst, funSecond,
                                assumptions, timeout, verbose)
        # On timeout, stop the analysis because for many assumption levels,
        # this could run for a long time. Moreover, if the analysis times out
        # once, there is a high chance it will time out with more strict
        # assumptions as well.
        if result == Result.TIMEOUT:
            break
        # Stop the analysis when functions are proven to be equal
        if result == Result.EQUAL:
            if assume_level > 0:
                result = Result.EQUAL_UNDER_ASSUMPTIONS
            break

    print result
    if result == Result.EQUAL_UNDER_ASSUMPTIONS:
        print "    Used assumptions:"
        for assume in [a for a in assumptions if a.diff > 0]:
            print "      Functions {} and {} are same".format(assume.first,
                                                              assume.second)
    return result


def functions_diff(first, second,
                   funFirst, funSecond,
                   glob_var,
                   timeout,
                   syntax_only=False, control_flow_only=False, verbose=False):
    """
    Compare two functions for equality.

    First, functions are simplified and compared for syntactic equality using
    the SimpLL tool. If they are not syntactically equal, SimpLL prints a list
    of functions that the syntactic equality depends on. These are then
    compared for semantic equality.
    :param first: File with the first LLVM module
    :param second: File with the second LLVM module
    :param funFirst: Function from the first module to be compared
    :param funSecond: Function from the second module to be compared
    :param timeout: Timeout for analysis of a function pair (default is 40s)
    :param verbose: Verbosity option
    """
    try:
        if not syntax_only:
            if funFirst == funSecond:
                fun_str = funFirst
            else:
                fun_str = "{} and {}".format(funFirst, funSecond)
            print "    Syntactic diff of {} (in {})".format(fun_str, first)

        # Simplify modules
        first_simpl, second_simpl, funs_to_compare = \
            simplify_modules_diff(first, second,
                                  funFirst, funSecond,
                                  glob_var.name if glob_var else None,
                                  glob_var.name if glob_var else "simpl",
                                  control_flow_only,
                                  verbose)
        if not funs_to_compare:
            result = Result.EQUAL_SYNTAX
        elif syntax_only:
            # Only display the syntax diff of the functions that are
            # syntactically different.
            result = Result.NOT_EQUAL
            print "{}:".format(funFirst)
            for fun_pair in funs_to_compare:
                diff = syntax_diff(fun_pair[0]["file"], fun_pair[1]["file"],
                                   fun_pair[0]["fun"])
                print "  {} differs:".format(fun_pair[0]["fun"])
                print "  {{{"
                print diff
                print "  }}}"
        else:
            # If the functions are not syntactically equal, funs_to_compare
            # contains a list of functions that need to be compared
            # semantically. If these are all equal, then the originally
            # compared functions are equal as well.
            stat = Statistics()
            for fun_pair in funs_to_compare:
                # Find couplings of funcions called by the compared
                # functions
                called_couplings = FunctionCouplings(first_simpl,
                                                     second_simpl)
                called_couplings.infer_called_by(fun_pair[0]["fun"],
                                                 fun_pair[1]["fun"])
                called_couplings.clean()
                # Do semantic difference of functions
                result = functions_semdiff(first_simpl, second_simpl,
                                           fun_pair[0]["fun"],
                                           fun_pair[1]["fun"],
                                           called_couplings.called, timeout,
                                           verbose)
                stat.log_result(result, fun_pair[0]["fun"])
            result = stat.overall_result()
        if not syntax_only:
            print "      {}".format(result)
    except SimpLLException:
        print "    Simplifying has failed"
        result = Result.ERROR
    return result
