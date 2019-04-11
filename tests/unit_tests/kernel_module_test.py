"""
Unit tests for a kernel module in LLVM IR.
Mostly contains tests for searching module parameters.
"""

from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.llvm_ir.kernel_module import KernelModuleException
import pytest


@pytest.fixture(scope="module")
def builder():
    """Create module builder that is shared among multiple tests."""
    return LlvmKernelBuilder("3.10", "sound/core")


@pytest.mark.parametrize("mod, params", [
    ("snd", ["debug", "slots", "cards_limit", "major"]),
    ("oss/snd-pcm-oss", ["dsp_map", "adsp_map", "nonblock_open"])
])
def test_collect_all_parameters(builder, mod, params):
    """
    Collecting parameters in a module.
    More test scenarios can be added using pytest parametrization.
    """
    module = builder.build_module(mod)
    module.parse_module()
    module.collect_all_parameters()
    assert sorted(module.params.keys()) == sorted(params)


def test_find_param_var():
    """
    Searching for a name of variable that corresponds to a parameter.
    This is necessary since for parameters defined with module_param_named,
    names of the parameter and of the variable differ.
    """
    builder = LlvmKernelBuilder("3.10", "net/rfkill", rebuild=False)
    module = builder.build_module("rfkill")
    module.parse_module()
    assert module.find_param_var("default_state") == "rfkill_default_state"
    assert module.find_param_var("master_switch_mode") == \
        "rfkill_master_switch_mode"


def test_set_param(builder):
    """Setting a single parameter"""
    module = builder.build_module("snd")
    module.parse_module()
    module.set_param("cards_limit")
    assert module.params.keys() == ["cards_limit"]


def test_set_param_fail(builder):
    """Trying to set an invalid parameter should raise an exception"""
    module = builder.build_module("snd")
    module.parse_module()
    with pytest.raises(KernelModuleException):
        module.set_param("card_limit")


def test_get_filename(builder):
    """Getting file name."""
    module = builder.build_file("sound.c")
    module.parse_module()
    assert module.get_filename() == "sound/core/sound.c"


def test_links_mod(builder):
    """
    Testing if a module links another module. Then restoring the module to
    the unlinked state.
    """
    # Need to rebuild the file with debug info.
    builder.rebuild = True
    builder.debug = True
    sound = builder.build_file("sound.c")
    sound.parse_module()
    init = builder.build_file("init.c")
    init.parse_module()
    sound.link_modules([init])
    assert sound.links_mod(init)
    sound.restore_unlinked_llvm()
    assert not sound.links_mod(init)
