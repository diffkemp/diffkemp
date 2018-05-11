#! /usr/bin/env python

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
    result = list()
    cwd = os.getcwd()
    os.chdir(specs_path)
    for spec_file_path in glob.glob("*.yaml"):
        with open(spec_file_path, "r") as spec_file:
            spec_yaml = yaml.load(spec_file)
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
    def __init__(self, spec):
        self.param = spec["param"]
        self.module_path = spec["path"]
        self.module_filename = spec["filename"]
        self.old_kernel = spec["old_kernel"]
        self.new_kernel = spec["new_kernel"]
        self.debug = spec["debug"]

        self.functions = dict()
        self.only_old = set()
        self.only_new = set()
        for fun, desc in spec["functions"].iteritems():
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
        self.task_dir = os.path.join(tasks_path, module)
        self.old = os.path.join(self.task_dir, module + "_old.bc")
        self.new = os.path.join(self.task_dir, module + "_new.bc")
        self.old_sliced = os.path.join(self.task_dir, module + "_old-sliced-" +
                                                      spec["param"] + ".bc")
        self.new_sliced = os.path.join(self.task_dir, module + "_new-sliced-" +
                                                      spec["param"] + ".bc")


def _build_module(kernel_version, module_path, module_file, param, debug):
    module = build_llvm.LlvmKernelModule(kernel_version, module_path,
                                         module_file, param)
    module.build(debug)
    return module


def _copy_files(module, ir_file, sliced_ir_file):
    # Copy .bc file, sliced .bc file, and .c file   
    shutil.copyfile(module.llvm_unsliced, ir_file)
    shutil.copyfile(module.llvm, sliced_ir_file)
    shutil.copyfile(module.src, ir_file[:-3] + ".c")


def prepare_task(spec):
    # Create task dir
    if not os.path.isdir(spec.task_dir):
        os.mkdir(spec.task_dir)

    # Compile old module
    if not os.path.isfile(spec.old):
        first_mod = _build_module(spec.old_kernel, spec.module_path,
                                  spec.module_filename, spec.param,
                                  spec.debug)
        _copy_files(first_mod, spec.old, spec.old_sliced)

    if not os.path.isfile(spec.new):
        second_mod = _build_module(spec.new_kernel, spec.module_path,
                                   spec.module_filename, spec.param,
                                   spec.debug)
        _copy_files(second_mod, spec.new, spec.new_sliced)


@pytest.fixture(params=[x[1] for x in specs],
                ids=[x[0] for x in specs])
def task_spec(request):
    spec = TaskSpec(request.param)
    prepare_task(spec)
    return spec


class TestClass(object):
    def test_slicer(self, task_spec):
        first = slicer.slice_module(task_spec.old, task_spec.param)
        os.rename(first, task_spec.old_sliced)
        second = slicer.slice_module(task_spec.new, task_spec.param)
        os.rename(second, task_spec.new_sliced)


    def test_couplings(self, task_spec):
        couplings = FunctionCouplings(task_spec.old_sliced,
                                      task_spec.new_sliced)
        couplings.infer_for_param(task_spec.param)

        coupled = set([(c.first, c.second) for c in couplings.main])
        assert coupled == set(task_spec.functions.keys())
        assert couplings.uncoupled_first == task_spec.only_old
        assert couplings.uncoupled_second == task_spec.only_new

    def test_function_comparison(self, task_spec):
        couplings = FunctionCouplings(task_spec.old_sliced,
                                      task_spec.new_sliced)
        couplings.infer_for_param(task_spec.param)
        for funPair, expected in task_spec.functions.iteritems():
            result = compare_functions(task_spec.old_sliced,
                                       task_spec.new_sliced,
                                       funPair[0], funPair[1],
                                       couplings.called)
            assert result == expected

