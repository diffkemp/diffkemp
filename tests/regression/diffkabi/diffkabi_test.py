"""
Regression testing using pytest.
Individual tests are specified using YAML in the
tests/regression/test_specs/diffkabi directory. Each test contains a list of
KABI functions and two kernel versions between which the functions are
compared.
For each function the expected analysis results is specified.
This script parses the test specification and prepares testing scenarions for
pytest.
"""

from diffkemp.config import Config
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from tests.regression.module_tools import prepare_module
import glob
import os
import pytest
import yaml

specs_path = "tests/regression/diffkabi/test_specs"
tasks_path = "tests/regression/diffkabi/kernel_modules"


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
            for function, desc in spec_yaml["functions"].iteritems():
                spec = dict(spec_yaml)
                spec["function"] = function
                spec["expected_result"] = desc

                spec_id = os.path.splitext(spec_file_path)[0] + "_" + function
                result.append((spec_id, spec))
    os.chdir(cwd)
    return result


specs = collect_task_specs()


class TaskSpec:
    """
    Task specification representing one testing scenario (one KABI function).
    """
    def __init__(self, spec):
        # Values from the YAML file
        self.old_kernel = spec["old_kernel"]
        self.new_kernel = spec["new_kernel"]
        if "control_flow_only" in spec:
            self.control_flow_only = spec["control_flow_only"]
        else:
            self.control_flow_only = False
        self.debug = spec["debug"] if "debug" in spec else False
        self.function = spec["function"]
        self.expected_result = Result.Kind[spec["expected_result"].upper()]
        self.config = None


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules and copy
    created files.
    """
    # Find the modules
    first_builder = LlvmKernelBuilder(spec.old_kernel, None,
                                      debug=spec.debug, rebuild=True)
    second_builder = LlvmKernelBuilder(spec.new_kernel, None,
                                       debug=spec.debug, rebuild=True)

    spec.config = Config(first_builder, second_builder, 120, False,
                         spec.control_flow_only, False)

    # Build the modules
    old_module = first_builder.build_file_for_symbol(spec.function)
    new_module = second_builder.build_file_for_symbol(spec.function)

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
    task. Contains a test for finding and semantically comparing KABI
    functions.
    """

    def test_diffkabi(self, task_spec):
        """
        Test comparison of semantic difference of functions from the KABI
        whitelist.
        For each compared function, the module is first simplified using the
        SimpLL tool and then the actual analysis is run. Compares the obtained
        result with the expected one.
        If timeout is expected, the analysis is not run to increase testing
        speed.
        """
        if task_spec.expected_result != Result.Kind.TIMEOUT:
            result = functions_diff(
                file_first=task_spec.old_module,
                file_second=task_spec.new_module,
                fun_first=task_spec.function, fun_second=task_spec.function,
                glob_var=None, config=task_spec.config)
            assert result.kind == task_spec.expected_result
