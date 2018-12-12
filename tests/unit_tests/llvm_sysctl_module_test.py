"""Unit tests for working with sysctl modules."""

import pytest
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.llvm_ir.llvm_sysctl_module import LlvmSysctlModule


@pytest.fixture(scope="module")
def module():
    # Builds sysctl source for net.core and uses it for creating a
    # LlvmSysctlModule object
    kernel_builder = LlvmKernelBuilder("3.10.0-862", "net/core")
    kernel_module = kernel_builder.build_file("sysctl_net_core")
    return LlvmSysctlModule(kernel_module, "net_core_table")


def test_get_sysctl(module):
    sysctl = module._get_sysctl("wmem_max")
    assert sysctl.is_a_constant_struct()


def test_get_proc_fun(module):
    assert module.get_proc_fun("wmem_max") == "proc_dointvec_minmax"


def test_get_data(module):
    assert module.get_data("wmem_max") == "sysctl_wmem_max"
