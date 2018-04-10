from llvmcpy.llvm import *
import itertools


class FunctionCollector():
    def __init__(self, module_file):
        # Parse LLVM module
        buffer = create_memory_buffer_with_contents_of_file(module_file)
        context = get_global_context()
        self._module = context.parse_ir(buffer)


    # List of standard functions that are supported, so they should not be
    # included in couplings
    supported_names = ["malloc", "calloc", "kmalloc", "kzalloc", "__kmalloc",
                       "llvm.dbg.value", "llvm.dbg.declare"]
    @staticmethod
    def supported_fun(llvm_fun):
        name = llvm_fun.get_name()
        if name:
            return name in FunctionCollector.supported_names
        return False


    # Find names of all functions that are (recursively) called by given 
    # function
    @staticmethod
    def _called_by_one(llvm_fun, result):
        for bb in llvm_fun.iter_basic_blocks():
            for instr in bb.iter_instructions():
                if instr.get_instruction_opcode() != Call:
                    continue
                called = instr.get_called()
                if (called.get_name() and
                    not FunctionCollector.supported_fun(called) and
                    not called.get_name() in result):
                    result.add(called.get_name())
                    FunctionCollector._called_by_one(called, result)

                for opIndex in range(0, instr.get_num_operands()):
                    op = instr.get_operand(opIndex)
                    if op.get_kind() != FunctionValueKind:
                        continue

                    if (op.get_name() and
                        not FunctionCollector.supported_fun(op) and
                        not op.get_name() in result):
                        result.add(op.get_name())
                        FunctionCollector._called_by_one(op, result)


    # Find names of all functions using given parameter (global variable)
    def using_param(self, param):
        glob = self._module.get_named_global(param)
        result = set()
        for use in glob.iter_uses():
            if use.user.get_kind() != InstructionValueKind:
                continue
            bb = use.user.instruction_parent
            func = bb.parent
            result.add(func.name)
        return result


    # Find names of all functions (recursively) called by one of functions
    # in the given set
    def called_by(self, function_names):
        result = set()
        for fun_name in function_names:
            llvm_fun = self._module.get_named_function(fun_name)
            self._called_by_one(llvm_fun, result)
        return result


# Rules defining similarity of function names, used when coupling functions.
# Each rule returns a number that indicates how similar functions are. The
# lower the number is, the higher similarity it represents. 
#   0 denotes identity,
#  -1 denotes no similarity under that rule

# Compares names for equality
def same(name_first, name_second):
    if name_first == name_second:
        return 0
    return -1

# Checks if one name is substring of another, returns number of characters in
# which names differ.
def substring(name_first, name_second):
    if name_first in name_second:
        return len(name_second) - len(name_first)
    if name_second in name_first:
        return len(name_first) - len(name_second)
    return -1

# Rules sorted by importance
rules = [same, substring]


# A couple of functions with difference value (similarity)
class FunCouple():
    def __init__(self, first, second, diff):
        self.first = first
        self.second = second
        self.diff = diff

    def __hash__(self):
        return hash(self.first) ^ hash(self.second)


# Determine which functions from the set are not coupled with anything in 
# couplings
def uncoupled(functions, couplings):
    coupled = set()
    for c in couplings:
        coupled.add(c.first)
        coupled.add(c.second)
    return functions - coupled


class FunctionCouplings():
    def __init__(self, first, second):
        self._fun_collector_first = FunctionCollector(first)
        self._fun_collector_second = FunctionCollector(second)
        # Functions to be compared
        self.main = set()
        # All other functions that are called by one of main functions
        self.called = set()
        # Functions that have no corresponding function in the other module
        self.uncoupled_first = set()
        self.uncoupled_second = set()


    # Find pairs of functions that correspond to each other between given
    # function sets
    def _infer_from_sets(self, functions_first, functions_second):
        couplings = set()

        uncoupled_first = functions_first
        uncoupled_second = functions_second
        # Apply rules by their priority until all functions are coupled or
        # no more rules are left.
        for rule in rules:
            for fun in uncoupled_first:
                candidates = [(f, rule(fun, f)) for f in uncoupled_second
                                                if rule(fun, f) >= 0]
                if len(candidates) == 1:
                    couplings.add(FunCouple(fun, candidates[0][0],
                                            candidates[0][1]))
            uncoupled_first = uncoupled(uncoupled_first, couplings)
            uncoupled_second = uncoupled(uncoupled_second, couplings)
            if (not uncoupled_first or not uncoupled_second):
                break

        return couplings


    # Find couplings of functions for the given parameter
    # The parameter determines which functions are treated as main
    def infer_for_param(self, param):
        main_first = self._fun_collector_first.using_param(param)
        main_second = self._fun_collector_second.using_param(param)

        self.main = self._infer_from_sets(main_first, main_second)
        self.uncoupled_first = uncoupled(main_first, self.main)
        self.uncoupled_second = uncoupled(main_second, self.main)

        called_first = self._fun_collector_first.called_by(main_first)
        called_second = self._fun_collector_second.called_by(main_second)
        self.called = self._infer_from_sets(called_first, called_second)

