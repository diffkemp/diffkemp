from argparse import ArgumentParser, SUPPRESS
from diffkemp.config import Config
from diffkemp.snapshot import Snapshot
from diffkemp.llvm_ir.kernel_llvm_source_builder import KernelLlvmSourceBuilder
from diffkemp.llvm_ir.source_tree import SourceNotFoundException
from diffkemp.llvm_ir.llvm_module import LlvmParam, LlvmModule
from diffkemp.llvm_ir.single_llvm_finder import SingleLlvmFinder
from diffkemp.semdiff.caching import SimpLLCache
from diffkemp.semdiff.pattern_config import PatternConfig
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from tempfile import mkdtemp
from timeit import default_timer
import errno
import os
import re
import sys
import subprocess


def __make_argument_parser():
    """Parsing CLI arguments."""
    ap = ArgumentParser(description="Checking equivalence of semantics of "
                                    "functions in large C projects.")
    ap.add_argument("-v", "--verbose",
                    help="increase output verbosity",
                    action="count", default=0)
    sub_ap = ap.add_subparsers(dest="command", metavar="command")
    sub_ap.required = True

    # "generate" sub-command
    generate_ap = sub_ap.add_parser("generate",
                                    help="generate snapshot of compared "
                                         "functions")
    generate_ap.add_argument("source_dir",
                             help="project's root directory")
    generate_ap.add_argument("output_dir",
                             help="output directory of the snapshot")
    generate_ap.add_argument("functions_list",
                             help="list of functions to compare")

    source_kind = generate_ap.add_mutually_exclusive_group(required=True)
    source_kind.add_argument(
        "--kernel-with-builder", action="store_true",
        help="source is the Linux kernel not pre-built into LLVM IR")
    source_kind.add_argument(
        "--single-llvm-file", metavar="FILE",
        help="source project is built into a single LLVM IR file")

    generate_ap.add_argument("--sysctl", action="store_true",
                             help="function list is a list of function "
                                  "parameters")
    generate_ap.set_defaults(func=generate)

    # "compare" sub-command
    compare_ap = sub_ap.add_parser("compare",
                                   help="compare generated snapshots for "
                                        "semantic equality")
    compare_ap.add_argument("snapshot_dir_old",
                            help="directory with the old snapshot")
    compare_ap.add_argument("snapshot_dir_new",
                            help="directory with the new snapshot")
    compare_ap.add_argument("--show-diff",
                            help="show diff for non-equal functions",
                            action="store_true")
    compare_ap.add_argument("--regex-filter",
                            help="filter function diffs by given regex")
    compare_ap.add_argument("--output-dir", "-o",
                            help="name of the output directory")
    compare_ap.add_argument("--stdout", help="print results to stdout",
                            action="store_true")
    compare_ap.add_argument("--report-stat",
                            help="report statistics of the analysis",
                            action="store_true")
    compare_ap.add_argument("--source-dirs",
                            nargs=2,
                            help="specify root dirs for the compared projects")
    compare_ap.add_argument("--function", "-f",
                            help="compare only selected function")
    compare_ap.add_argument("--patterns", "-p",
                            help="difference pattern file or configuration")
    compare_ap.add_argument("--output-llvm-ir",
                            help="output each simplified module to a file",
                            action="store_true")
    compare_ap.add_argument("--control-flow-only",
                            help=SUPPRESS,
                            action="store_true")
    compare_ap.add_argument("--print-asm-diffs",
                            help="print raw inline assembly differences (does \
                            not apply to macros)",
                            action="store_true")
    compare_ap.add_argument("--semdiff-tool",
                            help=SUPPRESS,
                            choices=["llreve"])
    compare_ap.add_argument("--show-errors",
                            help="show functions that are either unknown or \
                            ended with an error in statistics",
                            action="store_true")
    compare_ap.add_argument("--enable-simpll-ffi",
                            help="calls SimpLL through FFI",
                            action="store_true")
    compare_ap.add_argument("--enable-module-cache",
                            help="loads frequently used modules to memory and \
                            uses them in SimpLL (requires SimpLL FFI to be \
                            enabled)",
                            action="store_true")
    compare_ap.set_defaults(func=compare)
    return ap


def run_from_cli():
    """Main method to run the tool."""
    ap = __make_argument_parser()
    args = ap.parse_args()
    args.func(args)


def generate(args):
    """
    Generate snapshot of sources of the compared functions.
    This involves:
      - get LLVM files with function definitions
      - copy LLVM and C source files into the snapshot directory
      - create YAML with a list mapping functions to their LLVM sources
    """
    # Choose the right LlvmSourceFinder and set the corresponding path to
    # the file/folder that the finder needs.
    if args.kernel_with_builder:
        # Linux kernel to LLVM builder
        source_finder_cls = KernelLlvmSourceBuilder
        source_finder_path = None
    elif args.single_llvm_file:
        # Project pre-built into a single LLVM IR file
        source_finder_cls = SingleLlvmFinder
        source_finder_path = args.single_llvm_file

    # Create a new snapshot from the source directory.
    snapshot = Snapshot.create_from_source(
        args.source_dir, args.output_dir,
        source_finder_cls, source_finder_path,
        "sysctl" if args.sysctl else None)
    source = snapshot.source_tree

    # Build sources for symbols from the list into LLVM IR
    with open(args.functions_list, "r") as fun_list_file:
        for line in fun_list_file.readlines():
            symbol = line.strip()
            if not symbol or not (symbol[0].isalpha() or symbol[0] == "_"):
                continue
            if args.sysctl:
                # For a sysctl parameter, we have to:
                #  - get LLVM of a file which defines the sysctl option
                #  - find and compile proc handler function and add it to the
                #    snapshot
                #  - find sysctl data variable
                #  - find, compile, and add to snapshot all functions that
                #    use the data variable

                # Get module with sysctl definitions
                try:
                    sysctl_mod = source.get_sysctl_module(symbol)
                except SourceNotFoundException:
                    print("{}: sysctl not supported".format(symbol))

                # Iterate all sysctls represented by the symbol (it can be
                # a pattern).
                sysctl_list = sysctl_mod.parse_sysctls(symbol)
                if not sysctl_list:
                    print("{}: no sysctl found".format(symbol))
                for sysctl in sysctl_list:
                    print("{}:".format(sysctl))
                    # New group in function list for the sysctl
                    snapshot.add_fun_group(sysctl)

                    # Proc handler function for sysctl
                    proc_fun = sysctl_mod.get_proc_fun(sysctl)
                    if proc_fun:
                        try:
                            proc_fun_mod = source.get_module_for_symbol(
                                proc_fun)
                            snapshot.add_fun(name=proc_fun,
                                             llvm_mod=proc_fun_mod,
                                             glob_var=None,
                                             tag="proc handler function",
                                             group=sysctl)
                            print("  {}: {} (proc handler)".format(
                                proc_fun,
                                os.path.relpath(proc_fun_mod.llvm,
                                                args.source_dir)))
                        except SourceNotFoundException:
                            print("  could not build proc handler")

                    # Functions using the sysctl data variable
                    data = sysctl_mod.get_data(sysctl)
                    if not data:
                        continue
                    for data_mod in source.get_modules_using_symbol(data.name):
                        for data_fun in \
                                data_mod.get_functions_using_param(data):
                            if data_fun == proc_fun:
                                continue
                            snapshot.add_fun(
                                name=data_fun,
                                llvm_mod=data_mod,
                                glob_var=data.name,
                                tag="function using sysctl data "
                                    "variable \"{}\"".format(data.name),
                                group=sysctl)
                            print(
                                "  {}: {} (using data variable \"{}\")".format(
                                    data_fun,
                                    os.path.relpath(data_mod.llvm,
                                                    args.source_dir),
                                    data.name))
            else:
                try:
                    # For a normal function, we compile its source and include
                    # it into the snapshot
                    sys.stdout.write("{}: ".format(symbol))
                    llvm_mod = source.get_module_for_symbol(symbol)
                    if not llvm_mod.has_function(symbol):
                        print("unsupported")
                        continue
                    print(os.path.relpath(llvm_mod.llvm, args.source_dir))
                    snapshot.add_fun(symbol, llvm_mod)
                except SourceNotFoundException:
                    print("source not found")
                    snapshot.add_fun(symbol, None)

    snapshot.generate_snapshot_dir()
    snapshot.finalize()


def _get_modules_to_cache(functions, group_name, other_snapshot,
                          min_frequency):
    """
    Generates a list of frequently used modules. These will be loaded into
    cache if DiffKemp is running with module caching enable.
    :param functions: List of pairs of functions to be compared along
    with their description objects
    :param group_name: Name of the group the functions are in
    :param other_snapshot: Snapshot object for looking up the functions
    in the other snapshot
    :param min_frequency: Minimal frequency for a module to be included into
    the cache
    :return: Set of modules that should be loaded into module cache
    """
    module_frequency_map = dict()
    for fun, old_fun_desc in functions:
        # Check if the function exists in the other snapshot
        new_fun_desc = other_snapshot.get_by_name(fun, group_name)
        if not new_fun_desc:
            continue
        for fun_desc in [old_fun_desc, new_fun_desc]:
            if not fun_desc.mod:
                continue
            if fun_desc.mod.llvm not in module_frequency_map:
                module_frequency_map[fun_desc.mod.llvm] = 0
            module_frequency_map[fun_desc.mod.llvm] += 1
    return {mod for mod, frequency in module_frequency_map.items()
            if frequency >= min_frequency}


def compare(args):
    """
    Compare the generated snapshots. Runs the semantic comparison and shows
    information about the compared functions that are semantically different.
    """
    # Parse both the new and the old snapshot.
    old_snapshot = Snapshot.load_from_dir(args.snapshot_dir_old)
    new_snapshot = Snapshot.load_from_dir(args.snapshot_dir_new)

    # Check if snapshot LLVM versions are compatible with the current version
    llvm_version = subprocess.check_output(
        ["llvm-config", "--version"]).decode().rstrip().split(".")[0]

    if llvm_version != old_snapshot.llvm_version:
        sys.stderr.write(
            "Error: old snapshot was built with LLVM {}, "
            "current version is LLVM {}.\n".format(
                old_snapshot.llvm_version, llvm_version))
        sys.exit(errno.EINVAL)

    if llvm_version != new_snapshot.llvm_version:
        sys.stderr.write(
            "Error: new snapshot was built with LLVM {}, "
            "current version is LLVM {}.\n".format(
                new_snapshot.llvm_version, llvm_version))
        sys.exit(errno.EINVAL)

    # Set the output directory
    if not args.stdout:
        if args.output_dir:
            output_dir = args.output_dir
            if os.path.isdir(output_dir):
                sys.stderr.write("Error: output directory exists\n")
                sys.exit(errno.EEXIST)
        else:
            output_dir = default_output_dir(args.snapshot_dir_old,
                                            args.snapshot_dir_new)
    else:
        output_dir = None

    if args.function:
        old_snapshot.filter([args.function])
        new_snapshot.filter([args.function])

    # Transform difference pattern files into an LLVM IR based configuration.
    if args.patterns:
        pattern_config = PatternConfig.create_from_file(args.patterns)
    else:
        pattern_config = None

    config = Config(old_snapshot, new_snapshot, args.show_diff,
                    args.output_llvm_ir, pattern_config,
                    args.control_flow_only, args.print_asm_diffs,
                    args.verbose, args.enable_simpll_ffi,
                    args.semdiff_tool)
    result = Result(Result.Kind.NONE, args.snapshot_dir_old,
                    args.snapshot_dir_old, start_time=default_timer())

    for group_name, group in sorted(old_snapshot.fun_groups.items()):
        group_printed = False

        # Set the group directory
        if output_dir is not None and group_name is not None:
            group_dir = os.path.join(output_dir, group_name)
        else:
            group_dir = None

        result_graph = None
        cache = SimpLLCache(mkdtemp())
        module_cache = {}

        if args.enable_module_cache:
            modules_to_cache = _get_modules_to_cache(group.functions.items(),
                                                     group_name,
                                                     new_snapshot,
                                                     2)
        else:
            modules_to_cache = set()

        for fun, old_fun_desc in sorted(group.functions.items()):
            # Check if the function exists in the other snapshot
            new_fun_desc = new_snapshot.get_by_name(fun, group_name)
            if not new_fun_desc:
                continue

            # Check if the module exists in both snapshots
            if old_fun_desc.mod is None or new_fun_desc.mod is None:
                result.add_inner(Result(Result.Kind.UNKNOWN, fun, fun))
                if group_name is not None and not group_printed:
                    print("{}:".format(group_name))
                    group_printed = True
                print("{}: unknown".format(fun))
                continue

            # If function has a global variable, set it
            glob_var = LlvmParam(old_fun_desc.glob_var) \
                if old_fun_desc.glob_var else None

            # Run the semantic diff
            fun_result = functions_diff(
                mod_first=old_fun_desc.mod, mod_second=new_fun_desc.mod,
                fun_first=fun, fun_second=fun,
                glob_var=glob_var, config=config,
                prev_result_graph=result_graph, function_cache=cache,
                module_cache=module_cache, modules_to_cache=modules_to_cache)
            result_graph = fun_result.graph

            if fun_result is not None:
                if args.regex_filter is not None:
                    # Filter results by regex
                    pattern = re.compile(args.regex_filter)
                    for called_res in fun_result.inner.values():
                        if pattern.search(called_res.diff):
                            break
                    else:
                        fun_result.kind = Result.Kind.EQUAL

                result.add_inner(fun_result)

                # Printing information about failures and non-equal functions.
                if fun_result.kind in [Result.Kind.NOT_EQUAL,
                                       Result.Kind.ERROR, Result.Kind.UNKNOWN]:
                    if fun_result.kind == Result.Kind.NOT_EQUAL:
                        # Create the output directory if needed
                        if output_dir is not None:
                            if not os.path.isdir(output_dir):
                                os.mkdir(output_dir)
                        # Create the group directory or print the group name
                        # if needed
                        if group_dir is not None:
                            if not os.path.isdir(group_dir):
                                os.mkdir(group_dir)
                        elif group_name is not None and not group_printed:
                            print("{}:".format(group_name))
                            group_printed = True
                        print_syntax_diff(
                            snapshot_dir_old=args.snapshot_dir_old,
                            snapshot_dir_new=args.snapshot_dir_new,
                            fun=fun,
                            fun_result=fun_result,
                            fun_tag=old_fun_desc.tag,
                            output_dir=group_dir if group_dir else output_dir,
                            show_diff=args.show_diff,
                            initial_indent=2 if (group_name is not None and
                                                 group_dir is None) else 0)
                    else:
                        # Print the group name if needed
                        if group_name is not None and not group_printed:
                            print("{}:".format(group_name))
                            group_printed = True
                        print("{}: {}".format(fun, str(fun_result.kind)))

            # Clean LLVM modules (allow GC to collect the occupied memory)
            old_fun_desc.mod.clean_module()
            new_fun_desc.mod.clean_module()
            LlvmModule.clean_all()

    old_snapshot.finalize()
    new_snapshot.finalize()

    if output_dir is not None and os.path.isdir(output_dir):
        print("Differences stored in {}/".format(output_dir))

    if args.report_stat:
        print("")
        print("Statistics")
        print("----------")
        result.stop_time = default_timer()
        result.report_stat(args.show_errors)
    return 0


def default_output_dir(src_snapshot, dest_snapshot):
    """Name of the directory to put log files into."""
    base_dirname = "diff-{}-{}".format(
        os.path.basename(os.path.normpath(src_snapshot)),
        os.path.basename(os.path.normpath(dest_snapshot)))
    if os.path.isdir(base_dirname):
        suffix = 0
        dirname = base_dirname
        while os.path.isdir(dirname):
            dirname = "{}-{}".format(base_dirname, suffix)
            suffix += 1
        return dirname
    return base_dirname


def print_syntax_diff(snapshot_dir_old, snapshot_dir_new, fun, fun_result,
                      fun_tag, output_dir, show_diff, initial_indent):
    """
    Log syntax diff of 2 functions. If log_files is set, the output is printed
    into a separate file, otherwise it goes to stdout.
    :param snapshot_dir_old: Old snapshot directory.
    :param snapshot_dir_new: New snapshot directory.
    :param fun: Name of the analysed function
    :param fun_tag: Analysed function tag
    :param fun_result: Result of the analysis
    :param output_dir: True if the output is to be written into a file
    :param show_diff: Print syntax diffs.
    :param initial_indent: Initial indentation of printed messages
    """
    def text_indent(text, width):
        """Indent each line in the text by a number of spaces given by width"""
        return ''.join(" " * width + line for line in text.splitlines(True))

    if fun_result.kind == Result.Kind.NOT_EQUAL:
        if output_dir:
            output = open(os.path.join(output_dir, "{}.diff".format(fun)), "w")
            output.write(
                "Found differences in functions called by {}".format(fun))
            if fun_tag is not None:
                output.write(" ({})".format(fun_tag))
            output.write("\n\n")
            indent = initial_indent + 2
        else:
            output = sys.stdout
            if fun_tag is not None:
                output.write(text_indent("{} ({}):\n".format(fun, fun_tag),
                                         initial_indent))
            else:
                output.write(text_indent("{}:\n".format(fun), initial_indent))
            indent = initial_indent + 4

        for called_res in sorted(fun_result.inner.values(),
                                 key=lambda r: r.first.name):
            if called_res.diff == "" and called_res.first.covered:
                # Do not print empty diffs
                continue

            output.write(
                text_indent("{} differs:\n".format(called_res.first.name),
                            indent - 2))
            if not output_dir:
                output.write(text_indent("{{{\n", indent - 2))

            if called_res.first.callstack:
                output.write(
                    text_indent("Callstack ({}):\n".format(snapshot_dir_old),
                                indent))
                output.write(text_indent(
                    called_res.first.callstack.replace(
                        os.path.join(os.path.abspath(snapshot_dir_old), ""),
                        ""), indent))
                output.write("\n\n")
            if called_res.second.callstack:
                output.write(
                    text_indent("Callstack ({}):\n".format(snapshot_dir_new),
                                indent))
                output.write(text_indent(
                    called_res.second.callstack.replace(
                        os.path.join(os.path.abspath(snapshot_dir_new), ""),
                        ""),
                    indent))
                output.write("\n\n")

            if show_diff:
                if (called_res.diff.strip() == "" and
                        called_res.macro_diff is not None):
                    output.write(text_indent(
                        "\n".join(map(str, called_res.macro_diff)), indent))
                else:
                    output.write(text_indent("Diff:\n", indent))
                    output.write(text_indent(
                        called_res.diff, indent))

            if not output_dir:
                output.write(text_indent("}}}\n", indent - 2))
            output.write("\n")
