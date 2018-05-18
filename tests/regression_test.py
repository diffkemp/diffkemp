#! /usr/bin/env python

"""
Regression testing using pytest.
Individual tests are specified using YAML in
the tests/test_specs/ directory. Each test describes a kernel module, two
kernel versions between which the module is compared, and a list of compared
module parameters. For each parameter, pairs of corresponding functions (using
the parameter) along with the expected analysis results must be provided.
This script parses the test specification and prepares testing scenarions for
pytest.
"""

from diffkemp.llvm_ir import build_llvm
from diffkemp.function_comparator import compare_functions, Result
from diffkemp.function_coupling import FunctionCouplings
from diffkemp.slicer import slicer
import glob
import os
import pytest
import shutil
from subprocess import Popen
import yaml

specs_path = "tests/test_specs"
tasks_path = "tests/kernel_modules"

def collect_task_specs():
    """Collecting and parsing YAML files with test specifications."""
    result = list()
    cwd = os.getcwd()
    os.chdir(specs_path)
    for spec_file_path in glob.glob("*.yaml"):
        with open(spec_file_path, "r") as spec_file:
            spec_yaml = yaml.load(spec_file)
            # One specification for each analysed parameter is created
            for param in spec_yaml["params"]:
                spec = param
                spec["module_name"] = spec_yaml["module"]
                spec["path"] = spec_yaml["path"]
                spec["filename"] = spec_yaml["filename"]
                spec["old_kernel"] = spec_yaml["old_kernel"]
                spec["new_kernel"] = spec_yaml["new_kernel"]
                if "debug" in spec_yaml:
                    spec["debug"] = True
                else:
                    spec["debug"] = False
                spec_id = spec["module_name"] + "-" + spec["param"]
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
        self.param = spec["param"]
        self.module_path = spec["path"]
        self.module_filename = spec["filename"]
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

        module = spec["module_name"]
        # Names of files
        self.task_dir = os.path.join(tasks_path, module)
        self.old = os.path.join(self.task_dir, module + "_old-" + spec["param"]
                                + ".bc")
        self.new = os.path.join(self.task_dir, module + "_new-" + spec["param"]
                                + ".bc")
        self.old_sliced = os.path.join(self.task_dir, module + "_old-sliced-" +
                                                      spec["param"] + ".bc")
        self.new_sliced = os.path.join(self.task_dir, module + "_new-sliced-" +
                                                      spec["param"] + ".bc")
        self.old_src = os.path.join(self.task_dir, module + "_old.c")
        self.new_src = os.path.join(self.task_dir, module + "_new.c")


def _build_module(kernel_version, module_path, module_file, param, debug):
    """
    Build LLVM IR of the analysed module. The unsliced file is linked as
    well, so that the slicer can be re-run without the need to rebuild the
    whole module.
    """
    module = build_llvm.LlvmKernelModule(kernel_version, module_path,
                                         module_file, param, debug)
    module.build()
    module.link_unsliced()
    return module


def _copy_files(module, ir_file, sliced_ir_file, src_file):
    """Copy .bc file, sliced .bc file, and .c file"""
    shutil.copyfile(module.llvm_unsliced, ir_file)
    shutil.copyfile(module.llvm, sliced_ir_file)
    shutil.copyfile(module.src, src_file)


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules if needed and
    copy created files.
    """
    # Create task dir
    if not os.path.isdir(spec.task_dir):
        os.mkdir(spec.task_dir)

    # Compile old module
    if not os.path.isfile(spec.old):
        first_mod = _build_module(spec.old_kernel, spec.module_path,
                                  spec.module_filename, spec.param,
                                  spec.debug)
        _copy_files(first_mod, spec.old, spec.old_sliced, spec.old_src)

    # Compile new module
    if not os.path.isfile(spec.new):
        second_mod = _build_module(spec.new_kernel, spec.module_path,
                                   spec.module_filename, spec.param,
                                   spec.debug)
        _copy_files(second_mod, spec.new, spec.new_sliced, spec.new_src)


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
        first = slicer.slice_module(task_spec.old, task_spec.param)
        os.rename(first, task_spec.old_sliced)
        second = slicer.slice_module(task_spec.new, task_spec.param)
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
                result = compare_functions(task_spec.old_sliced,
                                           task_spec.new_sliced,
                                           funPair[0], funPair[1],
                                           couplings.called)
                assert result == expected

