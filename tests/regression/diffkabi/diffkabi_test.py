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

from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
from tests.regression.task_spec import TaskSpec
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


class DiffKabiTaskSpec(TaskSpec):
    """
    Task specification for one KABI function.
    """
    def __init__(self, spec):
        TaskSpec.__init__(self, spec, tasks_path, spec["function"], None)
        self.function = spec["function"]
        self.expected_result = Result.Kind[spec["expected_result"].upper()]


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules and copy the
    created files.
    """
    # Build the modules if needed
    if not os.path.isfile(spec.old_llvm_file()):
        spec.old_module = spec.old_builder.build_file_for_symbol(spec.function)
    else:
        spec.old_module = LlvmKernelModule(spec.function, spec.old_llvm_file(),
                                           "")

    if not os.path.isfile(spec.new_llvm_file()):
        spec.new_module = spec.new_builder.build_file_for_symbol(spec.function)
    else:
        spec.new_module = LlvmKernelModule(spec.function, spec.new_llvm_file(),
                                           "")

    # Copy the source files to the task directory (kernel_modules/function)
    spec.prepare_dir(old_module=spec.old_module,
                     old_src="{}.c".format(spec.old_module.llvm[:-3]),
                     new_module=spec.new_module,
                     new_src="{}.c".format(spec.new_module.llvm[:-3]))


@pytest.fixture(params=[x[1] for x in specs],
                ids=[x[0] for x in specs])
def task_spec(request):
    """pytest fixture to prepare tasks"""
    spec = DiffKabiTaskSpec(request.param)
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
                mod_first=task_spec.old_module,
                mod_second=task_spec.new_module,
                fun_first=task_spec.function, fun_second=task_spec.function,
                glob_var=None, config=task_spec.config)
            assert result.kind == task_spec.expected_result
