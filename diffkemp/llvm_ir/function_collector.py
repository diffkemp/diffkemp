"""Collecting functions inside LLVM modules by various criteria"""
from llvmcpy.llvm import *


class FunctionCollector():
    """
    Class for function collection.
    Uses the llvmcpy module to parse and analyse LLVM IR.
    """
    def __init__(self, module_file):
        # Parse LLVM module
        buffer = create_memory_buffer_with_contents_of_file(module_file)
        context = get_global_context()
        self._module = context.parse_ir(buffer)

    # List of standard functions that are supported, so they should not be
    # included in function collecting.
    # Some functions have multiple variants so we need to check for prefix
    supported_names = ["llvm.dbg.value", "llvm.dbg.declare"]
    supported_prefixes = ["llvm.lifetime", "kmalloc", "kzalloc", "malloc",
                          "calloc", "__kmalloc", "devm_kzalloc"]

    @staticmethod
    def supported_fun(llvm_fun):
        """Check whether the function is supported."""
        name = llvm_fun.get_name()
        if name:
            if name in FunctionCollector.supported_names:
                return True
            for p in FunctionCollector.supported_prefixes:
                if name.startswith(p):
                    return True
        return False

    @staticmethod
    def _called_by_one(llvm_fun, result):
        """
        Find names of all functions that are (recursively) called by the given
        function. Found names are stored in the 'result' list.
        """
        for bb in llvm_fun.iter_basic_blocks():
            for instr in bb.iter_instructions():
                if instr.get_instruction_opcode() == Call:
                    called = instr.get_called()
                    # Collect all unsupported functions that are called
                    if (called.get_kind() == FunctionValueKind and
                            called.get_name() and
                            not FunctionCollector.supported_fun(called) and
                            not called.get_name() in result):
                        result.add(called.get_name())
                        # Recursively call the method on the called function
                        FunctionCollector._called_by_one(called, result)

                # Collect also functions that are passed as parameters to
                # instructions. For these, do not descend recursively since
                # their body will not be analysed.
                for opIndex in range(0, instr.get_num_operands()):
                    op = instr.get_operand(opIndex)

                    if (op.get_kind() == FunctionValueKind and
                            op.get_name() and
                            not FunctionCollector.supported_fun(op) and
                            not op.get_name() in result):
                        result.add(op.get_name())

    def using_param(self, param):
        """
        Find names of all functions using the given parameter (global variable)
        """
        glob = self._module.get_named_global(param)
        result = set()
        for use in glob.iter_uses():
            if use.user.get_kind() == InstructionValueKind:
                # Use is an Instruction
                bb = use.user.instruction_parent
                func = bb.parent
                result.add(func.name)
            elif use.user.get_kind() == ConstantExprValueKind:
                # Use is an Operator (typicalluy GEP)
                for inner_use in use.user.iter_uses():
                    if inner_use.user.get_kind() == InstructionValueKind:
                        bb = inner_use.user.instruction_parent
                        func = bb.parent
                        result.add(func.name)
        return result

    def called_by(self, function_names):
        """
        Find names of all functions (recursively) called by one of functions
        in the given set.
        """
        result = set()
        for fun_name in function_names:
            llvm_fun = self._module.get_named_function(fun_name)
            self._called_by_one(llvm_fun, result)
        return result

    def undefined(self, function_names):
        """
        Find names of functions from the given set that are not defined in the
        analysed module.
        """
        result = set()
        for fun_name in function_names:
            llvm_fun = self._module.get_named_function(fun_name)
            if llvm_fun.is_declaration():
                result.add(llvm_fun.name)
        return result
