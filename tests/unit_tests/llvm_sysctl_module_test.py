"""Unit tests for working with sysctl modules."""

from diffkemp.utils import get_llvm_version
import pytest
from diffkemp.llvm_ir.kernel_source_tree import KernelSourceTree
from diffkemp.llvm_ir.kernel_llvm_source_builder import KernelLlvmSourceBuilder
from diffkemp.llvm_ir.llvm_sysctl_module import matches, LlvmSysctlModule


@pytest.fixture
def mod():
    """
    Build LlvmSysctlModule for net.core.* sysctl options shared among tests.
    """
    kernel_dir = "kernel/linux-3.10.0-957.el7"
    source = KernelSourceTree(kernel_dir, KernelLlvmSourceBuilder(kernel_dir))
    kernel_module = source.get_module_for_symbol("net_core_table")
    yield LlvmSysctlModule(kernel_module, "net_core_table")
    source.finalize()


def test_get_proc_fun(mod):
    """Test getting proc function for a sysctl."""
    assert mod.get_proc_fun("wmem_max") == "proc_dointvec_minmax"


def test_get_data(mod):
    """Test getting data variables for different sysctl options."""
    # Data variable with bitcast
    assert mod.get_data("netdev_budget").name == "netdev_budget"
    # Data variable with GEP and bitcast
    data = mod.get_data("message_burst")
    assert data.name == "net_ratelimit_state"
    assert data.indices == [8]
    # Data variable with GEP (without bitcast)
    # Note: The GEP is missing (redundant) when opaque pointers are enabled
    data = mod.get_data("netdev_rss_key")
    assert data.name == "netdev_rss_key"
    if get_llvm_version() < 15:
        assert data.indices == [0, 0]


def test_get_child():
    """Test getting child of a sysctl definition."""
    kernel_dir = "kernel/linux-3.10.0-957.el7"
    source = KernelSourceTree(kernel_dir, KernelLlvmSourceBuilder(kernel_dir))
    kernel_module = source.get_module_for_symbol("sysctl_base_table")
    sysctl_module = LlvmSysctlModule(kernel_module, "sysctl_base_table")
    assert sysctl_module.get_child("vm").name == "vm_table"


def test_pattern_matches():
    """Test sysctl pattern matching."""
    assert matches("core_pattern", "*")
    assert matches("core_pattern", "{core_pattern|core_uses_pid}")
    assert not matches("core_pattern", "{acct|hotplug}")
