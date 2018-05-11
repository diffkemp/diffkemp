from llvmcpy.llvm import *


class FunctionCollector():
    def __init__(self, module_file):
        # Parse LLVM module
        buffer = create_memory_buffer_with_contents_of_file(module_file)
        context = get_global_context()
        self._module = context.parse_ir(buffer)


    # List of standard functions that are supported, so they should not be
    # included in couplings
    # Some functions have multiple variants so we need to check for prefix
    supported_names = ["malloc", "calloc", "kmalloc", "kzalloc", "__kmalloc",
                       "devm_kzalloc",
                       "llvm.dbg.value", "llvm.dbg.declare"]
    supported_prefixes = ["llvm.memcpy", "llvm.lifetime"]

    @staticmethod
    def supported_fun(llvm_fun):
        name = llvm_fun.get_name()
        if name:
            if name in FunctionCollector.supported_names:
                return True
            for p in FunctionCollector.supported_prefixes:
                if name.startswith(p):
                    return True
        return False


    # Find names of all functions that are (recursively) called by given 
    # function
    @staticmethod
    def _called_by_one(llvm_fun, result):
        for bb in llvm_fun.iter_basic_blocks():
            for instr in bb.iter_instructions():
                if instr.get_instruction_opcode() == Call:
                    called = instr.get_called()
                    if (called.get_kind() == FunctionValueKind and
                        called.get_name() and
                        not FunctionCollector.supported_fun(called) and
                        not called.get_name() in result):
                        result.add(called.get_name())
                        FunctionCollector._called_by_one(called, result)

                for opIndex in range(0, instr.get_num_operands()):
                    op = instr.get_operand(opIndex)

                    if (op.get_kind() == FunctionValueKind and
                        op.get_name() and
                        not FunctionCollector.supported_fun(op) and
                        not op.get_name() in result):
                        result.add(op.get_name())


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

