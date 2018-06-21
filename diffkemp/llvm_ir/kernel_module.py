"""
Kernel modules in LLVM IR.
Functions for working with parameters of modules.
"""

from diffkemp.slicer.slicer import slice_module
from llvmcpy.llvm import *
import os
import re
from subprocess import check_output


class ModuleParam:
    """
    Kernel module parameter.
    Has name, type, and description.
    """
    def __init__(self, name, varname, ctype, desc, default_value):
        self.name = name
        self.varname = varname
        self.ctype = ctype
        self.desc = desc
        self.default_value = default_value


class LlvmKernelModule:
    """
    Kernel module in LLVM IR
    """
    def __init__(self, name, file_name, module_dir):
        self.name = name
        self.llvm = os.path.join(module_dir, "{}.bc".format(file_name))
        self.kernel_object = os.path.join(module_dir,
                                          "{}.ko".format(file_name))
        self.params = dict()
        self.llvm_module = None

    def _parse_module(self):
        """Parse module file into LLVM module using llvmcpy library"""
        buffer = create_memory_buffer_with_contents_of_file(self.llvm)
        context = get_global_context()
        self.llvm_module = context.parse_ir(buffer)

    def _extract_param_name(self, param_var):
        """
        Extract name of the global variable representing a module parameter
        from the structure describing it.

        :param param_var: LLVM expression containing the variable
        :return Name of the variable
        """
        if param_var.get_kind() != GlobalVariableValueKind:
            # Variable can be inside bitcast or getelementptr, in both cases
            # it is inside the first operand of the expression
            return self._extract_param_name(param_var.get_operand(0))

        if ("__param_arr" in param_var.get_name() or
                "__param_string" in param_var.get_name()):
            # For array and string parameters, the actual variable is inside
            # another structure as its last element
            param_var_value = param_var.get_initializer()
            # Get the last element
            var_expr = param_var_value.get_operand(
                param_var_value.get_num_operands() - 1)
            return self._extract_param_name(var_expr)

        return param_var.get_name()

    def _extract_param_default_value(self, param):
        """Extract parameter default value from LLVM IR."""
        regex = re.compile(r"@{}\s=.*".format(param))
        with open(self.llvm) as llvm_ir_file:
            for line in llvm_ir_file:
                if regex.search(line):
                    return line.rstrip()

    def find_param_var(self, param):
        """
        Find global variable in the module that corresponds to the given param.
        In case the param is defined by module_param_named, this can be
        different from the param name.
        Information about the variable is stored inside the last element of the
        structure assigned to the '__param_#name' variable (#name is the name
        of param).
        :param param
        :return Name of the global variable corresponding to param
        """
        glob = self.llvm_module.get_named_global("__param_{}".format(param))
        if not glob:
            return None
        # Get value of __param_#name variable
        glob_value = glob.get_initializer()
        # Get the last element
        var_union = glob_value.get_operand(glob_value.get_num_operands() - 1)
        # Last element is a struct with single element, get it
        var = var_union.get_operand(0)

        return self._extract_param_name(var)

    def collect_all_parameters(self):
        """
        Collect all parameters defined in the module.
        This is done by parsing output of `modinfo -p module.ko`.
        """
        self._parse_module()
        self.params = dict()
        with open(os.devnull, "w") as stderr:
            modinfo = check_output(["modinfo", "-p", self.kernel_object],
                                   stderr=stderr)
        lines = modinfo.splitlines()
        for line in lines:
            name, sep, rest = line.partition(":")
            desc, sep, ctype = rest.partition(" (")
            ctype = ctype[:-1]
            varname = self.find_param_var(name)
            if varname:
                default_value = self._extract_param_default_value(varname)
                self.params[name] = ModuleParam(
                    name, varname, ctype, desc, default_value)

    def set_param(self, param):
        self.params = {param: ModuleParam(param, param, None, None, None)}

    def slice(self, param, verbose=False):
        """
        Slice the module w.r.t. to the given parameter.
        """
        sliced = slice_module(self.llvm, param, verbose)
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
