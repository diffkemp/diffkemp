"""Semantic difference of two functions using llreve and Z3 SMT solver."""
from __future__ import division
from diffkemp.simpll.simpll import simplify_modules_diff, SimpLLException
from diffkemp.semdiff.function_coupling import FunctionCouplings
from diffkemp.semdiff.result import Result
from diffkemp.syndiff.function_syntax_diff import syntax_diff
from subprocess import Popen, PIPE
from threading import Timer
import sys


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
        result_kind = Result.Kind.ERROR
        # Processing the output
        for line in z3_process.stdout:
            line = line.strip()
            if line == b"sat":
                result_kind = Result.Kind.NOT_EQUAL
            elif line == b"unsat":
                result_kind = Result.Kind.EQUAL
            elif line == b"unknown":
                result_kind = Result.Kind.UNKNOWN

        if z3_process.returncode != 0:
            result_kind = Result.Kind.ERROR
    finally:
        if not timer.is_alive():
            result_kind = Result.Kind.TIMEOUT
        timer.cancel()

    return Result(result_kind, first, second)


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
        if result.kind == Result.Kind.TIMEOUT:
            break
        # Stop the analysis when functions are proven to be equal
        if result.kind == Result.Kind.EQUAL:
            if assume_level > 0:
                result.kind = Result.Kind.EQUAL_UNDER_ASSUMPTIONS
            break

    print result.kind
    if result == Result.Kind.EQUAL_UNDER_ASSUMPTIONS:
        print "    Used assumptions:"
        for assume in [a for a in assumptions if a.diff > 0]:
            print "      Functions {} and {} are same".format(assume.first,
                                                              assume.second)
    return result


def functions_diff(file_first, file_second,
                   fun_first, fun_second,
                   glob_var, config):
    """
    Compare two functions for equality.

    First, functions are simplified and compared for syntactic equality using
    the SimpLL tool. If they are not syntactically equal, SimpLL prints a list
    of functions that the syntactic equality depends on. These are then
    compared for semantic equality.
    :param file_first: File with the first LLVM module
    :param file_second: File with the second LLVM module
    :param fun_first: Function from the first module to be compared
    :param fun_second: Function from the second module to be compared
    :param glob_var: Global variable whose effect on the functions to compare
    :param config: Configuration
    """
    result = Result(Result.Kind.NONE, file_first, file_second)
    try:
        if not config.syntax_only:
            if fun_first == fun_second:
                fun_str = fun_first
            else:
                fun_str = "{} and {}".format(fun_first, fun_second)
            print "    Syntactic diff of {} (in {})".format(fun_str,
                                                            file_first)

        # Simplify modules
        first_simpl, second_simpl, funs_to_compare = \
            simplify_modules_diff(file_first, file_second,
                                  fun_first, fun_second,
                                  glob_var.name if glob_var else None,
                                  glob_var.name if glob_var else "simpl",
                                  config.control_flow_only,
                                  config.verbosity)
        if not funs_to_compare:
            result.kind = Result.Kind.EQUAL_SYNTAX
        elif config.syntax_only:
            # Only display the syntax diff of the functions that are
            # syntactically different.
            for fun_pair in funs_to_compare:
                fun_result = Result(Result.Kind.NOT_EQUAL, "", "")
                fun_result.first = fun_pair[0]
                fun_result.second = fun_pair[1]
                fun_result.diff = syntax_diff(fun_result.first.filename,
                                              fun_result.second.filename,
                                              fun_result.first.name)
                result.add_inner(fun_result)
        else:
            # If the functions are not syntactically equal, funs_to_compare
            # contains a list of functions that need to be compared
            # semantically. If these are all equal, then the originally
            # compared functions are equal as well.
            for fun_pair in funs_to_compare:
                # Find couplings of funcions called by the compared
                # functions
                called_couplings = FunctionCouplings(first_simpl,
                                                     second_simpl)
                called_couplings.infer_called_by(fun_pair[0].name,
                                                 fun_pair[1].name)
                called_couplings.clean()
                # Do semantic difference of functions
                fun_result = functions_semdiff(first_simpl, second_simpl,
                                               fun_pair[0].name,
                                               fun_pair[1].name,
                                               called_couplings.called,
                                               config.timeout,
                                               config.verbosity)
                result.add_inner(fun_result)
        if not config.syntax_only:
            print "      {}".format(result)
    except SimpLLException:
        print "    Simplifying has failed"
        result.kind = Result.Kind.ERROR
    return result
