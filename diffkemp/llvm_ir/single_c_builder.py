"""
LLVM source builder for single C file.
"""
from diffkemp.llvm_ir.single_llvm_finder import SingleLlvmFinder
from diffkemp.utils import get_functions_from_llvm
import os
from subprocess import check_call


class SingleCBuilder(SingleLlvmFinder):
    """
    LLVM source builder for single C file.
    Extends the SingleLlvmFinder class by compiling C file to LLVM IR file.
    """
    def __init__(self, source_dir, c_file_name, clang="clang",
                 clang_append=[]):
        """
        :param clang: clang compiler to be used
        :param clang_append: list of args to add when compiling
        """
        llvm_file_name = os.path.splitext(c_file_name)[0] + ".ll"
        SingleLlvmFinder.__init__(self, source_dir, llvm_file_name)

        self.c_file_name = c_file_name
        self.clang = clang
        self.clang_append = clang_append
        self.initialize()

    def str(self):
        return "single_c_file"

    def initialize(self):
        # Using clang to compile c file to llvm file.
        command = [self.clang, "-S", "-emit-llvm", "-g", self.c_file_name,
                   "-o", self.llvm_file_name]
        command.extend(self.clang_append)
        check_call(command, cwd=self.source_dir)

    def get_function_list(self):
        """Returns a list of functions that are found in the source file."""
        return list(get_functions_from_llvm(
            [os.path.join(self.source_dir, self.llvm_file_name)]).keys())
