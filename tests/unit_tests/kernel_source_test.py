"""Unit tests for browsing kernel sources, finding function definitions, ..."""

from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
import pytest
import os


# LLVM kernel builder (mainly because of source download, otherwise
# it would be sufficient to use KernelSource only).
@pytest.fixture(scope="module")
def builder():
    return LlvmKernelBuilder("3.10", None)


def test_get_sources_with_params(builder):
    mod_dir = os.path.join(builder.kernel_path, "sound/core")
    srcs = builder.source.get_sources_with_params(mod_dir)
    srcs = [os.path.relpath(s, mod_dir) for s in srcs]
    assert sorted(srcs) == sorted([
        "rtctimer.c",
        "pcm_memory.c",
        "seq/seq.c",
        "seq/seq_dummy.c",
        "seq/seq_midi.c",
        "seq/oss/seq_oss_init.c",
        "seq/oss/seq_oss.c",
        "misc.c",
        "timer.c",
        "sound.c",
        "init.c",
        "oss/pcm_oss.c",
        "rawmidi.c"
    ])


def test_build_cscope_database(builder):
    builder.source.build_cscope_database()
    for file in ["cscope.files", "cscope.in.out", "cscope.out",
                 "cscope.po.out"]:
        assert os.path.isfile(os.path.join("kernel/linux-3.10", file))


def test_find_srcs_using_symbol(builder):
    srcs = builder.source.find_srcs_using_symbol("net_ratelimit_state")
    assert srcs == set(['net/core/utils.c'])


def test_find_srcs_with_symbol_def(builder):
    srcs = builder.source.find_srcs_with_symbol_def("ipmi_set_gets_events")
    assert srcs == [
        "drivers/char/ipmi/ipmi_msghandler.c",
        "drivers/char/ipmi/ipmi_devintf.c"
    ]
