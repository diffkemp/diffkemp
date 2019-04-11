"""
Regression testing using pytest.
Individual tests are specified using YAML in the
tests/regression/test_specs/diffsysctl directory. Each test contains a list of
sysctl parameters - for each parameter the proc handler function is specified
along with the data variable and all functions using it.
For each function the expected analysis results is specified.
This script parses the test specification and prepares testing scenarions for
pytest.
"""

from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
from tests.regression.task_spec import TaskSpec
import glob
import os
import pytest
import yaml


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


specs_path = "tests/regression/diffsysctl/test_specs"
tasks_path = "tests/regression/diffsysctl/kernel_modules"


def collect_task_specs():
    """Collecting and parsing YAML files with test specifications."""
    result = list()
    if not os.path.isdir(tasks_path):
        os.mkdir(tasks_path)
    cwd = os.getcwd()
    os.chdir(specs_path)
    for spec_file_path in glob.glob("*.yaml"):
        with open(spec_file_path, "r") as spec_file:
            try:
                spec_yaml = yaml.safe_load(spec_file)
                if "disabled" in spec_yaml and spec_yaml["disabled"] is True:
                    continue
                for sysctl in spec_yaml["sysctls"]:
                    proc_yaml = sysctl["proc_handler"]
                    data_yaml = sysctl["data_variable"]

                    spec = dict(spec_yaml)
                    spec["sysctl"] = sysctl["sysctl"]
                    spec["data"] = data_yaml["name"]
                    spec["data_functions"] = data_yaml["functions"]
                    spec["proc_handler"] = proc_yaml

                    spec_id = os.path.splitext(spec_file_path)[0] + "_" + \
                        sysctl["sysctl"]
                    result.append((spec_id, spec))
            except yaml.YAMLError:
                pass

    os.chdir(cwd)
    return result


specs = collect_task_specs()


class DiffSysctlTaskSpec(TaskSpec):
    """
    Task specification for one sysctl parameter
    """
    def __init__(self, spec):
        TaskSpec.__init__(self, spec, tasks_path, spec["sysctl"], None)
        # Values from the YAML file
        self.sysctl = spec["sysctl"]
        self.data = spec["data"]
        self.data_functions = list()
        for function, result in spec["data_functions"].iteritems():
            self.data_functions.append(FunctionSpec(function, result))
        self.proc_handler = list()
        for function, result in spec["proc_handler"].iteritems():
            self.proc_handler = FunctionSpec(function, result)


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules and copy
    created files.
    """
    function_list = [spec.proc_handler] + spec.data_functions

    for function in function_list:
        if function.result == Result.Kind.NONE:
            continue

        # Build the modules for the function
        if not os.path.isfile(spec.old_llvm_file(function.name)):
            function.old_module = spec.old_builder.build_file_for_symbol(
                function.name)
        else:
            function.old_module = LlvmKernelModule(
                function.name, spec.old_llvm_file(function.name), "")
        if not os.path.isfile(spec.new_llvm_file(function.name)):
            function.new_module = spec.new_builder.build_file_for_symbol(
                function.name)
        else:
            function.new_module = LlvmKernelModule(
                function.name, spec.new_llvm_file(function.name), "")

        # Copy the source files to the task directory (kernel_modules/sysctl)
        spec.prepare_dir(old_module=function.old_module,
                         old_src="{}.c".format(function.old_module.llvm[:-3]),
                         new_module=function.new_module,
                         new_src="{}.c".format(function.new_module.llvm[:-3]),
                         name=function.name)


@pytest.fixture(params=[x[1] for x in specs],
                ids=[x[0] for x in specs])
def task_spec(request):
    """pytest fixture to prepare tasks"""
    spec = DiffSysctlTaskSpec(request.param)
    prepare_task(spec)
    return spec


class TestClass(object):
    """
    Main testing class. One object of the class is created for each testing
    task. Contains a test for finding and semantically comparing either proc
    handler function of the sysctl parameter or function using the data global
    variable and a test for checking whether the data variable and the proc
    handler function were determined correctly.
    """
    def test_llvm_sysctl_module(self, task_spec):
        """
        Looks whether the data variable and the proc handler function were
        determined correctly.
        """
        # Get the relevant values
        sysctl_module = task_spec.old_builder.build_sysctl_module(
            task_spec.sysctl)
        sysctl_module.parse_sysctls(task_spec.sysctl)

        data = sysctl_module.get_data(task_spec.sysctl)
        proc_handler = sysctl_module.get_proc_fun(task_spec.sysctl)

        # Data variable function
        if "." in task_spec.data:
            # Structure attribute
            assert task_spec.data.split(".")[0] == data.name
        else:
            # Standard variable
            assert task_spec.data == data.name
        # Proc handler function
        assert task_spec.proc_handler.name == proc_handler

    def test_proc_handler(self, task_spec):
        """
        Test comparison of semantic difference of both functions using the data
        variable associated with the sysctl parameter and the proc_handler
        function.
        For each compared function, the module is first simplified using the
        SimpLL tool and then the actual analysis is run. Compares the obtained
        result with the expected one.
        If timeout is expected, the analysis is not run to increase testing
        speed.
        """
        if task_spec.proc_handler.result not in [Result.Kind.TIMEOUT,
                                                 Result.Kind.NONE]:
            result = functions_diff(
                mod_first=task_spec.proc_handler.old_module,
                mod_second=task_spec.proc_handler.new_module,
                fun_first=task_spec.proc_handler.name,
                fun_second=task_spec.proc_handler.name,
                glob_var=None, config=task_spec.config)

            assert result.kind == task_spec.proc_handler.result

    def test_data_functions(self, task_spec):
        """
        Test comparison of semantic difference of both functions using the data
        variable associated with the sysctl parameter and the proc_handler
        function.
        For each compared function, the module is first simplified using the
        SimpLL tool and then the actual analysis is run. Compares the obtained
        result with the expected one.
        If timeout is expected, the analysis is not run to increase testing
        speed.
        """
        # Get the data variable KernelParam object
        sysctl_module = task_spec.old_builder.build_sysctl_module(
            task_spec.sysctl)
        data_kernel_param = sysctl_module.get_data(task_spec.sysctl)

        for function in task_spec.data_functions:
            if function.result != Result.Kind.TIMEOUT:
                result = functions_diff(mod_first=function.old_module,
                                        mod_second=function.new_module,
                                        fun_first=function.name,
                                        fun_second=function.name,
                                        glob_var=data_kernel_param,
                                        config=task_spec.config)
                assert result.kind == function.result
