from diffkemp.llvm_ir.kernel_llvm_source_builder import KernelLlvmSourceBuilder
from diffkemp.llvm_ir.source_tree import SourceNotFoundException
from diffkemp.llvm_ir.kernel_source_tree import KernelSourceTree
from diffkemp.snapshot import Snapshot
from diffkemp.building.build_utils import (
    generate_from_function_list,
    read_symbol_list,
    EMSG_EMPTY_SYMBOL_LIST)
import errno
import os
import sys


def build_kernel(args):
    """
    Create snapshot of a Linux kernel source tree. Kernel sources are
    compiled into LLVM IR on-the-fly as necessary.
    Supports two kinds of symbol lists to generate the snapshot from:
      - list of functions (default)
      - list of sysctl options
    """
    # Create a new snapshot from the kernel source directory.
    snapshot = _generate_snapshot(args)

    # Read the symbol list
    symbol_list = read_symbol_list(args.symbol_list)
    if not symbol_list:
        sys.stderr.write(EMSG_EMPTY_SYMBOL_LIST)
        sys.exit(errno.EINVAL)

    # Generate snapshot contents
    if args.sysctl:
        generate_from_sysctl_list(snapshot, symbol_list)
    else:
        generate_from_function_list(snapshot, symbol_list)

    # Create the snapshot directory containing the YAML description file
    snapshot.generate_snapshot_dir()
    snapshot.finalize()


def _generate_snapshot(args):
    source_finder = KernelLlvmSourceBuilder(args.source_dir)
    if not _validate_kernel_config(source_finder):
        sys.exit(errno.EINVAL)

    source = KernelSourceTree(args.source_dir, source_finder)
    list_kind = "sysctl" if args.sysctl else "function"

    return Snapshot.create_from_source(source,
                                       args.output_dir,
                                       list_kind,
                                       not args.no_source_dir)


def _validate_kernel_config(source_finder):
    if source_finder.is_configured():
        return True

    sys.stderr.write(
        "Error: Kernel configuration is incomplete or invalid.\n"
        "Run `make olddefconfig` to set missing configurations.\n"
    )
    return False


def generate_from_sysctl_list(snapshot, sysctl_list):
    """
    Generate a snapshot from a list of sysctl options.
    For each sysctl option:
      - get LLVM IR of the file which defines the sysctl option
      - find and compile the proc handler function and add it to the snapshot
      - find the sysctl data variable
      - find, compile, and add to the snapshot all functions that use
        the data variable
    :param snapshot: Existing Snapshot object to fill
    :param sysctl_list: List of sysctl options.
                        May contain patterns such as "kernel.*".
    """
    for symbol in sysctl_list:
        # Get module with sysctl definitions
        try:
            sysctl_mod = snapshot.source_tree.get_sysctl_module(symbol)
        except SourceNotFoundException:
            print("{}: sysctl not supported".format(symbol))
            continue

        # Iterate all sysctls represented by the symbol (it can be a pattern)
        sysctl_list = sysctl_mod.parse_sysctls(symbol)
        if not sysctl_list:
            print("{}: no sysctl found".format(symbol))
            continue
        for sysctl in sysctl_list:
            print("{}:".format(sysctl))

            proc_fun = sysctl_mod.get_proc_fun(sysctl)
            # Proc handler function for sysctl
            _add_proc_handler(snapshot, sysctl, proc_fun)
            # Adds functions which use the sysctl data variable
            _add_funcs_for_data(snapshot, sysctl, sysctl_mod, proc_fun)


def _add_proc_handler(snapshot, sysctl, proc_fun):
    if not proc_fun:
        return

    try:
        proc_fun_mod = snapshot.source_tree.get_module_for_symbol(
            proc_fun)
        snapshot.add_fun(name=proc_fun,
                         llvm_mod=proc_fun_mod,
                         glob_var=None,
                         tag="proc handler function",
                         group=sysctl)
        print("  {}: {} (proc handler)".format(
            proc_fun,
            os.path.relpath(proc_fun_mod.llvm,
                            snapshot.source_tree.source_dir)))
    except SourceNotFoundException:
        print("  could not build proc handler")


def _add_funcs_for_data(snapshot, sysctl, sysctl_mod, proc_fun):
    data = sysctl_mod.get_data(sysctl)

    if not data:
        return

    for module in \
            snapshot.source_tree.get_modules_using_symbol(data.name):
        # For now, we only support the x86 architecture in kernel
        if "/arch/" in module.llvm and \
                "/arch/x86/" not in module.llvm:
            continue

        curr_mod_path = os.path.relpath(module.llvm,
                                        snapshot.source_tree.source_dir)
        for func in module.get_functions_using_param(data):
            if func == proc_fun:
                continue

            snapshot.add_fun(
                name=func,
                llvm_mod=module,
                glob_var=data.name,
                tag="using data variable \"{}\"".format(data.name),
                group=sysctl)
            print("  {}: {} (using data variable \"{}\")".format(
                func,
                curr_mod_path,
                data.name))
