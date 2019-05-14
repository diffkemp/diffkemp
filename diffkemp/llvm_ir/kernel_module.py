"""
Kernel modules in LLVM IR.
Functions for working with parameters of modules.
"""

from llvmcpy.llvm import *
import os
from subprocess import check_call, check_output, CalledProcessError


class KernelModuleException(Exception):
    pass


class KernelParam:
    """
    Kernel parameter.
    Contains name and optionally a list of indices.
    """
    def __init__(self, name, indices=None):
        self.name = name
        self.indices = indices

    def __str__(self):
        return self.name


class LlvmKernelModule:
    """
    Kernel module in LLVM IR
    """
    def __init__(self, name, file_name, module_dir):
        self.name = name
        file_basename = os.path.splitext(file_name)[0]
        self.llvm = os.path.join(module_dir, "{}.ll".format(file_basename))
        self.llvm_module = None
        kernel_object = os.path.join(module_dir, "{}.ko".format(file_basename))
        if os.path.isfile(kernel_object):
            self.kernel_object = kernel_object
            self.get_depends()
        else:
            self.kernel_object = None
            self.depends = None
        self.params = dict()
        self.unlinked_llvm = None
        self.linked_modules = set()

    def get_depends(self):
        """
        Retrieve list other modules that this module depends on.
        Can be found using modinfo command.
        The list is stored into self.depends.
        """
        with open(os.devnull, "w") as stderr:
            modinfo = check_output(["modinfo", "-F", "depends",
                                   self.kernel_object], stderr=stderr)
            self.depends = modinfo.rstrip().split(",")

    def parse_module(self, force=False):
        """Parse module file into LLVM module using llvmcpy library"""
        if force or self.llvm_module is None:
            buffer = create_memory_buffer_with_contents_of_file(self.llvm)
            context = get_global_context()
            self.llvm_module = context.parse_ir(buffer)

    def clean_module(self):
        """Free the parsed LLVM module."""
        if self.llvm_module is not None:
            self.llvm_module.dispose()
            self.llvm_module = None

    @staticmethod
    def clean_all():
        """Clean all statically managed LLVM memory."""
        shutdown()

    def _update_module_file(self):
        """Update module file with changes done to parsed LLVM IR"""
        self.llvm_module.write_bitcode_to_file(self.llvm)

    def link_modules(self, modules):
        """Link module against a list of other modules."""
        link_llvm_modules = [m for m in modules if not self.links_mod(m)]

        if not link_llvm_modules:
            return False

        if "-linked" not in self.llvm:
            new_llvm = "{}-linked.ll".format(self.llvm[:-3])
        else:
            new_llvm = self.llvm
        link_command = ["llvm-link", "-S", self.llvm]
        link_command.extend([m.llvm for m in link_llvm_modules])
        link_command.extend(["-o", new_llvm])
        opt_command = ["opt", "-S", "-constmerge", "-mergefunc", new_llvm,
                       "-o", new_llvm]
        with open(os.devnull, "w") as devnull:
            try:
                check_call(link_command, stdout=devnull, stderr=devnull)
                check_call(opt_command, stdout=devnull, stderr=devnull)
                if self.unlinked_llvm is None:
                    self.unlinked_llvm = self.llvm
                self.llvm = new_llvm
                self.linked_modules.update([m.name for m in link_llvm_modules])
                self.parse_module(True)
            except CalledProcessError:
                return False
            finally:
                return any([m for m in link_llvm_modules if self.links_mod(m)])

    def _remove_global(self, glob_name):
        """Remove global variable or alias with the given name if it exists."""
        globvar = self.llvm_module.get_named_global(glob_name)
        if globvar:
            globvar.delete()

    def _globvar_exists(self, param):
        """Check if a global variable with the given name exists."""
        return self.llvm_module.get_named_global(param) is not None

    def _extract_param_name(self, param_var):
        """
        Extract name of the global variable representing a module parameter
        from the structure describing it.

        :param param_var: LLVM expression containing the variable
        :return Name of the variable
        """
        if param_var.get_kind() == GlobalVariableValueKind:
            if ("__param_arr" in param_var.get_name() or
                    "__param_string" in param_var.get_name()):
                # For array and string parameters, the actual variable is
                # inside another structure as its last element
                param_var_value = param_var.get_initializer()
                # Get the last element
                var_expr = param_var_value.get_operand(
                    param_var_value.get_num_operands() - 1)
                return self._extract_param_name(var_expr)
            else:
                return param_var.get_name()

        if param_var.get_num_operands() == 1:
            # Variable can be inside bitcast or getelementptr, in both cases
            # it is inside the first operand of the expression
            return self._extract_param_name(param_var.get_operand(0))

        return None

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
        if var_union.get_num_operands() == 1:
            # Last element should be a struct with single element, get it
            var = var_union.get_operand(0)
            return self._extract_param_name(var)
        else:
            return None

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
            varname = self.find_param_var(name)
            if varname:
                self.params[name] = KernelParam(varname)

    def set_param(self, param):
        """Set single parameter"""
        globvar = param
        if not self._globvar_exists(param):
            globvar = self.find_param_var(param)
        if globvar:
            self.params = {globvar: KernelParam(globvar)}
        else:
            raise KernelModuleException("Parameter {} not found".format(param))

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

    def has_function(self, fun):
        """Check if module contains a function definition."""
        llvm_fun = self.llvm_module.get_named_function(fun)
        return llvm_fun is not None and not llvm_fun.is_declaration()

    def has_global(self, glob):
        """Check if module contains a global variable with the given name."""
        return self.llvm_module.get_named_global(glob) is not None

    def is_declaration(self, fun):
        """
        Check if the given function is a declaration (does not have body).
        """
        llvm_fun = self.llvm_module.get_named_function(fun)
        return (llvm_fun.get_kind() == FunctionValueKind and
                llvm_fun.is_declaration())

    def get_filename(self):
        """
        Get name of the source file for this module.
        """
        self.parse_module()
        len = ffi.new("size_t *")
        try:
            name = self.llvm_module.get_source_file_name(len)
            return name
        except RuntimeError:
            return None

    def links_mod(self, module):
        """
        Check if the given module has been linked to this module
        """
        return module.name in self.linked_modules

    def restore_unlinked_llvm(self):
        """Restore the module to the state before any linking was done."""
        if self.unlinked_llvm is not None:
            self.llvm = self.unlinked_llvm
            self.unlinked_llvm = None
            self.linked_modules = set()
            self.parse_module(True)
