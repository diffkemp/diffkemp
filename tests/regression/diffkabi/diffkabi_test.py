"""
Regression testing using pytest.
Individual tests are specified using YAML in the tests/regression/test_specs/
directory. Each test describes a kernel module, two kernel versions between
which the module is compared, and a list of compared module parameters. For
each parameter, pairs of corresponding functions (using the parameter) along
with the expected analysis results must be provided.
This script parses the test specification and prepares testing scenarions for
pytest.
"""

from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.semdiff.function_diff import functions_diff, Result
import glob
import os
import shutil
import pytest
import yaml

specs_path = "tests/regression/diffkabi/test_specs"
tasks_path = "tests/regression/diffkabi/kernel_modules"


def collect_task_specs():
    """Collecting and parsing YAML files with test specifications."""
    result = list()
    cwd = os.getcwd()
    os.chdir(specs_path)
    for spec_file_path in glob.glob("*.yaml"):
        with open(spec_file_path, "r") as spec_file:
            spec_yaml = yaml.load(spec_file)
            if "disabled" in spec_yaml and spec_yaml["disabled"] is True:
                continue
            spec_id = os.path.splitext(spec_file_path)[0]
            result.append((spec_id, spec_yaml))
    os.chdir(cwd)
    return result


specs = collect_task_specs()


class TaskSpec:
    """
    Task specification representing one testing scenario (a kernel module with
    a parameter)
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
        self.functions = spec["functions"]


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules and copy
    created files.
    """
    spec.built_functions = dict()

    for function, desc in spec.functions.iteritems():
        # Find the modules
        first_builder = LlvmKernelBuilder(spec.old_kernel, None,
                                          debug=spec.debug)
        first_builder.source.build_cscope_database()
        second_builder = LlvmKernelBuilder(spec.new_kernel, None,
                                           debug=spec.debug)
        second_builder.source.build_cscope_database()

        # Build the modules
        mod_first = first_builder.build_llvm_function(function)
        mod_second = second_builder.build_llvm_function(function)

        # Add the function to the dictionary
        fun = (function, mod_first, mod_second)
        spec.built_functions[fun] = Result[desc.upper()]

        # The modules were already built when finding their sources.
        # Now the files need only to be copied to the right place.
        #
        # Note: the files are copied to the task directory for reference only.
        # The exact name of the module is not known before building the
        # function, therefore building from module sources in kernel_modules or
        # using already built LLVM IR files is not possible.
        mod_path_old = mod_first.llvm
        mod_path_new = mod_second.llvm
        mod_old = os.path.basename(mod_first.name)
        mod_new = os.path.basename(mod_second.name)
        task_dir = os.path.join(tasks_path, mod_old)
        if not os.path.isdir(tasks_path):
            os.mkdir(tasks_path)
        if not os.path.isdir(task_dir):
            os.mkdir(task_dir)
        old_src = os.path.join(task_dir, mod_old + "_old.c")
        new_src = os.path.join(task_dir, mod_new + "_new.c")
        old_llvm = os.path.join(task_dir, mod_old + "_old.ll")
        new_llvm = os.path.join(task_dir, mod_new + "_new.ll")
        old_source_src = os.path.splitext(mod_path_old)[0] + ".c"
        new_source_src = os.path.splitext(mod_path_new)[0] + ".c"

        shutil.copyfile(mod_path_old, old_llvm)
        shutil.copyfile(old_source_src, old_src)

        shutil.copyfile(mod_path_new, new_llvm)
        shutil.copyfile(new_source_src, new_src)


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
    task. Contains 2 tests for function couplings collection and for the
    actual function analysis (semantic comparison).
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
        for function, expected in task_spec.built_functions.iteritems():
            if expected != Result.TIMEOUT:
                result = functions_diff(function[1].llvm, function[2].llvm,
                                        function[0], function[0],
                                        None, 120,
                                        control_flow_only =
                                        task_spec.control_flow_only)
                assert result == expected
