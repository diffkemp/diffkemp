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

from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from tests.regression.module_tools import prepare_module
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
            spec_yaml = yaml.load(spec_file)
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

    os.chdir(cwd)
    return result


specs = collect_task_specs()


class TaskSpec:
    """
    Task specification representing one testing scenario (one sysctl parameter)
    """
    def __init__(self, spec):
        # Values from the YAML file
        self.old_kernel = spec["old_kernel"]
        self.new_kernel = spec["new_kernel"]
        self.sysctl = spec["sysctl"]
        self.data = spec["data"]
        self.data_functions = list()
        for function, result in spec["data_functions"].iteritems():
            self.data_functions.append(FunctionSpec(function, result))
        self.proc_handler = list()
        for function, result in spec["proc_handler"].iteritems():
            self.proc_handler = FunctionSpec(function, result)
        if "control_flow_only" in spec:
            self.control_flow_only = spec["control_flow_only"]
        else:
            self.control_flow_only = False
        self.debug = spec["debug"] if "debug" in spec else False


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules and copy
    created files.
    """
    function_list = [spec.proc_handler] + spec.data_functions

    # Find the modules
    first_builder = LlvmKernelBuilder(spec.old_kernel, None,
                                      debug=spec.debug)
    second_builder = LlvmKernelBuilder(spec.new_kernel, None,
                                       debug=spec.debug)

    # Build the modules
    for function in function_list:
        old_module = first_builder.build_file_for_symbol(function.name)
        new_module = second_builder.build_file_for_symbol(function.name)

        # The modules were already built when finding their sources.
        # Now the files need only to be copied to the right place.
        #
        # Note: the files are copied to the task directory for reference only.
        # The exact name of the module is not known before building the
        # function, therefore building from module sources in kernel_modules or
        # using already built LLVM IR files is not possible.
        mod_old = os.path.basename(old_module.name)
        mod_new = os.path.basename(new_module.name)
        spec.task_dir = os.path.join(tasks_path, function.name)
        if not os.path.isdir(spec.task_dir):
            os.mkdir(spec.task_dir)
        old_src = os.path.join(spec.task_dir, mod_old + "_old.c")
        new_src = os.path.join(spec.task_dir, mod_new + "_new.c")
        function.old_module = os.path.join(spec.task_dir, mod_old + "_old.ll")
        function.new_module = os.path.join(spec.task_dir, mod_new + "_new.ll")
        old_simpl = os.path.join(spec.task_dir, mod_old + "_old-simpl.ll")
        new_simpl = os.path.join(spec.task_dir, mod_new + "_new-simpl.ll")

        prepare_module(os.path.dirname(old_module.llvm), mod_old,
                       mod_old + ".c", spec.old_kernel, function.old_module,
                       old_simpl, old_src, spec.debug, build_module=False)

        prepare_module(os.path.dirname(new_module.llvm), mod_new,
                       mod_new + ".c", spec.new_kernel, function.new_module,
                       new_simpl, new_src, spec.debug, build_module=False)


@pytest.fixture(params=[x[1] for x in specs],
                ids=[x[0] for x in specs])
def task_spec(request):
    """pytest fixture to prepare tasks"""
    spec = TaskSpec(request.param)
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
        builder = LlvmKernelBuilder(task_spec.old_kernel, None,
                                    debug=task_spec.debug)
        sysctl_module = builder.build_sysctl_module(task_spec.sysctl)
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
            result = functions_diff(task_spec.proc_handler.old_module,
                                    task_spec.proc_handler.new_module,
                                    task_spec.proc_handler.name,
                                    task_spec.proc_handler.name,
                                    None, 120, False,
                                    task_spec.control_flow_only)

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
        builder = LlvmKernelBuilder(task_spec.old_kernel, None,
                                    debug=task_spec.debug)
        sysctl_module = builder.build_sysctl_module(task_spec.sysctl)
        data_kernel_param = sysctl_module.get_data(task_spec.sysctl)

        for function in task_spec.data_functions:
            if function.result != Result.Kind.TIMEOUT:
                result = functions_diff(function.old_module,
                                        function.new_module,
                                        function.name, function.name,
                                        data_kernel_param,
                                        120, False,
                                        task_spec.control_flow_only,
                                        verbose=True)
                assert result.kind == function.result
