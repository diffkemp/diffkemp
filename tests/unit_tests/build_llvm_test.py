"""
Unit tests for building kernel sources into LLVM IR.
Mostly tests for the LlvmKernelBuilder class located in llvm_ir/build_llvm.py.
"""

from diffkemp.llvm_ir.build_llvm import BuildException, LlvmKernelBuilder
import pytest
import os


versions = ("kernel/linux-3.10", "kernel/linux-3.10.0-957.el7")


@pytest.fixture
def builder(request):
    """
    Create kernel builder that is shared among tests.
    Parametrized by kernel directory.
    """
    b = LlvmKernelBuilder(request.param)
    yield b
    b.finalize()


@pytest.mark.parametrize("kernel_dir", versions)
def test_create_kernel(kernel_dir):
    """Creating kernel builder."""
    builder = LlvmKernelBuilder(kernel_dir)
    assert builder.kernel_dir == os.path.join(os.getcwd(), kernel_dir)
    assert builder.built_modules == dict()
    # Check that "asm goto" has been disabled
    with open(os.path.join(kernel_dir, "include/linux/compiler-gcc.h"),
              "r") as gcc_header:
        assert "asm goto(x)" not in gcc_header.read()


@pytest.mark.parametrize("builder", versions, indirect=True)
def test_kbuild_object_command(builder):
    """Finding which command is used to build an object file."""
    command = builder.kbuild_object_command("sound/core/sound.o")
    assert command.startswith("gcc")


@pytest.mark.parametrize("builder", versions, indirect=True)
def test_kbuild_module_commands(builder):
    file_name, commands = builder.kbuild_module_commands("drivers/firewire",
                                                         "firewire-sbp2")
    assert file_name == "firewire-sbp2"
    assert commands
    for c in commands:
        assert c.startswith("gcc") or c.startswith("ld")


@pytest.mark.parametrize("builder", versions, indirect=True)
def test_build_src_to_llvm(builder):
    """Building single object into LLVM."""
    builder.build_source_to_llvm("sound/core/init.c", "sound/core/init.ll")
    assert os.path.isfile(
        os.path.join(builder.kernel_dir, "sound/core/init.ll"))


@pytest.mark.parametrize("builder", versions, indirect=True)
def test_build_file_fail(builder):
    """Try to build a file that has no source."""
    with pytest.raises(BuildException):
        builder.build_source_to_llvm("sound/core/snd.c", "sound/core/snd.ll")


@pytest.mark.parametrize("builder", versions, indirect=True)
def test_build_mod_to_llvm(builder):
    mod_file = os.path.join(builder.kernel_dir,
                            "drivers/firewire/firewire-sbp2.ll")
    if os.path.isfile(mod_file):
        os.unlink(mod_file)
    builder.build_kernel_mod_to_llvm("drivers/firewire", "firewire-sbp2")
    assert os.path.isfile(mod_file)


@pytest.mark.parametrize("builder", versions, indirect=True)
def test_build_mod_fail(builder):
    with pytest.raises(BuildException):
        builder.build_kernel_mod_to_llvm("drivers/firewire", "firewire")


def test_finalize():
    """Testing destructor of LlvmKernelBuilder."""
    builder = LlvmKernelBuilder("kernel/linux-3.10.0-957.el7")
    gcc_header_path = os.path.join(builder.kernel_dir,
                                   "include/linux/compiler-gcc.h")
    builder.finalize()
    # Check that "asm goto" has been re-enabled.
    with open(gcc_header_path, "r") as gcc_header:
        assert "asm goto(x)" in gcc_header.read()
