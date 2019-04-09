"""
Regression testing using pytest.
Individual tests are specified using YAML in the
tests/regression/diffkemp/test_specs/ directory. Each test describes a kernel
module, two kernel versions between which the module is compared, and a list of
compared module parameters. For each parameter, pairs of corresponding
functions (using the parameter) along with the expected analysis results must
be provided. This script parses the test specification and prepares testing
scenarions for pytest.
"""

from diffkemp.llvm_ir.kernel_module import KernelParam, LlvmKernelModule
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.function_coupling import FunctionCouplings
from diffkemp.semdiff.result import Result
from tests.regression.task_spec import TaskSpec
import glob
import os
import pytest
import yaml

specs_path = "tests/regression/diffkemp/test_specs"
tasks_path = "tests/regression/diffkemp/kernel_modules"


def collect_task_specs():
    """Collecting and parsing YAML files with test specifications."""
    result = list()
    cwd = os.getcwd()
    os.chdir(specs_path)
    for spec_file_path in glob.glob("*.yaml"):
        with open(spec_file_path, "r") as spec_file:
            try:
                spec_yaml = yaml.safe_load(spec_file)
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
            except yaml.YAMLError:
                pass
    os.chdir(cwd)
    return result


specs = collect_task_specs()


class DiffKempTaskSpec(TaskSpec):
    """
    Task specification for one parameter of one kernel module.
    """
    def __init__(self, spec):
        TaskSpec.__init__(self, spec, tasks_path, spec["module"], spec["path"])
        self.module = spec["module"]
        self.param = KernelParam(spec["param"])
        self.module_dir = spec["path"]
        self.module_src = spec["filename"]

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
            if desc == "skip":
                self.functions[fun] = Result.Kind.NONE
            else:
                try:
                    self.functions[fun] = Result.Kind[desc.upper()]
                except KeyError:
                    if desc == "only_old":
                        self.only_old.add(fun[0])
                    elif desc == "only_new":
                        self.only_new.add(fun[0])


def prepare_task(spec):
    """
    Prepare testing task (scenario). Build old and new modules and copy the
    source files.
    """
    # Build the modules
    if not os.path.isfile(spec.old_llvm_file()):
        spec.old_module = spec.old_builder.build_module(spec.module)
    else:
        spec.old_module = LlvmKernelModule(spec.module, spec.old_llvm_file(),
                                           "")
    if not os.path.isfile(spec.new_llvm_file()):
        spec.new_module = spec.new_builder.build_module(spec.module)
    else:
        spec.new_module = LlvmKernelModule(spec.module, spec.new_llvm_file(),
                                           "")

    # Copy the source files to the task directory (kernel_modules/module)
    spec.prepare_dir(old_module=spec.old_module,
                     old_src=os.path.join(spec.old_builder.kernel_path,
                                          spec.module_dir, spec.module_src),
                     new_module=spec.new_module,
                     new_src=os.path.join(spec.new_builder.kernel_path,
                                          spec.module_dir, spec.module_src))


@pytest.fixture(params=[x[1] for x in specs],
                ids=[x[0] for x in specs])
def task_spec(request):
    """pytest fixture to prepare tasks"""
    spec = DiffKempTaskSpec(request.param)
    prepare_task(spec)
    return spec


class TestClass(object):
    """
    Main testing class. One object of the class is created for each testing
    task. Contains 2 tests for function couplings collection and for the
    actual function analysis (semantic comparison).
    """
    def test_couplings(self, task_spec):
        """
        Test collection of function couplings. Checks whether the collected
        couplings of main functions (functions using the paramter of the
        analysis) match functions specified in the test spec.
        """
        couplings = FunctionCouplings(task_spec.old_module.llvm,
                                      task_spec.new_module.llvm)
        couplings.infer_for_param(task_spec.param)

        coupled = set([(c.first, c.second) for c in couplings.main])
        assert coupled == set(task_spec.functions.keys())
        assert couplings.uncoupled_first == task_spec.only_old
        assert couplings.uncoupled_second == task_spec.only_new

    def test_simpll(self, task_spec):
        """
        Test comparison of semantic difference of modules w.r.t. a parameter.
        For each compared function, the module is first simplified using the
        SimpLL tool and then the actual analysis is run. Compares the obtained
        result with the expected one.
        If timeout is expected, the analysis is not run to increase testing
        speed.
        """
        # Configuration (only set the timeout, the rest is not used).
        for fun_pair, expected in task_spec.functions.iteritems():
            if expected not in [Result.Kind.TIMEOUT, Result.Kind.NONE]:
                result = functions_diff(mod_first=task_spec.old_module,
                                        mod_second=task_spec.new_module,
                                        fun_first=fun_pair[0],
                                        fun_second=fun_pair[1],
                                        glob_var=task_spec.param,
                                        config=task_spec.config)
                assert result.kind == expected
