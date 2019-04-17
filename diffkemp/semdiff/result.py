"""Result of difference analysis."""
from __future__ import division
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
        def __init__(self, name, filename=None, callstack=None):
            self.name = name
            self.filename = filename
            self.callstack = callstack

    class MacroElement:
        """
        Represents a macro element on the macro stack.
        """
        def __init__(self, yaml):
            """Transforms YAML to MacroElement object."""
            self.name = yaml["name"]
            self.body = yaml["body"]
            self.file = yaml["file"]
            self.line = yaml["line"]

    class MacroDifference:
        """
        Represents a macro difference as found by SimpLL.
        """
        def __init__(self, yaml):
            """Transforms YAML to MacroDifference object."""
            self.name = yaml["name"]
            self.first_stack = map(Result.MacroElement, yaml["left-stack"])
            self.second_stack = map(Result.MacroElement, yaml["right-stack"])
            self.first_line = yaml["left-line"]
            self.second_line = yaml["right-line"]

        def __str__(self):
            res = "Macro {} is different:\n".format(self.first_stack[0].name)
            for stack in [self.first_stack, self.second_stack]:
                for macro in stack:
                    res += "  from macro {} in file {} on line {}\n".format(
                        macro.name, macro.file, str(macro.line))
                res += "  used in the function on line {}\n".format(
                    (str(self.first_line) if stack is self.first_stack
                     else str(self.second_line)))
                res += "    {}\n".format(stack[0].body)
                if stack is self.first_stack:
                    res += "\n"
            return res

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
        eq_syn = len(filter(lambda r: r.kind == Result.Kind.EQUAL_SYNTAX,
                            self.inner.itervalues()))
        eq = len(filter(
            lambda r: r.kind in [Result.Kind.EQUAL,
                                 Result.Kind.EQUAL_UNDER_ASSUMPTIONS],
            self.inner.itervalues())) + eq_syn
        neq = len(filter(lambda r: r.kind == Result.Kind.NOT_EQUAL,
                         self.inner.itervalues()))
        unkwn = len(filter(lambda r: r.kind == Result.Kind.UNKNOWN,
                           self.inner.itervalues()))
        errs = len(filter(
            lambda r: r.kind in [Result.Kind.ERROR, Result.Kind.TIMEOUT],
            self.inner.itervalues()))
        if total > 0:
            print "Total params: {}".format(total)
            print "Equal:        {0} ({1:.0f}%)".format(eq, eq / total * 100)
            print " same syntax: {0}".format(eq_syn)
            print "Not equal:    {0} ({1:.0f}%)".format(neq, neq / total * 100)
            print "Unknown:      {0} ({1:.0f}%)".format(unkwn,
                                                        unkwn / total * 100)
            print "Errors:       {0} ({1:.0f}%)".format(errs,
                                                        errs / total * 100)
