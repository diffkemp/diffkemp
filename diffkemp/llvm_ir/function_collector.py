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

    def clean(self):
        """Free the inner LLVM module."""
        self._module.dispose()

    # List of standard functions that are supported, so they should not be
    # included in function collecting.
    # Some functions have multiple variants so we need to check for prefix
    supported_names = ["llvm.dbg.value", "llvm.dbg.declare"]
    supported_prefixes = ["llvm.lifetime", "kmalloc", "kzalloc", "malloc",
                          "calloc", "__kmalloc", "devm_kzalloc"]

    def _indices_correspond(self, gep, indices):
        """
        Checks whether the indices in the GEP correspond to the indices in
        the list. When one list is longer than the other, the function behaves
        like one is cut to the size of the other and compares the rest.
        :param gep: The GEP operator. Both the instruction and the constant
        expression are supported.
        :param indices: A list of integers representing indices to compare the
        GEP operator with.
        :return: True or false based on whether the indices correspond.
        """
        for i in range(1, gep.get_num_operands()):
            if (i - 1) >= len(indices):
                break
            op = gep.get_operand(i)
            if (op.is_constant() and
                    op.const_int_get_z_ext() != indices[i - 1]):
                return False
        return True

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
                    if (called.get_kind() == ConstantExprValueKind):
                        # Called function may be inside a bitcast
                        called = called.get_operand(0)
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
        glob = self._module.get_named_global(param.name)
        result = set()
        for use in glob.iter_uses():
            if use.user.get_kind() == InstructionValueKind:
                # Use is an Instruction
                bb = use.user.instruction_parent
                func = bb.parent
                if (use.user.is_a_get_element_ptr_inst() and
                        param.indices is not None):
                    # Look whether the GEP references the desired index or not
                    if not self._indices_correspond(use.user, param.indices):
                        continue
                if ".old" not in func.name.decode("utf-8"):
                    result.add(func.name.decode("utf-8"))
            elif use.user.get_kind() == ConstantExprValueKind:
                # Use is an Operator (typicalluy GEP)
                for inner_use in use.user.iter_uses():
                    if inner_use.user.get_kind() == InstructionValueKind:
                        bb = inner_use.user.instruction_parent
                        func = bb.parent
                        if (use.user.get_const_opcode() ==
                                Opcode['GetElementPtr'] and
                                param.indices is not None):
                            # Look whether the GEP references the desired
                            # index or not
                            if not self._indices_correspond(use.user,
                                                            param.indices):
                                continue
                        if ".old" not in func.name.decode("utf-8"):
                            result.add(func.name.decode("utf-8"))
        return result

    def called_by(self, function_names):
        """
        Find names of all functions (recursively) called by one of functions
        in the given set.
        """
        result = set()
        for fun_name in function_names:
            llvm_fun = self._module.get_named_function(fun_name)
            if llvm_fun:
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
                result.add(llvm_fun.name.decode("utf-8"))
        return result
