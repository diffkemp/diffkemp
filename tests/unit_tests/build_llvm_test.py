"""
Unit tests for building kernel modules into LLVM IR.
Mostly tests for the LlvmKernelBuilder class located in llvm_ir/build_llvm.py.
"""

from diffkemp.llvm_ir.build_llvm import BuildException, LlvmKernelBuilder
import pytest
import os


@pytest.fixture(scope="module")
def builder():
    """Create module builder that is shared among tests"""
    return LlvmKernelBuilder("3.10", "sound/core")


def test_prepare_kernel(builder):
    """Kernel downloading and preparation."""
    builder._prepare_kernel()
    assert os.path.isdir("kernel/linux-3.10")


def test_get_sources_with_params(builder):
    """
    Searching for source C files that contain definitions of module
    parameters.
    """
    mod_dir = os.path.join(builder.kernel_path, builder.modules_dir)
    srcs = builder._get_sources_with_params(mod_dir)
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


def test_kbuild_object_command(builder):
    """Finding which command is used to build an object file."""
    command = builder.kbuild_object_command("sound.o")
    assert command.startswith("gcc")


def test_get_module_name(builder):
    """Deducing name of the module that the object is build within."""
    command = builder.kbuild_object_command("sound.o")
    assert builder.get_module_name(command) == "snd"


def test_get_output_file(builder):
    """Deducing name of the output file from a GCC command."""
    command = builder.kbuild_object_command("sound.o")
    assert builder.get_output_file(command.split()) == \
        "sound/core/.tmp_sound.o"


@pytest.mark.parametrize("input_name, module_name", [
    ("snd", "snd"),
    ("snd_timer", "snd-timer")
])
def test_kbuild_module(builder, input_name, module_name):
    """
    Building a kernel module using Kbuild.
    More test scenarios can be added using pytest parametrization.
    """
    file_name, commands = builder.kbuild_module(input_name)
    assert file_name == module_name
    module_file = os.path.join(builder.kernel_path, builder.modules_dir,
                               "{}.ko".format(module_name))
    assert os.path.isfile(module_file)


def test_kbuild_module_fail(builder):
    """Try to build an invalid module - expect an exception"""
    with pytest.raises(BuildException):
        builder.kbuild_module("rtctimer")


def test_build_file(builder):
    """Building single object into LLVM."""
    mod = builder.build_file("init")
    assert mod.llvm == "/diffkemp/kernel/linux-3.10/sound/core/init.bc"
    assert os.path.isfile(mod.llvm)
    assert mod.kernel_object is None


def test_build_file_fail(builder):
    """Try to build a file that has no source."""
    with pytest.raises(BuildException):
        builder.build_file("snd")


def test_build_module(builder):
    """
    Building module into LLVM IR code.
    Checks whether all necessary files are identified and built.
    """
    mod = builder.build_module("oss/snd_pcm_oss", True)
    assert mod.name == "oss/snd_pcm_oss"

    module_path = "/diffkemp/kernel/linux-3.10/sound/core/oss/"
    # Check if kernel object file exists and is correct in the module
    kernel_obj = os.path.join(module_path, "snd-pcm-oss.ko")
    assert os.path.isfile(kernel_obj)
    assert mod.kernel_object == kernel_obj

    # Check if LLVM IR file exists and is correct in the module
    llvm_file = os.path.join(module_path, "snd-pcm-oss.bc")
    assert os.path.isfile(llvm_file)
    assert mod.llvm == llvm_file

    # Check if all sources of the module are built into .o and .bc files
    file_names = ["mixer_oss", "pcm_oss", "pcm_plugin", "io", "copy",
                  "linear", "mulaw", "route", "rate"]
    for f in file_names:
        assert os.path.isfile(os.path.join(module_path, "{}.o".format(f)))
        assert os.path.isfile(os.path.join(module_path, "{}.bc".format(f)))


def test_build_modules_with_params():
    """
    Building all modules that contain parameters.
    Checks whether all necessary modules are found and built.
    """
    builder = LlvmKernelBuilder("3.10", "sound/core/seq")
    modules = builder.build_modules_with_params(True)
    assert modules.keys() == ["snd_seq_midi", "snd_seq_dummy", "snd_seq",
                              "oss/snd_seq_oss"]

    for n, m in modules.iteritems():
        assert os.path.isfile(m.llvm)
