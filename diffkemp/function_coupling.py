from llvmcpy.llvm import *
import itertools


# Rule for testing that one function name is prefix/suffix of the other
def prefix_rule(fun1, fun2):
    return (fun1.startswith(fun2) or fun1.endswith(fun2) or
            fun2.startswith(fun1) or fun2.endswith(fun1))


class FunctionCollector():
    def __init__(self, module_file):
        # Parse LLVM module
        buffer = create_memory_buffer_with_contents_of_file(module_file)
        context = get_global_context()
        self._module = context.parse_ir(buffer)


    # List of standard functions that are supported, so they should not be
    # included in couplings
    supported_names = ["malloc", "calloc", "kmalloc", "kzalloc",
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
    def _called_by_one(llvm_fun):
        result = set()
        for bb in llvm_fun.iter_basic_blocks():
            for instr in bb.iter_instructions():
                if instr.get_instruction_opcode() != Call:
                    continue
                called = instr.get_called()
                if called.get_name():
                    if not FunctionCollector.supported_fun(called):
                        result.add(called.get_name())
                    if not called.is_declaration():
                        result.update(FunctionCollector._called_by_one(called))

        return result


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
            result.update(self._called_by_one(llvm_fun))
        return result


# Determine which functions from the set are not coupled with anything in 
# couplings
def uncoupled(functions, couplings):
    coupled = set(itertools.chain.from_iterable(couplings))
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

        # First couple all functions that have same name
        for fun in functions_first & functions_second:
            couplings.add((fun,fun))

        uncoupled_first = uncoupled(functions_first, couplings)
        uncoupled_second = uncoupled(functions_second, couplings)
        # Now try to apply some predefined naming rules to the rest of 
        # functions and find other couplings
        for fun in uncoupled_first:
            candidates = [f for f in uncoupled_second if prefix_rule(f, fun)]
            if len(candidates) == 1:
                couplings.add((fun, candidates[0]))

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

