from subprocess import Popen, PIPE
from enum import Enum
import sys


class Result(Enum):
    EQUAL = 0
    NOT_EQUAL = 1
    EQUAL_UNDER_ASSUMPTIONS = 2
    UNKNOWN = 5
    ERROR = -1

    def __str__(self):
        return self.name.lower().replace("_", " ")


def _run_llreve_z3(first, second, funFirst, funSecond, coupled, verbose):
    stderr = None
    if not verbose:
        stderr = open('/dev/null', 'w')

    command = ["build/diffkemp/llreve/reve/reve/llreve",
               first, second,
               "--fun=" + funFirst + "," + funSecond,
               "-muz", "--ir-input", "--bitvect", "--infer-marks",
               "--disable-auto-coupling"]
    for c in coupled:
        command.append("--couple-functions=%s,%s" % (c.first, c.second))

    if verbose:
        sys.stderr.write(" ".join(command) + "\n")

    llreve_process = Popen(command, stdout=PIPE, stderr=stderr)

    z3_process = Popen(["z3", "fixedpoint.engine=duality", "-in"],
                       stdin=llreve_process.stdout,
                       stdout=PIPE, stderr=stderr)

    result = Result.ERROR
    for line in z3_process.stdout:
        line = line.strip()
        if line == b"sat":
            result = Result.NOT_EQUAL
        elif line == b"unsat":
            result = Result.EQUAL
        elif line == b"unknown":
            result = Result.UNKNOWN
    z3_process.wait()
    if z3_process.returncode != 0:
        result = Result.ERROR

    return result


def compare_functions(first, second, funFirst, funSecond, coupled,
                      verbose=False):
    print ""
    if funFirst != funSecond:
        sys.stdout.write("Comparing functions %s and %s" % (funFirst,
                                                            funSecond))
    else:
        sys.stdout.write("Comparing function %s" % funFirst)
    sys.stdout.write("...")
    sys.stdout.flush()

    assumption_levels = sorted(set([c.diff for c in coupled]))
    if not assumption_levels:
        assumption_levels = [0]

    for assume_level in assumption_levels:
        assumptions = [c for c in coupled if c.diff <= assume_level]
        result = _run_llreve_z3(first, second, funFirst, funSecond,
                                assumptions, verbose)
        if result == Result.EQUAL:
            if assume_level > 0:
                result = Result.EQUAL_UNDER_ASSUMPTIONS
            break

    print result
    if result == Result.EQUAL_UNDER_ASSUMPTIONS:
        print "  Used assumptions:"
        for assume in [a for a in assumptions if a.diff > 0]:
            print "    Functions %s and %s are same" % (assume.first,
                                                        assume.second)
    return result

