"""
Unit tests for testing function syntax diff.
"""

from diffkemp.config import Config
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.semdiff.module_diff import functions_diff
import pytest


@pytest.fixture(scope="module")
def builder_left():
    """Create module builder that is shared among tests"""
    return LlvmKernelBuilder("3.10.0-862.el7", None, debug=True, rebuild=True)


@pytest.fixture(scope="module")
def builder_right():
    """Create module builder that is shared among tests"""
    return LlvmKernelBuilder("3.10.0-957.el7", None, debug=True, rebuild=True)


def test_syntax_diff(builder_left, builder_right):
    f = "dio_iodone2_helper"
    diff = ('*************** static void dio_iodone2_helper(struct dio *dio, l'
            'off_t offset,\n*** 246,250 ***\n  {\n! \tif (dio->end_io && dio->'
            'result)\n! \t\tdio->end_io(dio->iocb, offset,\n! \t\t\t\ttransfer'
            'red, dio->private, ret, is_async);\n  \n--- 246,249 ---\n  {\n! '
            '\tif (dio->end_io)\n! \t\tdio->end_io(dio->iocb, offset, ret, dio'
            '->private, 0, 0);\n  \n')

    config = Config(builder_left, builder_right, timeout=15, print_diff=True,
                    control_flow_only=True, verbosity=False, do_not_link=False,
                    semdiff_tool=None)
    first = builder_left.build_file_for_symbol(f)
    second = builder_right.build_file_for_symbol(f)
    fun_result = functions_diff(mod_first=first, mod_second=second,
                                fun_first=f, fun_second=f, glob_var=None,
                                config=config)
    assert fun_result.inner[f].diff == diff
