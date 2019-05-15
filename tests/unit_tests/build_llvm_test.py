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
    return LlvmKernelBuilder("3.10", "sound/core", rebuild=True)


@pytest.fixture(scope="module")
def builder_rhel():
    """Create module builder for the RHEL kernel."""
    return LlvmKernelBuilder("3.10.0-957.el7", "sound/core", rebuild=True)


def test_prepare_kernel(builder):
    """Kernel downloading and preparation."""
    builder._prepare_kernel()
    assert os.path.isdir("kernel/linux-3.10")


def test_prepare_rhel_kernel(builder_rhel):
    """Download and preparation of a RHEL kernel."""
    builder_rhel._prepare_kernel()
    assert os.path.isdir("kernel/linux-3.10.0-957.el7")


def test_extract_kabi_whitelist(builder_rhel):
    builder_rhel._prepare_kernel()
    assert os.path.exists("kernel/linux-3.10.0-957.el7/kabi_whitelist_x86_64")


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
    assert mod.llvm == "/diffkemp/kernel/linux-3.10/sound/core/init.ll"
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
    mod = builder.build_module("oss/snd_pcm_oss")
    assert mod.name == "snd_pcm_oss"

    module_path = "/diffkemp/kernel/linux-3.10/sound/core/oss/"
    # Check if kernel object file exists and is correct in the module
    kernel_obj = os.path.join(module_path, "snd-pcm-oss.ko")
    assert os.path.isfile(kernel_obj)
    assert mod.kernel_object == kernel_obj

    # Check if LLVM IR file exists and is correct in the module
    llvm_file = os.path.join(module_path, "snd-pcm-oss.ll")
    assert os.path.isfile(llvm_file)
    assert mod.llvm == llvm_file

    # Check if all sources of the module are built into .o and .ll files
    file_names = ["mixer_oss", "pcm_oss", "pcm_plugin", "io", "copy",
                  "linear", "mulaw", "route", "rate"]
    for f in file_names:
        assert os.path.isfile(os.path.join(module_path, "{}.o".format(f)))
        assert os.path.isfile(os.path.join(module_path, "{}.ll".format(f)))


def test_build_modules_with_params():
    """
    Building all modules that contain parameters.
    Checks whether all necessary modules are found and built.
    """
    builder = LlvmKernelBuilder("3.10", "sound/core/seq", rebuild=True)
    modules = builder.build_modules_with_params()
    assert sorted(modules.keys()) == sorted(["snd_seq_midi", "snd_seq_dummy",
                                             "snd_seq", "snd_seq_oss"])

    for n, m in modules.iteritems():
        assert os.path.isfile(m.llvm)


def test_build_all_modules():
    """
    Building all modules in a folder.
    Checks whether all modules are built.
    """
    builder = LlvmKernelBuilder("3.10", "sound/core/oss")
    modules = builder.build_all_modules()
    assert sorted(modules.keys()) == sorted(["snd-mixer-oss", "snd-pcm-oss"])

    for n, m in modules.iteritems():
        assert os.path.isfile(m.llvm)


def test_build_sysctl_module():
    """
    Building source containing definitions of a sysctl option.
    """
    builder = LlvmKernelBuilder("3.10.0-862.el7", None, rebuild=True)
    for mod in [{
        "name": "net.core.message_burst",
        "file": "net/core/sysctl_net_core",
        "table": "net_core_table"
    }, {
        "name": "kernel.usermodehelper.bset",
        "file": "kernel/kmod",
        "table": "usermodehelper_table"
    }]:
        module = builder.build_sysctl_module(mod["name"])
        assert module is not None
        assert module.mod is not None
        assert module.mod.name == mod["file"]
        assert module.ctl_table == mod["table"]
