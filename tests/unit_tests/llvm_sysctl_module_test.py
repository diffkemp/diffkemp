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
    # Data variable with bitcast
    assert module.get_data("netdev_budget").name == "netdev_budget"
    # Data variable with GEP and bitcast
    data = module.get_data("message_burst")
    assert data.name == "net_ratelimit_state"
    assert data.indices == [8L]
    # Data variable with GEP (without bitcast)
    data = module.get_data("netdev_rss_key")
    assert data.name == "netdev_rss_key"
    assert data.indices == [0L, 0L]


def test_get_child():
    builder = LlvmKernelBuilder("3.10.0-862", "kernel")
    kernel_module = builder.build_file("sysctl.c")
    sysctl_module = LlvmSysctlModule(kernel_module, "sysctl_base_table")
    assert sysctl_module.get_child("vm").name == "vm_table"
