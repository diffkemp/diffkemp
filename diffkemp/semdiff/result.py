"""Result of difference analysis."""

from enum import IntEnum


class Result:
    """
    Result of a difference analysis.
    Contains the compared entities (functions, params, etc.) and optionally
    contains a list of results of underlying entities (e.g. called functions).
    """
    class Kind(IntEnum):
        """
        Enumeration type for possible kinds of analysis results.
        Sorted by priority for result aggregation.
        """
        NONE = 0
        EQUAL_SYNTAX = 1
        EQUAL = 2
        EQUAL_UNDER_ASSUMPTIONS = 3
        NOT_EQUAL = 4
        UNKNOWN = 5
        TIMEOUT = 6
        ERROR = 7

        def __str__(self):
            return self.name.lower().replace("_", " ")

    class Entity:
        """
        Compared entity information. This can be e.g. a function, a module,
        or a parameter.
        If it is a function, it contains the file of the function.
        """
        def __init__(self, name, filename=None, line=None, callstack=None,
                     is_syn_diff=False, covered_by_syn_diff=False):
            self.name = name
            self.filename = filename
            self.line = line
            self.callstack = callstack
            self.is_syn_diff = is_syn_diff
            self.covered_by_syn_diff = covered_by_syn_diff

    def __init__(self, kind, first_name, second_name):
        self.kind = kind
        self.first = Result.Entity(first_name)
        self.second = Result.Entity(second_name)
        self.diff = None
        self.macro_diff = None
        self.inner = dict()

    def __str__(self):
        return str(self.kind)

    def add_inner(self, result):
        """
        Add result of an inner entity.
        The overall current result is updated based on the entity result.
        """
        self.inner[result.first.name] = result
        # The current result is joined with the inner result (the result with
        # a higher priority is chosen from the two).
        self.kind = Result.Kind(max(int(self.kind), int(result.kind)))

    def report_stat(self):
        """
        Report statistics.
        Print numbers of equal, non-equal, unknown, and error results with
        percentage that each has from the total results.
        """
        total = len(self.inner)
        eq_syn = len([r for r in iter(self.inner.values())
                      if r.kind == Result.Kind.EQUAL_SYNTAX])
        eq = len([r for r in iter(self.inner.values())
                  if r.kind in [Result.Kind.EQUAL,
                                Result.Kind.EQUAL_UNDER_ASSUMPTIONS]]) + eq_syn
        neq = len([r for r in iter(self.inner.values())
                   if r.kind == Result.Kind.NOT_EQUAL])
        unkwn = len([r for r in iter(self.inner.values())
                     if r.kind == Result.Kind.UNKNOWN])
        errs = len([r for r in iter(self.inner.values())
                    if r.kind in [Result.Kind.ERROR, Result.Kind.TIMEOUT]])
        if total > 0:
            print("Total params: {}".format(total))
            print("Equal:        {0} ({1:.0f}%)".format(eq, eq / total * 100))
            print(" same syntax: {0}".format(eq_syn))
            print("Not equal:    {0} ({1:.0f}%)".format(
                  neq, neq / total * 100))
            print("Unknown:      {0} ({1:.0f}%)".format(unkwn,
                                                        unkwn / total * 100))
            print("Errors:       {0} ({1:.0f}%)".format(errs,
                                                        errs / total * 100))

        if unkwn > 0:
            print("\nFunctions that are unknown: ")
            for f, r in iter(self.inner.items()):
                if r.kind == Result.Kind.UNKNOWN:
                    print(f)
            print()
        if errs > 0:
            print("\nFunctions whose comparison ended with an error: ")
            for f, r in iter(self.inner.items()):
                if r.kind == Result.Kind.ERROR:
                    print(f)
            print()
