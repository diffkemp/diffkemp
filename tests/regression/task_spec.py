"""Functions for working with modules used by regression tests."""

import os

from diffkemp.config import Config, BuiltinPatterns
from diffkemp.llvm_ir.kernel_llvm_source_builder import KernelLlvmSourceBuilder
from diffkemp.llvm_ir.kernel_source_tree import KernelSourceTree
from diffkemp.semdiff.custom_pattern_config import CustomPatternConfig
from diffkemp.semdiff.result import Result
from diffkemp.snapshot import Snapshot
from diffkemp.utils import get_llvm_version

from .mock_source_tree import MockSourceTree


base_path = os.path.abspath(".")
custom_patterns_path = os.path.abspath("tests/regression/custom_patterns")
specs_path = os.path.abspath("tests/regression/test_specs")
tasks_path = os.path.abspath("tests/regression/test_data")


class FunctionSpec:
    """"
    Specification of a function in kernel along the modules/source files where
    it is located and the expected result of the function comparison between
    two kernel versions.
    """
    def __init__(self, name, result, old_module=None, new_module=None):
        self.name = name
        self.old_module = old_module
        self.new_module = new_module
        if result == "generic":
            self.result = Result.Kind.NONE
        else:
            self.result = Result.Kind[result.upper()]


class TaskSpec:
    """
    Task specification representing testing scenario.
    Contains a list of functions to be compared with DiffKemp during the test.
    """
    def __init__(self, spec, task_name, kernel_path):
        self.name = task_name
        self.task_dir = os.path.join(tasks_path, task_name)
        # Paths to test data directories (test_data/<taskname>/{old,new})
        self.old_source_dir = os.path.join(self.task_dir, "old")
        self.new_source_dir = os.path.join(self.task_dir, "new")

        # If paths to the real kernel sources exist, create corresponding
        # KernelSourceTree objects
        self.old_kernel_dir = os.path.join(kernel_path, spec["old_kernel"])
        self.new_kernel_dir = os.path.join(kernel_path, spec["new_kernel"])
        self.old_kernel_source_tree = None
        self.new_kernel_source_tree = None
        if os.path.isdir(self.old_kernel_dir):
            self.old_kernel_source_tree = KernelSourceTree(
                self.old_kernel_dir,
                KernelLlvmSourceBuilder(self.old_kernel_dir))
        if os.path.isdir(self.new_kernel_dir):
            self.new_kernel_source_tree = KernelSourceTree(
                self.new_kernel_dir,
                KernelLlvmSourceBuilder(self.new_kernel_dir))

        if not os.path.isdir(self.task_dir):
            os.mkdir(self.task_dir)
            os.mkdir(self.old_source_dir)
            os.mkdir(self.new_source_dir)

        # Create MockSourceTree objects which will be used to get LLVM IR
        # modules. These are primarily taken from the task data directory,
        # but if they don't exist there, we pass additional KernelSourceTree
        # objects for build the required modules.
        self.old_kernel = MockSourceTree(self.old_source_dir,
                                         self.old_kernel_source_tree)
        self.new_kernel = MockSourceTree(self.new_source_dir,
                                         self.new_kernel_source_tree)

        # Custom pattern configuration
        custom_pattern_config = None
        if "custom_pattern_config" in spec:
            if get_llvm_version() >= 15:
                config_filename = spec["custom_pattern_config"]["opaque"]
            else:
                config_filename = spec["custom_pattern_config"]["explicit"]
            custom_pattern_config = CustomPatternConfig.create_from_file(
                path=os.path.join(custom_patterns_path, config_filename),
                patterns_path=base_path
            )
        # Builtin pattern configuration
        builtin_patterns = BuiltinPatterns(
            control_flow_only=spec.get("control_flow_only", False),
        )

        # Create Config object. It requires snapshots so we just provide the
        # kernel directories.
        self.old_snapshot = Snapshot(self.old_kernel, self.old_kernel)
        self.new_snapshot = Snapshot(self.new_kernel, self.new_kernel)
        self.config = Config(snapshot_first=self.old_snapshot,
                             snapshot_second=self.new_snapshot,
                             custom_pattern_config=custom_pattern_config,
                             builtin_patterns=builtin_patterns)
        self.functions = dict()

    def finalize(self):
        """
        Task finalization - should be called before the task is destroyed.
        """
        self.old_kernel.finalize()
        self.new_kernel.finalize()

    def add_function_spec(self, fun, result):
        """Add a function comparison specification."""
        self.functions[fun] = FunctionSpec(fun, result)

    def build_modules_for_function(self, fun):
        """
        Build LLVM modules containing definition of the compared function in
        both kernels.
        """
        # Since PyTest may share KernelSourceTree objects among tasks, we need
        # to explicitly initialize kernels.
        self.old_kernel.initialize()
        self.new_kernel.initialize()
        mod_old = self.old_kernel.get_module_for_symbol(fun)
        mod_new = self.new_kernel.get_module_for_symbol(fun)
        self.functions[fun].old_module = mod_old
        self.functions[fun].new_module = mod_new
        return mod_old, mod_new


class SysctlTaskSpec(TaskSpec):
    """
    Task specification for test of sysctl comparison.
    Extends TaskSpec by data variable and proc handler function.
    """
    def __init__(self, spec, task_name, kernel_path, sysctl, data_var):
        TaskSpec.__init__(self, spec, task_name, kernel_path)
        self.sysctl = sysctl
        self.data_var = data_var
        self.proc_handler = None
        self.old_sysctl_module = None
        self.new_sysctl_module = None

    def add_proc_handler(self, proc_handler, result):
        """Add proc handler function to the spec."""
        self.proc_handler = proc_handler
        self.add_function_spec(proc_handler, result)

    def get_proc_handler_spec(self):
        """Retrieve specification of the proc handler function."""
        return self.functions[self.proc_handler]

    def add_data_var_function(self, fun, result):
        """Add a new function using the data variable."""
        self.add_function_spec(fun, result)

    def build_sysctl_module(self):
        """Build the compared sysctl modules into LLVM."""
        self.old_sysctl_module = self.old_kernel.get_sysctl_module(self.sysctl)
        self.new_sysctl_module = self.new_kernel.get_sysctl_module(self.sysctl)


class ModuleParamSpec(TaskSpec):
    """
    Task specification for test of kernel module parameter comparison.
    Extends TaskSpec by module and parameter specification.
    """
    def __init__(self, spec, task_name, kernel_path, dir, mod, param):
        TaskSpec.__init__(self, spec, task_name, kernel_path)
        self.dir = dir
        self.mod = mod
        self.param = param
        self.old_module = None
        self.new_module = None

    def build_module(self):
        """Build the compared kernel modules into LLVM."""
        self.old_module = self.old_kernel.get_kernel_module(self.dir, self.mod)
        self.new_module = self.new_kernel.get_kernel_module(self.dir, self.mod)

    def get_param(self):
        """Get the name of the global variable representing the parameter."""
        return self.old_module.find_param_var(self.param)


class DiffSpec:
    """
    Specification of a syntax difference. Contains the name of the differing
    symbol and its old and new definition.
    """
    def __init__(self, symbol, diff):
        self.symbol = symbol
        self.diff = diff


class SyntaxDiffSpec(TaskSpec):
    """
    Task specification for test of syntax difference.
    Extends TaskSpec by concrete syntax differences that should be found by
    DiffKemp. These are currently intended to be macros or inline assemblies.
    """
    def __init__(self, spec, task_name, kernel_path):
        TaskSpec.__init__(self, spec, task_name, kernel_path)
        self.equal_symbols = set()
        self.syntax_diffs = dict()

    def add_equal_symbol(self, symbol):
        """Add a symbol that should not be present in the result"""
        self.equal_symbols.add(symbol)

    def add_syntax_diff_spec(self, symbol, diff):
        """Add an expected syntax difference"""
        self.syntax_diffs[symbol] = DiffSpec(symbol, diff)
