"""
Package for work with LLVM internal representation (IR).  Contains classes and
functions to build kernel module sources into LLVM IR and to perform some
simple analyses over the created LLVM modules and functions.
"""
import sys

# only this one needs to be python2 compliant
from .compiler import *

if sys.version_info.major >= 3:
    from .optimiser import *
    from .llvm_module import *
    from .llvm_source_finder import *
    from .llvm_sysctl_module import *
    from .source_tree import *
    from .kernel_llvm_source_builder import *
    from .kernel_source_tree import *
    from .single_llvm_finder import *
    from .single_c_builder import *
    from .wrapper_build_finder import *
