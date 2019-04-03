"""Functions for working with modules used by regression tests."""

import os
import shutil

from diffkemp.config import Config
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder


class TaskSpec:
    """Base class for task specification representing one testing scenario."""
    def __init__(self, spec, tasks_path, task_name, module_dir):
        self.old_kernel = spec["old_kernel"]
        self.new_kernel = spec["new_kernel"]
        self.name = task_name
        self.task_dir = os.path.join(tasks_path, task_name)
        self.debug = spec["debug"] if "debug" in spec else False
        if "control_flow_only" in spec:
            self.control_flow_only = spec["control_flow_only"]
        else:
            self.control_flow_only = False

        # Create LLVM builders and configuration
        self.old_builder = LlvmKernelBuilder(self.old_kernel, module_dir,
                                             debug=self.debug, rebuild=True)
        self.new_builder = LlvmKernelBuilder(self.new_kernel, module_dir,
                                             debug=self.debug, rebuild=True)
        self.config = Config(self.old_builder, self.new_builder, 120, False,
                             self.control_flow_only, False)

    def _file_name(self, suffix, ext, name=None):
        """
        Get name of a task file having the given name, suffix, and extension.
        """
        return os.path.join(self.task_dir,
                            "{}_{}.{}".format(name or self.name, suffix, ext))

    def old_llvm_file(self, name=None):
        """Name of the old LLVM file in the task dir."""
        return self._file_name("old", "ll", name)

    def new_llvm_file(self, name=None):
        """Name of the new LLVM file in the task dir."""
        return self._file_name("new", "ll", name)

    def old_src_file(self, name=None):
        """Name of the old C file in the task dir."""
        return self._file_name("old", "c", name)

    def new_src_file(self, name=None):
        """Name of the new C file in the task dir."""
        return self._file_name("new", "c", name)

    def prepare_dir(self, old_module, new_module, old_src, new_src, name=None):
        """
        Create the task directory and copy the LLVM and the C files there.
        :param old_module: Old LLVM module (instance of LlvmKernelModule).
        :param old_src: C source from the old kernel version to be copied.
        :param new_module: New LLVM module (instance of LlvmKernelModule).
        :param new_src: C source from the new kernel version to be copied.
        :param name: Optional parameter to specify the new file names. If None
                     then the spec name is used.
        """
        if not os.path.isdir(self.task_dir):
            os.mkdir(self.task_dir)

        if not os.path.isfile(self.old_llvm_file(name)):
            shutil.copyfile(old_module.llvm, self.old_llvm_file(name))
        if not os.path.isfile(self.old_src_file(name)):
            shutil.copyfile(old_src, self.old_src_file(name))
        if not os.path.isfile(self.new_llvm_file(name)):
            shutil.copyfile(new_module.llvm, self.new_llvm_file(name))
        if not os.path.isfile(self.new_src_file(name)):
            shutil.copyfile(new_src, self.new_src_file(name))
