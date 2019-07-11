"""
Unit tests for KernelSource class.
Tests for finding sources and building them into LLVM modules.
"""

from diffkemp.llvm_ir.kernel_source import KernelSource, \
    SourceNotFoundException
import os
import pytest
import shutil
import tempfile


@pytest.fixture
def source():
    s = KernelSource("kernel/linux-3.10.0-957.el7", True)
    yield s
    s.finalize()


def test_get_sources_with_params(source):
    """Test finding sources that define module parameters."""
    mod_dir = os.path.join(source.kernel_dir, "sound/core")
    srcs = source.get_sources_with_params(mod_dir)
    srcs = [os.path.relpath(s, mod_dir) for s in srcs]
    assert sorted(srcs) == sorted([
        "pcm_memory.c",
        "seq/seq.c",
        "seq/seq_dummy.c",
        "seq/seq_midi.c",
        "seq/oss/seq_oss_init.c",
        "misc.c",
        "timer.c",
        "sound.c",
        "init.c",
        "oss/pcm_oss.c",
        "rawmidi.c"
    ])


def test_build_cscope_database(source):
    """Test building CScope database."""
    source.build_cscope_database()
    for file in ["cscope.files", "cscope.in.out", "cscope.out",
                 "cscope.po.out"]:
        assert os.path.isfile(os.path.join(source.kernel_dir, file))


def test_find_srcs_using_symbol(source):
    """Test finding sources using a global variable."""
    srcs = source.find_srcs_using_symbol("net_ratelimit_state")
    assert srcs == {"net/core/utils.c"}


def test_find_srcs_with_symbol_def(source):
    """Test finding sources with function definition."""
    srcs = source.find_srcs_with_symbol_def("ipmi_set_gets_events")
    assert sorted(srcs) == sorted([
        "drivers/char/ipmi/ipmi_msghandler.c",
        "drivers/char/ipmi/ipmi_devintf.c"
    ])


def test_find_srcs_with_symbol_def_fail(source):
    """Test finding sources with definition of a non-existing function."""
    with pytest.raises(SourceNotFoundException):
        source.find_srcs_with_symbol_def("nonexisting_function")


def test_get_module_from_source(source):
    """Test building LLVM module from a C source file."""
    llvm_file = os.path.join(source.kernel_dir, "net/core/utils.ll")
    if os.path.isfile(llvm_file):
        os.unlink(llvm_file)
    mod = source.get_module_from_source("net/core/utils.c")
    assert mod is not None
    assert "net/core/utils" in source.modules
    assert mod.llvm == llvm_file
    assert mod.source == os.path.join(source.kernel_dir, "net/core/utils.c")


def test_get_module_for_symbol(source):
    """
    Test building LLVM module from a source containing a function definition.
    """
    mod = source.get_module_for_symbol("__alloc_workqueue_key")
    assert mod is not None
    assert mod.llvm == os.path.join(source.kernel_dir, "kernel/workqueue.ll")
    assert mod.has_function("__alloc_workqueue_key")


def test_get_module_for_symbol_fail(source):
    """Test building LLVM module for a non-existing function definition."""
    with pytest.raises(SourceNotFoundException):
        source.get_module_for_symbol("__get_user_2")


@pytest.mark.parametrize("name, llvm_file, table", [
    ("net.core.message_burst",
     "net/core/sysctl_net_core.ll",
     "net_core_table"),
    ("kernel.usermodehelper.bset",
     "kernel/kmod.ll",
     "usermodehelper_table")
])
def test_get_sysctl_module(source, name, llvm_file, table):
    """
    Test building source files containing definitions of a sysctl option.
    """
    m = source.get_sysctl_module(name)
    assert m is not None
    assert m.mod is not None
    assert m.mod.llvm == os.path.join(source.kernel_dir, llvm_file)
    assert m.ctl_table == table


def test_get_module_for_kernel_mod(source):
    m = source.get_module_for_kernel_mod("drivers/firewire", "firewire-sbp2")
    assert m is not None
    assert m.llvm == os.path.join(source.kernel_dir,
                                  "drivers/firewire/firewire-sbp2.ll")


def test_copy_source_files(source):
    """Test copying source files into other root kernel directory."""
    m1 = source.get_module_from_source("net/core/utils.c")
    m2 = source.get_module_from_source("kernel/workqueue.c")
    tmp = tempfile.mkdtemp()
    source.copy_source_files([m1, m2], tmp)

    # Check that necessary directories were created.
    for d in ["net", "net/core", "kernel", "include", "include/linux"]:
        assert os.path.isdir(os.path.join(tmp, d))

    # Check that files were successfully copied.
    for f in ["net/core/utils.c", "net/core/utils.ll", "kernel/workqueue.c",
              "kernel/workqueue.ll", "include/linux/module.h",
              "include/linux/kernel.h"]:
        assert os.path.isfile(os.path.join(tmp, f))

    shutil.rmtree(tmp)


def test_copy_cscope_files(source):
    """Test copying CScope database into different directory."""
    source.build_cscope_database()
    tmp = tempfile.mkdtemp()
    source.copy_cscope_files(tmp)

    for f in ["cscope.files", "cscope.in.out", "cscope.out", "cscope.po.out"]:
        assert os.path.isfile(os.path.join(tmp, f))

    shutil.rmtree(tmp)
