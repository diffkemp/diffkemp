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
    Contains name and optionally a list of indices in case the parameter is
    a member of a composite data type.
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
    def __init__(self, llvm_file, source_file=None):
        self.llvm = llvm_file
        self.source = source_file
        self.llvm_module = None
        self.unlinked_llvm = None
        self.linked_modules = set()

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
                self.linked_modules.update(link_llvm_modules)
                self.parse_module(True)
            except CalledProcessError:
                return False
            finally:
                return any([m for m in link_llvm_modules if self.links_mod(m)])

    def _extract_param_name(self, param_var):
        """
        Extract name of the global variable representing a module parameter
        from the structure describing it.
        :param param_var: LLVM expression containing the variable
        :return Name of the variable
        """
        if param_var.get_kind() == GlobalVariableValueKind:
            param_name = param_var.get_name().decode("utf-8")
            if ("__param_arr" in param_name or
                    "__param_string" in param_name):
                # For array and string parameters, the actual variable is
                # inside another structure as its last element
                param_var_value = param_var.get_initializer()
                # Get the last element
                var_expr = param_var_value.get_operand(
                    param_var_value.get_num_operands() - 1)
                return self._extract_param_name(var_expr)
            else:
                return param_name

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
            return KernelParam(self._extract_param_name(var), [])
        return None

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

    def links_mod(self, mod):
        """
        Check if the given module has been linked to this module
        """
        return mod in self.linked_modules

    def restore_unlinked_llvm(self):
        """Restore the module to the state before any linking was done."""
        if self.unlinked_llvm is not None:
            self.llvm = self.unlinked_llvm
            self.unlinked_llvm = None
            self.linked_modules = set()
            self.parse_module(True)

    def move_to_other_root_dir(self, old_root, new_root):
        """
        Move this LLVM module into a different kernel root directory.
        :param old_root: Kernel root directory to move from.
        :param new_root: Kernel root directory to move to.
        :return:
        """
        if self.llvm.startswith(old_root):
            dest_llvm = os.path.join(new_root,
                                     os.path.relpath(self.llvm, old_root))
            # Copy the .ll file and replace all occurrences of the old root by
            # the new root. There are usually in debug info.
            with open(self.llvm, "r") as llvm:
                with open(dest_llvm, "w") as llvm_new:
                    for line in llvm.readlines():
                        if "constant" not in line:
                            llvm_new.write(line.replace(old_root.strip("/"),
                                                        new_root.strip("/")))
                        else:
                            llvm_new.write(line)
            self.llvm = dest_llvm

        if self.source and self.source.startswith(old_root):
            # Copy the C source file.
            dest_source = os.path.join(new_root,
                                       os.path.relpath(self.source, old_root))
            shutil.copyfile(self.source, dest_source)
            self.source = dest_source

    def get_included_headers(self):
        """
        Get the list of headers that this module includes.
        Requires debugging information.
        """
        # Search for all .h files mentioned in the debug info.
        pattern = re.compile(r"filename:\s*\"([^\"]*)\"")
        result = set()
        with open(self.llvm, "r") as llvm:
            for line in llvm.readlines():
                s = pattern.search(line)
                if s and s.group(1).endswith(".h"):
                    result.add(s.group(1))
        return result
