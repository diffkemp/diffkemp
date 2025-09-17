from diffkemp.building.build_c_project \
    import build_c_project, build_c_file
from diffkemp.building.build_utils import (
    generate_from_function_list,
    read_symbol_list,
    EMSG_EMPTY_SYMBOL_LIST)
from diffkemp.snapshot import Snapshot
from diffkemp.llvm_ir.source_tree import SourceTree
from diffkemp.llvm_ir.single_llvm_finder import SingleLlvmFinder
import errno
import os
import sys


def build(args):
    # build snapshot from make-based c project
    if os.path.isdir(args.source_dir):
        build_c_project(args)
    # make snapshot from single c file
    elif os.path.isfile(args.source_dir) and args.source_dir.endswith(".c"):
        build_c_file(args)
    else:
        sys.stderr.write(
            "Error: the specified source_dir is not a directory nor a C file\n"
        )
        sys.exit(errno.EINVAL)


def llvm_to_snapshot(args):
    """
    Create snapshot from a project pre-compiled into a single LLVM IR file.
    :param args: CLI arguments of the "diffkemp llvm-to-snapshot" command.
    """
    source_finder = SingleLlvmFinder(args.source_dir, args.llvm_file)
    source = SourceTree(args.source_dir, source_finder)
    snapshot = Snapshot.create_from_source(source, args.output_dir, "function")

    function_list = read_symbol_list(args.function_list)
    if not function_list:
        sys.stderr.write(EMSG_EMPTY_SYMBOL_LIST)
        sys.exit(errno.EINVAL)

    generate_from_function_list(snapshot, function_list)
    snapshot.generate_snapshot_dir()
    snapshot.finalize()
