"""
Kernel modules in LLVM IR.
Functions for working with parameters of modules.
"""

from diffkemp.slicer.slicer import slice_module
import os
from subprocess import check_output

class ModuleParam:
    """
    Kernel module parameter.
    Has name, type, and description.
    """
    def __init__(self, name, ctype, desc):
        self.name = name
        self.ctype = ctype
        self.desc = desc


class LlvmKernelModule:
    """
    Kernel module in LLVM IR
    """
    def __init__(self, name, file_name, module_dir):
        self.name = name
        self.llvm = os.path.join(module_dir, "%s.bc" % file_name)
        self.kernel_object = os.path.join(module_dir, "%s.ko" % file_name)
        self.params = dict()


    def collect_all_parameters(self):
        """
        Collect all parameters defined in the module.
        This is done by parsing output of `modinfo -p module.ko`.
        """
        self.params = dict()
        with open(os.devnull, "w") as stderr:
            modinfo = check_output(["modinfo", "-p", self.kernel_object],
                                   stderr=stderr)
        lines = modinfo.splitlines()
        for line in lines:
            name, sep, rest = line.partition(":")
            desc, sep, ctype = rest.partition(" (")
            ctype = ctype[:-1]
            self.params[name] = ModuleParam(name, ctype, desc)


    def set_param(self, param):
        self.params = {param: ModuleParam(param, None, None)}


    def slice(self, param):
        """
        Slice the module w.r.t. to the given parameter.
        """
        sliced = slice_module(self.llvm, param)
        return sliced


    def collect_functions(self):
        """
        Collect main and called functions for the module.
        Main functions are those that directly use the analysed parameter and
        that will be compared to corresponding functions of the other module.
        Called functions are those that are (recursively) called by main
        functions.
        """
        collector = FunctionCollector(self.llvm)
        self.main_functions = collector.using_param(self.param)
        self.called_functions = collector.called_by(self.main_functions)

