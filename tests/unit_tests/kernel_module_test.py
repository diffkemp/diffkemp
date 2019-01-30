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
