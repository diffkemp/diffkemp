#! /usr/bin/env python

from diffkemp.function_comparator import compare_functions, Result
from diffkemp.function_coupling import FunctionCouplings
from diffkemp.slicer import slicer
import os
import pytest
from subprocess import Popen
import yaml

base_path = "tests/kernel_modules"

def collect_task_specs():
    result = list()
    for taskdir in os.listdir(base_path):
        task_spec_file = os.path.join(base_path, taskdir, "test.yaml")
        if not os.path.isfile(task_spec_file):
            continue
        with open(task_spec_file, "r") as spec_file:
            spec_yaml = yaml.load(spec_file)
            for param in spec_yaml["params"]:
                spec = param
                spec["module"] = spec_yaml["module"]
                spec_id = spec["module"] + "-" + spec["param"]
                result.append((spec_id, spec))
    return result
specs = collect_task_specs()


class TaskSpec:
    def __init__(self, spec):
        self.param = spec["param"]

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

        module = spec["module"]
        module_path = os.path.join(base_path, module)
        self.old = os.path.join(module_path, module + "_old.bc")
        self.new = os.path.join(module_path, module + "_new.bc")
        self.old_sliced = os.path.join(module_path, module + "_old-sliced-" +
                                                    spec["param"] + ".bc")
        self.new_sliced = os.path.join(module_path, module + "_new-sliced-" +
                                                    spec["param"] + ".bc")


@pytest.fixture(params=[x[1] for x in specs],
                ids=[x[0] for x in specs])
def task_spec(request):
        return TaskSpec(request.param)


class TestClass(object):
    def _run_slicer(self, module, param, out_file):
        # Delete the sliced file if exists
        try:
            os.remove(out_file)
        except OSError:
            pass

        # Slice the module
        slicer.slice_module(module, param, out_file, False)
        # Check that the slicer has produced a file
        assert os.path.exists(out_file)

        # Check that the produced file contains a valid LLVM bitcode
        opt = Popen(["opt", "-verify", out_file],
                    stdout=open("/dev/null", "w"))
        opt.wait()
        assert opt.returncode == 0

    def test_slicer(self, task_spec):
        self._run_slicer(task_spec.old, task_spec.param, task_spec.old_sliced)
        self._run_slicer(task_spec.new, task_spec.param, task_spec.new_sliced)

    def test_couplings(self, task_spec):
        couplings = FunctionCouplings(task_spec.old_sliced,
                                      task_spec.new_sliced)
        couplings.infer_for_param(task_spec.param)

        assert couplings.main == set(task_spec.functions.keys())
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

