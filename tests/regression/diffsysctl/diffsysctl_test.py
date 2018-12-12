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
from diffkemp.llvm_ir.kernel_module import KernelParam
from diffkemp.semdiff.function_diff import functions_diff, Result
from tests.regression.module_tools import prepare_module
import glob
import os
import pytest
import yaml

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
                # First handle the proc handler function
                for function, desc in sysctl["proc_handler"].iteritems():
                    if desc == "skipped":
                        continue

                    spec = dict(spec_yaml)
                    spec["sysctl"] = sysctl["sysctl"]
                    spec["data"] = None
                    spec["function"] = function
                    spec["expected_result"] = desc

                    spec_id = os.path.splitext(spec_file_path)[0] + "_" + \
                        sysctl["sysctl"] + "-" + function
                    result.append((spec_id, spec))
                # Then process the data variable
                data_yaml = sysctl["data_variable"]
                for function, desc in data_yaml["functions"].iteritems():
                    spec = dict(spec_yaml)
                    spec["sysctl"] = sysctl["sysctl"]
                    spec["data"] = data_yaml["name"]
                    spec["function"] = function
                    spec["expected_result"] = desc

                    spec_id = os.path.splitext(spec_file_path)[0] + "_" + \
                        sysctl["sysctl"] + "-" + data_yaml["name"] + "-" + \
                        function
                    result.append((spec_id, spec))

    os.chdir(cwd)
    return result


specs = collect_task_specs()


class TaskSpec:
    """
    Task specification representing one testing scenario (one function - in
    case the function is one of the functions using the data variable, this
    also contains the data variable as the parameter).
    """
    def __init__(self, spec):
        # Values from the YAML file
        self.old_kernel = spec["old_kernel"]
        self.new_kernel = spec["new_kernel"]
        self.sysctl = spec["sysctl"]
        self.data = spec["data"]
        if "control_flow_only" in spec:
            self.control_flow_only = spec["control_flow_only"]
        else:
            self.control_flow_only = False
        self.debug = spec["debug"] if "debug" in spec else False
        self.function = spec["function"]
        self.expected_result = spec["expected_result"]


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules and copy
    created files.
    """
    # Find the modules
    first_builder = LlvmKernelBuilder(spec.old_kernel, None,
                                      debug=spec.debug)
    second_builder = LlvmKernelBuilder(spec.new_kernel, None,
                                       debug=spec.debug)

    # Build the modules
    old_module = first_builder.build_file_for_function(spec.function)
    new_module = second_builder.build_file_for_function(spec.function)

    sysctl_module = first_builder.build_sysctl_module(spec.sysctl)
    if spec.data is not None:
        # Create the data variable KernelParam object
        spec.param = sysctl_module.get_data(spec.sysctl)
    else:
        # Get the name of the proc handler
        spec.param = None
        spec.proc_handler = sysctl_module.get_proc_fun(spec.sysctl)


    # The modules were already built when finding their sources.
    # Now the files need only to be copied to the right place.
    #
    # Note: the files are copied to the task directory for reference only.
    # The exact name of the module is not known before building the
    # function, therefore building from module sources in kernel_modules or
    # using already built LLVM IR files is not possible.
    mod_old = os.path.basename(old_module.name)
    mod_new = os.path.basename(new_module.name)
    spec.task_dir = os.path.join(tasks_path, spec.function)
    if not os.path.isdir(spec.task_dir):
        os.mkdir(spec.task_dir)
    spec.old_src = os.path.join(spec.task_dir, mod_old + "_old.c")
    spec.new_src = os.path.join(spec.task_dir, mod_new + "_new.c")
    spec.old_module = os.path.join(spec.task_dir, mod_old + "_old.ll")
    spec.new_module = os.path.join(spec.task_dir, mod_new + "_new.ll")
    spec.old_simpl = os.path.join(spec.task_dir, mod_old + "_old-simpl.ll")
    spec.new_simpl = os.path.join(spec.task_dir, mod_new + "_new-simpl.ll")

    prepare_module(os.path.dirname(old_module.llvm), mod_old, mod_old + ".c",
                   spec.old_kernel, spec.old_module, spec.old_simpl,
                   spec.old_src, spec.debug, build_module=False)

    prepare_module(os.path.dirname(new_module.llvm), mod_new, mod_new + ".c",
                   spec.new_kernel, spec.new_module, spec.new_simpl,
                   spec.new_src, spec.debug, build_module=False)


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
        if task_spec.data is not None:
            # Data variable function
            if "." in task_spec.data:
                # Structure attribute
                assert task_spec.data.split(".")[0] == task_spec.param.name
            else:
                # Standard variable
                assert task_spec.data == task_spec.param.name
        else:
            # Proc handler function
            assert task_spec.function == task_spec.proc_handler


    def test_diffsysctl(self, task_spec):
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
        if task_spec.expected_result != Result.TIMEOUT:
            result = functions_diff(task_spec.old_module, task_spec.new_module,
                                    task_spec.function, task_spec.function,
                                    task_spec.param, 120, False,
                                    task_spec.control_flow_only)
            assert result == Result[task_spec.expected_result.upper()]
