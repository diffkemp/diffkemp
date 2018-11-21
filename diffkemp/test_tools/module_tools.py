"""Tools for working with modules used by both diffkemp and diffkabi tests."""

from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
import shutil
import os


def _build_module(kernel_version, module_dir, module, debug):
    """
    Build LLVM IR of the analysed module.
    """
    builder = LlvmKernelBuilder(kernel_version, module_dir, debug)
    llvm_mod = builder.build_module(module, True)
    return llvm_mod


def prepare_module(module_dir, module, module_src, kernel_version, llvm,
                   llvm_simpl, src, debug=False, build_module=True):
    if not os.path.isfile(llvm):
        if build_module:
            mod = _build_module(kernel_version, module_dir, module,
                                debug)
            original_llvm = mod.llvm
        else:
            original_llvm = os.path.join(module_dir, module + ".ll")

        shutil.copyfile(original_llvm, llvm)
        mod_src = os.path.join(os.path.dirname(original_llvm), module_src)
        shutil.copyfile(mod_src, src)
