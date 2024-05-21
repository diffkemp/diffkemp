"""
LLVM IR modules.
Functions for working with modules and parameters in them.
"""

from diffkemp.simpll.library import SimpLLModule
from diffkemp.simpll._simpll.lib import shutdownSimpLL
from diffkemp.utils import get_opt_command
import os
import re
import shutil
from subprocess import check_call, CalledProcessError

# Set of standard functions that are supported, so they should not be
# included in function collecting.
# Some functions have multiple variants so we need to check for prefix
supported_names = {"llvm.dbg.value", "llvm.dbg.declare"}
supported_prefixes = {"llvm.lifetime", "kmalloc", "kzalloc", "malloc",
                      "calloc", "__kmalloc", "devm_kzalloc"}


def supported_fun(llvm_fun):
    """Check whether the function is supported."""
    name = llvm_fun.get_name().decode("utf-8")
    if name:
        return (name in supported_names or
                any([name.startswith(p) for p in supported_prefixes]))


class LlvmParam:
    """
    A parameter represented by a (global) variable and an optional list of
    field indices in case the parameter is a member of a composite data type.
    The indices correspond to the indices used in LLVM GEP instruction to get
    the address of the particular element within the given variable.
    """

    def __init__(self, name, indices=None):
        self.name = name
        self.indices = indices

    def __str__(self):
        return self.name


class LlvmModule:
    """
    Representation of a module in LLVM IR
    """

    def __init__(self, llvm_file, source_file=None):
        self.llvm = llvm_file
        self.source = source_file
        self.llvm_module = None
        self.unlinked_llvm = None
        self.linked_modules = set()

    def parse_module(self, force=False):
        """Parse module file into LLVM module using SimpLL"""
        if force or self.llvm_module is None:
            self.llvm_module = SimpLLModule(self.llvm)

    def clean_module(self):
        """Free the parsed LLVM module."""
        if self.llvm_module is not None:
            self.llvm_module = None

    @staticmethod
    def clean_all():
        """Clean all statically managed LLVM memory."""
        shutdownSimpLL()

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
        opt_command = get_opt_command([("constmerge", "module")], new_llvm)
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

    def find_param_var(self, param):
        """
        Find global variable in the module that corresponds to the given param.
        In case the param is defined by module_param_named, this can be
        different from the param name.
        Information about the variable is stored inside the last element of the
        structure assigned to the '__param_#name' variable (#name is the name
        of param).
        :param param Parameter name
        :return Name of the global variable corresponding to the parameter
        """
        self.parse_module()
        name = self.llvm_module.find_param_var(param)
        return LlvmParam(name, []) if name is not None else None

    def has_function(self, fun):
        """Check if module contains a function definition."""
        pattern = re.compile(r"^define.*@{}\(".format(fun), flags=re.MULTILINE)
        with open(self.llvm, "r") as llvm_file:
            return pattern.search(llvm_file.read()) is not None

    def has_global(self, glob):
        """Check if module contains a global variable with the given name."""
        pattern = re.compile(r"^@{}\s*=".format(glob), flags=re.MULTILINE)
        with open(self.llvm, "r") as llvm_file:
            return pattern.search(llvm_file.read()) is not None

    def is_declaration(self, fun):
        """
        Check if the given function is a declaration (does not have body).
        """
        self.parse_module()
        llvm_fun = self.llvm_module.get_function(fun)
        return llvm_fun.is_declaration()

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
        Move this LLVM module into a different project root directory.
        :param old_root: Project root directory to move from.
        :param new_root: Project root directory to move to.
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

    def get_included_sources(self):
        """
        Get the list of source files that this module includes.
        Requires debugging information.
        """
        # Search for all .h files mentioned in the debug info.
        pattern = re.compile(r"filename:\s*\"([^\"]*)\", "
                             r"directory:\s*\"([^\"]*)\"")
        result = set()
        with open(self.llvm, "r") as llvm:
            for line in llvm.readlines():
                s = pattern.search(line)
                if (s and (s.group(1).endswith(".h") or
                           s.group(1).endswith(".c"))):
                    result.add(os.path.join(s.group(2), s.group(1)))
        return result

    def get_functions_using_param(self, param):
        """
        Find names of all functions using the given parameter (global variable)
        """
        self.parse_module()
        return self.llvm_module.get_functions_using_param(param.name,
                                                          param.indices)

    def get_functions_called_by(self, fun_name):
        """
        Find names of all functions (recursively) called by one of functions
        in the given set.
        """
        self.parse_module()
        llvm_fun = self.llvm_module.get_function(fun_name)
        if llvm_fun:
            return {f.get_name() for f in llvm_fun.get_called_functions()
                    if f.get_name() not in supported_names.union({fun_name})}
