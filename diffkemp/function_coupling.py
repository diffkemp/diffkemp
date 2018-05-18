"""
Computing couplings of functions. A coupled pair are two functions (one
from each module) that are supposed to correspond to each other in both
modules. These are especially useful fom functions without definitions, since
for them, we have to assume some equivalence.
Currently, function coupling is computed based on function names.
"""
from __future__ import absolute_import

from diffkemp.llvm_ir.function_collector import FunctionCollector


# Rules defining similarity of function names, used for computing couplings.
# Each rule returns a number that indicates how similar functions are. The
# lower the number is, the higher similarity it represents. 
#   0 denotes identity,
#  -1 denotes no similarity under that rule

def same(name_first, name_second):
    """Compares names for equality."""
    if name_first == name_second:
        return 0
    return -1

def substring(name_first, name_second):
    """
    Checks if one name is substring of another, returns number of characters in
    which names differ.
    """
    if name_first in name_second:
        return len(name_second) - len(name_first)
    if name_second in name_first:
        return len(name_first) - len(name_second)
    return -1

# Rules sorted by importance
rules = [same, substring]


class FunCouple():
    """
    A couple of functions with difference value (similarity). These functions
    will be supposed to correspond to each other during the analysis.
    """
    def __init__(self, first, second, diff):
        self.first = first
        self.second = second
        self.diff = diff

    def __hash__(self):
        return hash(self.first) ^ hash(self.second)


def uncoupled(functions, couplings):
    """
    Determine which functions are not coupled with any other
    :param functions: Set of functions to check
    :param couplings: Existing couplings
    """
    coupled = set()
    for c in couplings:
        coupled.add(c.first)
        coupled.add(c.second)
    return functions - coupled


class FunctionCouplings():
    """Computing function couplings."""
    def __init__(self, first, second):
        self._fun_collector_first = FunctionCollector(first)
        self._fun_collector_second = FunctionCollector(second)
        # Functions whose semantics is to be compared by the analysis
        self.main = set()
        # All other functions that are called by one of the main functions
        self.called = set()
        # Functions that have no corresponding function in the other module
        self.uncoupled_first = set()
        self.uncoupled_second = set()


    def _infer_from_sets(self, functions_first, functions_second):
        """
        Find pairs of functions that correspond to each other between given
        function sets
        """
        couplings = set()

        uncoupled_first = functions_first
        uncoupled_second = functions_second
        # Apply rules by their priority until all functions are coupled or
        # no more rules are left.
        for rule in rules:
            for fun in uncoupled_first:
                # Compute the set of candidates as functions from the second 
                # module (f) that have similarity with fun under the rule
                candidates = [(f, rule(fun, f)) for f in uncoupled_second
                                                if rule(fun, f) >= 0]
                # A coupling is created only if there is a unique candidate
                if len(candidates) == 1:
                    couplings.add(FunCouple(fun, candidates[0][0],
                                            candidates[0][1]))
            uncoupled_first = uncoupled(uncoupled_first, couplings)
            uncoupled_second = uncoupled(uncoupled_second, couplings)
            if (not uncoupled_first or not uncoupled_second):
                break

        return couplings


    def infer_for_param(self, param):
        """
        Find couplings of functions for the given parameter param.
        The parameter determines which functions are treated as main (those
        that use the parameter).
        """
        # Compute main functions
        main_first = self._fun_collector_first.using_param(param)
        main_second = self._fun_collector_second.using_param(param)

        self.main = self._infer_from_sets(main_first, main_second)
        self.uncoupled_first = uncoupled(main_first, self.main)
        self.uncoupled_second = uncoupled(main_second, self.main)

        # Compute functions called by main
        called_first = self._fun_collector_first.called_by(main_first)
        called_second = self._fun_collector_second.called_by(main_second)
        self.called = self._infer_from_sets(called_first, called_second)

