"""
Computing couplings of functions. A coupled pair are two functions (one
from each module) that are supposed to correspond to each other in both
modules. These are especially useful for functions without definitions, since
for them, we have to assume some equivalence.
Currently, function coupling is computed based on function name equality.
"""
from diffkemp.llvm_ir.function_collector import FunctionCollector


class FunCouple:
    """
    A couple of functions that will be supposed to correspond to each other
    during the analysis.
    """
    def __init__(self, first, second):
        self.first = first
        self.second = second

    def __hash__(self):
        return hash(self.first) ^ hash(self.second)


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

    def clean(self):
        """Free parsed LLVM modules."""
        # Free modules in function collectors
        self._fun_collector_first.clean()
        self._fun_collector_second.clean()

    def _infer_from_sets(self, functions_first, functions_second):
        """
        Find pairs of functions that have the same between the given sets.
        """
        return set([FunCouple(f, f)
                    for f in functions_first & functions_second])

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
        self.uncoupled_first = main_first - set([c.first
                                                 for c in self.main])
        self.uncoupled_second = main_second - set([c.second
                                                   for c in self.main])

    def infer_called_by(self, fun_first, fun_second):
        """
        Find couplings of functions that are called by the given function pair.
        """
        # Compute functions called by main
        called_first = self._fun_collector_first.called_by([fun_first])
        called_second = self._fun_collector_second.called_by([fun_second])
        self.called = self._infer_from_sets(called_first, called_second)

    def set_main(self, main_first, main_second):
        """Manually set main functions."""
        self.main = [FunCouple(main_first, main_second)]
