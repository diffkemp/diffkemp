from subprocess import Popen, PIPE
from enum import Enum
import sys


class Result(Enum):
    EQUAL = 0
    NOT_EQUAL = 1
    UNKNOWN = 5
    ERROR = -1


def compare_functions(first, second, funFirst, funSecond, coupled,
                      verbose=False):
    if funFirst != funSecond:
        print("Comparing functions %s and %s" % (funFirst, funSecond))
    else:
        print("Comparing function %s" % funFirst)

    stderr = None
    if not verbose:
        stderr = open('/dev/null', 'w')

    command = ["build/diffkemp/llreve/reve/reve/llreve",
               first, second,
               "--fun=" + funFirst + "," + funSecond,
               "-muz", "--ir-input", "--bitvect", "--infer-marks",
               "--disable-auto-coupling"]
    for c in coupled:
        command.append("--couple-functions=%s,%s" % (c[0], c[1]))

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
