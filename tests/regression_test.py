#! /usr/bin/env python

from diffkemp.function_comparator import compare_function, Result
from diffkemp.module_comparator import _dependent_functions
from diffkemp.slicer import slicer
import os
import pytest
from subprocess import Popen
import yaml

base_path = "tests/kernel_modules"

def collect_task_spec_files():
    result = list()
    for taskdir in os.listdir(base_path):
        task_spec_file = os.path.join(base_path, taskdir, "test.yaml")
        if os.path.isfile(task_spec_file):
            result.append((taskdir, task_spec_file))
    return result
spec_files = collect_task_spec_files()


class TaskSpec:
    def __init__(self, spec_file_path):
        with open(spec_file_path, "r") as spec_file:
            spec = yaml.load(spec_file)
            self.param = spec["param"]

            self.functions = dict()
            for fun, result in spec["functions"].iteritems():
                self.functions[fun] = Result.from_string(result)

            module = spec["module"]
            module_path = os.path.join(base_path, module)
            self.old = os.path.join(module_path, module + "_old.bc")
            self.new = os.path.join(module_path, module + "_new.bc")
            self.old_sliced = os.path.join(module_path,
                                           module + "_old-sliced.bc")
            self.new_sliced = os.path.join(module_path,
                                           module + "_new-sliced.bc")


@pytest.fixture(params=[x[1] for x in spec_files],
                ids=[x[0] for x in spec_files])
def task_spec(request):
        return TaskSpec(request.param)


class TestClass(object):
    def _run_slicer(self, module, param):
        sliced_module = slicer.sliced_name(module)
        # Delete the sliced file if exists
        try:
            os.remove(sliced_module)
        except OSError:
            pass

        # Slice the module
        slicer.slice_module(module, param, False)
        # Check that the slicer has produced a file
        assert os.path.exists(sliced_module)

        # Check that the produced file contains a valid LLVM bitcode
        opt = Popen(["opt", "-verify", sliced_module],
                    stdout=open("/dev/null", "w"))
        opt.wait()
        assert opt.returncode == 0

    def test_slicer(self, task_spec):
        self._run_slicer(task_spec.old, task_spec.param)
        self._run_slicer(task_spec.new, task_spec.param)

    def test_dependent_functions(self, task_spec):
        result_old = _dependent_functions(task_spec.old_sliced, task_spec.param)
        result_new = _dependent_functions(task_spec.new_sliced, task_spec.param)
        function_names = set(task_spec.functions.keys())
        assert result_old == function_names and result_new == function_names

    def test_function_comparison(self, task_spec):
        for fun, expected in task_spec.functions.iteritems():
            result = compare_function(task_spec.old_sliced,
                                      task_spec.new_sliced,
                                      fun)
            assert result == expected

