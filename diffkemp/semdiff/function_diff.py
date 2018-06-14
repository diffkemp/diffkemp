"""Semantic difference of two functions using llreve and Z3 SMT solver."""
from subprocess import Popen, PIPE
from threading import Timer
from enum import Enum
import sys


class Result(Enum):
    """Enumeration type for possible kinds of analysis results."""
    EQUAL = 0
    NOT_EQUAL = 1
    EQUAL_UNDER_ASSUMPTIONS = 2
    UNKNOWN = 5
    ERROR = -1
    TIMEOUT = -2

    def __str__(self):
        return self.name.lower().replace("_", " ")


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


def functions_diff(first, second, funFirst, funSecond, coupled, timeout=40,
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
    if funFirst != funSecond:
        sys.stdout.write("    Comparing functions {} and {}".format(funFirst,
                                                                    funSecond))
    else:
        sys.stdout.write("    Comparing function {}".format(funFirst))
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
        # Stop the analysis when functions are proven to be equal
        if result == Result.EQUAL:
            if assume_level > 0:
                result = Result.EQUAL_UNDER_ASSUMPTIONS
            break

    print result
    if result == Result.EQUAL_UNDER_ASSUMPTIONS:
        print "  Used assumptions:"
        for assume in [a for a in assumptions if a.diff > 0]:
            print "    Functions {} and {} are same".format(assume.first,
                                                            assume.second)
    return result
