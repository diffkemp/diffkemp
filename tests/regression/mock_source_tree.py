"""
Class representing a mock source tree used in regression tests.

The source tree expects the source dir to contain the necessary LLVM IR
(and possibly C) files with known names. In case such files exist, the LLVM IR
modules are created from them. Otherwise, the real source tree must exist,
which is used to get the modules. After that, the modules (LLVM IR files) are
cached to the source directory for future use.
"""

import os
import shutil

from diffkemp.llvm_ir.source_tree import SourceTree
from diffkemp.llvm_ir.llvm_module import LlvmModule
from diffkemp.llvm_ir.llvm_sysctl_module import LlvmSysctlModule


class MockSourceTree(SourceTree):
    def __init__(self, source_dir, real_source_tree):
        SourceTree.__init__(self, source_dir)
        self.real_source_tree = real_source_tree

    def get_module_for_symbol(self, symbol, created_before=None):
        llvm_file = os.path.join(self.source_dir, "{}.ll".format(symbol))
        src_file = os.path.join(self.source_dir, "{}.c".format(symbol))

        if not os.path.exists(llvm_file) and self.real_source_tree is not None:
            mod = self.real_source_tree.get_module_for_symbol(symbol,
                                                              created_before)
            shutil.copyfile(mod.llvm, llvm_file)
            if mod.source is not None:
                shutil.copyfile(mod.source, src_file)

        return LlvmModule(llvm_file, src_file)

    def get_kernel_module(self, mod_dir, mod_name):
        llvm_file = os.path.join(self.source_dir, "{}.ll".format(mod_name))

        if not os.path.exists(llvm_file):
            assert self.real_source_tree is not None
            mod = self.real_source_tree.get_kernel_module(mod_dir, mod_name)
            shutil.copyfile(mod.llvm, llvm_file)

        return LlvmModule(llvm_file)

    def get_sysctl_module(self, sysctl):
        llvm_file = os.path.join(self.source_dir, "{}.ll".format(sysctl))
        table_file = os.path.join(self.source_dir, "table")

        if not os.path.exists(llvm_file):
            assert self.real_source_tree is not None
            sysctl = self.real_source_tree.get_sysctl_module(sysctl)
            shutil.copyfile(sysctl.mod.llvm, llvm_file)
            table = sysctl.ctl_table
            with open(table_file, "w") as tf:
                tf.write(table)
        else:
            assert os.path.isfile(table_file)
            with open(table_file, "r") as tf:
                table = tf.read()

        return LlvmSysctlModule(LlvmModule(llvm_file), table)
