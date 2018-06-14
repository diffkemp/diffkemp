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
from diffkemp.semdiff.function_coupling import FunctionCouplings
from diffkemp.slicer.slicer import slice_module
import glob
import os
import pytest
import shutil
import yaml

specs_path = "tests/regression/test_specs"
tasks_path = "tests/regression/kernel_modules"


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
            # One specification for each analysed parameter is created
            for param in spec_yaml["params"]:
                spec = param
                spec["module"] = spec_yaml["module"]
                spec["path"] = spec_yaml["path"]
                spec["filename"] = spec_yaml["filename"]
                spec["old_kernel"] = spec_yaml["old_kernel"]
                spec["new_kernel"] = spec_yaml["new_kernel"]
                if "debug" in spec_yaml:
                    spec["debug"] = True
                else:
                    spec["debug"] = False
                spec_id = spec["module"] + "-" + spec["param"]
                result.append((spec_id, spec))
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
        module = spec["module"]
        self.module = module
        self.param = spec["param"]
        self.module_dir = spec["path"]
        self.module_src = spec["filename"]
        self.old_kernel = spec["old_kernel"]
        self.new_kernel = spec["new_kernel"]
        self.debug = spec["debug"]

        # The dictionary mapping pairs of corresponding analysed functions into
        # expected results.
        # Special values 'only_old' and 'only_new' denote functions that occur
        # in a single version of the module only.
        self.functions = dict()
        self.only_old = set()
        self.only_new = set()
        for fun, desc in spec["functions"].iteritems():
            # If only a single function is specified, both compared functions
            # are supposed to have the same name
            if isinstance(fun, str):
                fun = (fun, fun)
            try:
                self.functions[fun] = Result[desc.upper()]
            except KeyError:
                if desc == "only_old":
                    self.only_old.add(fun[0])
                elif desc == "only_new":
                    self.only_new.add(fun[0])

        # Names of files
        self.task_dir = os.path.join(tasks_path, module)
        self.old = os.path.join(self.task_dir, module + "_old.bc")
        self.new = os.path.join(self.task_dir, module + "_new.bc")
        self.old_sliced = os.path.join(self.task_dir, module + "_old-" +
                                       spec["param"] + ".bc")
        self.new_sliced = os.path.join(self.task_dir, module + "_new-" +
                                       spec["param"] + ".bc")
        self.old_src = os.path.join(self.task_dir, module + "_old.c")
        self.new_src = os.path.join(self.task_dir, module + "_new.c")


def _build_module(kernel_version, module_dir, module, debug):
    """
    Build LLVM IR of the analysed module.
    """
    builder = LlvmKernelBuilder(kernel_version, module_dir, debug)
    llvm_mod = builder.build_module(module, True)
    return llvm_mod


def _copy_files(unsliced_src, unsliced_dest,
                sliced_src, sliced_dest,
                source_src, source_dest):
    """Copy .bc file, sliced .bc file, and .c file"""
    shutil.copyfile(unsliced_src, unsliced_dest)
    shutil.copyfile(sliced_src, sliced_dest)
    shutil.copyfile(source_src, source_dest)


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules if needed and
    copy created files.
    """
    # Create task dir
    if not os.path.isdir(spec.task_dir):
        os.mkdir(spec.task_dir)

    # Prepare old module
    if not os.path.isfile(spec.old):
        first_mod = _build_module(spec.old_kernel, spec.module_dir,
                                  spec.module, spec.debug)
        first_sliced = first_mod.slice(spec.param)
        first_src = os.path.join(os.path.dirname(first_mod.llvm),
                                 spec.module_src)
        _copy_files(first_mod.llvm, spec.old, first_sliced, spec.old_sliced,
                    first_src, spec.old_src)

    # Prepare new module
    if not os.path.isfile(spec.new):
        second_mod = _build_module(spec.new_kernel, spec.module_dir,
                                   spec.module, spec.debug)
        second_sliced = second_mod.slice(spec.param)
        second_src = os.path.join(os.path.dirname(second_mod.llvm),
                                  spec.module_src)
        _copy_files(second_mod.llvm, spec.new, second_sliced, spec.new_sliced,
                    second_src, spec.new_src)


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
    task. Contains 3 tests for slicing, function couplings collection, and the
    actual function analysis (semantic comparison).
    """
    def test_slicer(self, task_spec):
        """
        Test slicer. Will raise exception if the slicer does not produce valid
        LLVM IR code. Also this method is useful so that slicing is re-run
        before each analysis.
        """
        first = slice_module(task_spec.old, task_spec.param)
        os.rename(first, task_spec.old_sliced)
        second = slice_module(task_spec.new, task_spec.param)
        os.rename(second, task_spec.new_sliced)

    def test_couplings(self, task_spec):
        """
        Test collection of function couplings. Checks whether the collected
        couplings of main functions (functions using the paramter of the
        analysis) match functions specified in the test spec.
        """
        couplings = FunctionCouplings(task_spec.old_sliced,
                                      task_spec.new_sliced)
        couplings.infer_for_param(task_spec.param)

        coupled = set([(c.first, c.second) for c in couplings.main])
        assert coupled == set(task_spec.functions.keys())
        assert couplings.uncoupled_first == task_spec.only_old
        assert couplings.uncoupled_second == task_spec.only_new

    def test_function_comparison(self, task_spec):
        """
        Test the actual comparison of semantic difference of modules w.r.t. a
        parameter. Runs the analysis for each function couple and compares the
        result with the expected one.
        If timeout is expected, the analysis is not run to increase testing
        speed.
        """
        couplings = FunctionCouplings(task_spec.old_sliced,
                                      task_spec.new_sliced)
        couplings.infer_for_param(task_spec.param)
        for funPair, expected in task_spec.functions.iteritems():
            if expected != Result.TIMEOUT:
                result = functions_diff(task_spec.old_sliced,
                                        task_spec.new_sliced,
                                        funPair[0], funPair[1],
                                        couplings.called,
                                        timeout=120)
                assert result == expected
