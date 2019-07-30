"""
List of functions to be compared.
Contains mapping from function names onto corresponding LLVM sources.
Functions can be divided into groups. This is, e.g., used when comparing sysctl
options.
"""
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
import os
import yaml


class FunctionList:
    class FunctionDesc:
        """
        Function description.
        Contains LLVM module of the function definition, a global variable,
        and a tag.
        For sysctl option, glob_var is the data variable that the option sets.
        """
        def __init__(self, mod, glob_var, tag):
            self.mod = mod
            self.glob_var = glob_var
            self.tag = tag

    class FunctionGroup:
        """
        Group of functions. Used to group functions belonging to a single
        sysctl options.
        """
        def __init__(self):
            self.functions = dict()

    def __init__(self, root_dir, kind=None):
        self.root_dir = root_dir
        self.kind = kind
        self.groups = dict()

    def add(self, name, llvm_mod, glob_var=None, tag=None, group=None):
        """
        Add function to the list.
        :param name: Name of the function.
        :param llvm_mod: LLVM module with the function definition.
        :param tag: Function tag.
        :param group: Group to put the function to.
        """
        self.groups[group].functions[name] = self.FunctionDesc(llvm_mod,
                                                               glob_var, tag)

    def modules(self):
        """Get the set of all LLVM modules of all functions in the list."""
        return set([fun.mod for group in self.groups.values() for fun in
                    group.functions.values()])

    def get_by_name(self, name, group=None):
        """
        Get module for the function with the given name in the given group.
        """
        return self.groups[group].functions[name] \
            if name in self.groups[group].functions else None

    def from_yaml(self, yaml_file):
        """
        Load the list from YAML file. Paths are assumed to be relative to the
        root directory.
        :param yaml_file: Contents of the YAML file.
        """
        groups = yaml.safe_load(yaml_file)
        for g in groups:
            if "sysctl" in g:
                self.kind = "sysctl"
                group = g["sysctl"]
                functions = g["functions"]
            else:
                group = None
                functions = groups
            self.groups[group] = self.FunctionGroup()
            for f in functions:
                self.add(f["name"],
                         LlvmKernelModule(
                             os.path.join(self.root_dir, f["llvm"])),
                         f["glob_var"],
                         f["tag"],
                         group)

    def to_yaml(self):
        """
        Dump the list as a YAML string. Paths to files are given relative to
        the root directory.
        :return: YAML string.
        """
        if None in self.groups:
            yaml_dict = [{
                "name": name,
                "llvm": os.path.relpath(desc.mod.llvm, self.root_dir),
                "glob_var": desc.glob_var,
                "tag": desc.tag
            } for name, desc in self.groups[None].functions.items()]
        else:
            yaml_dict = [
                {self.kind: group_name,
                 "functions": [
                     {"name": fun_name,
                      "llvm": os.path.relpath(fun_desc.mod.llvm,
                                              self.root_dir),
                      "glob_var": fun_desc.glob_var,
                      "tag": fun_desc.tag}
                     for fun_name, fun_desc in g.functions.items()]
                 } for group_name, g in self.groups.items()
            ]
        return yaml.dump(yaml_dict)

    def filter(self, functions, group=None):
        """
        Filter only chosen functions in the given group.
        """
        self.groups[group].functions = {f: self.groups[group].functions[f] for
                                        f in functions}

    def add_group(self, name):
        """
        Add new function group with the given name and global variable.
        """
        self.groups[name] = self.FunctionGroup()

    def add_none_group(self):
        """
        Add new group with None name which is used when only a single group
        is used.
        """
        self.groups[None] = self.FunctionGroup()
