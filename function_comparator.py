from subprocess import Popen, PIPE
from enum import Enum


class Result(Enum):
    EQUAL = 0
    NOT_EQUAL = 1
    UNKNOWN = 5
    ERROR = -1


def compare(first, second, function, verbose=False):
    print("Comparing function %s" % function)

    stderr = None
    if (not verbose):
        stderr = open('/dev/null', 'w')

    llreveProcess = Popen(["build/llreve/reve/reve/llreve", first, second,
                           "--fun=" + function,
                           "-muz", "--ir-input"],
                          stdout=PIPE, stderr=stderr)

    z3Process = Popen(["z3", "fixedpoint.engine=duality","-in"],
                      stdin=llreveProcess.stdout,
                      stdout=PIPE, stderr=stderr)

    result = Result.ERROR
    for line in z3Process.stdout:
        line = line.strip()
        if line == b"sat":
            result = Result.NOT_EQUAL
        elif line == b"unsat":
            result = Result.EQUAL
        elif line == b"unknown":
            result = Result.UNKNOWN
    z3Process.wait()
    if z3Process.returncode != 0 :
        result = Result.ERROR

    return result

