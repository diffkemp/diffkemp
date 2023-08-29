"""
Unit tests for testing function syntax diff.
"""

from diffkemp.config import Config, BuiltinPatterns
from diffkemp.llvm_ir.source_tree import SourceTree
from diffkemp.llvm_ir.kernel_llvm_source_builder import KernelLlvmSourceBuilder
from diffkemp.semdiff.module_diff import functions_diff


def test_syntax_diff():
    f = "dio_iodone2_helper"
    diff = ('*************** static void dio_iodone2_helper(struct dio *dio, l'
            'off_t offset,\n*** 246,250 ***\n  {\n! \tif (dio->end_io && dio->'
            'result)\n! \t\tdio->end_io(dio->iocb, offset,\n! \t\t\t\ttransfer'
            'red, dio->private, ret, is_async);\n  \n--- 246,249 ---\n  {\n! '
            '\tif (dio->end_io)\n! \t\tdio->end_io(dio->iocb, offset, ret, dio'
            '->private, 0, 0);\n  \n')

    kernel_old = "kernel/linux-3.10.0-862.el7"
    kernel_new = "kernel/linux-3.10.0-957.el7"
    source_first = SourceTree(kernel_old, KernelLlvmSourceBuilder(kernel_old))
    source_second = SourceTree(kernel_new, KernelLlvmSourceBuilder(kernel_new))
    config = Config(builtin_patterns=BuiltinPatterns(control_flow_only=True))
    first = source_first.get_module_for_symbol(f)
    second = source_second.get_module_for_symbol(f)
    fun_result = functions_diff(mod_first=first, mod_second=second,
                                fun_first=f, fun_second=f, glob_var=None,
                                config=config)
    assert fun_result.inner[f].diff == diff
