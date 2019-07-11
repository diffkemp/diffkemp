"""
List of functions to be compared.
Contains mapping from function names onto corresponding LLVM sources.
"""
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
import os
import yaml


class FunctionList:
    def __init__(self, root_dir):
        self.root_dir = root_dir
        self.functions = dict()

    def add(self, name, llvm_mod):
        """
        Add function to the list.
        :param name: Name of the function.
        :param llvm_mod: LLVM module with the function definition.
        """
        self.functions[name] = llvm_mod

    def modules(self):
        """Get the set of all modules."""
        return set(self.functions.values())

    def get_by_name(self, name):
        """Get module for the function with the given name."""
        return self.functions[name] if name in self.functions else None

    def from_yaml(self, yaml_file):
        """
        Load the list from YAML file. Paths are assumed to be relative to the
        root directory.
        :param yaml_file: Contents of the YAML file.
        """
        funs = yaml.safe_load(yaml_file)
        for f in funs:
            self.add(f["name"],
                     LlvmKernelModule(os.path.join(self.root_dir, f["llvm"])))

    def to_yaml(self):
        """
        Dump the list as a YAML string. Paths to files are given relative to
        the root directory.
        :return: YAML string.
        """
        yaml_dict = [
            {"name": name, "llvm": os.path.relpath(mod.llvm, self.root_dir)}
            for name, mod in self.functions.items()]
        return yaml.dump(yaml_dict)

    def filter(self, functions):
        self.functions = {f: self.functions[f] for f in functions}
