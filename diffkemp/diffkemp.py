from argparse import ArgumentParser, SUPPRESS
from diffkemp.config import Config
from diffkemp.function_list import FunctionList
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
from diffkemp.llvm_ir.kernel_source import KernelSource, \
    SourceNotFoundException
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
import os
import re
import shutil
import sys


def __make_argument_parser():
    """Parsing CLI arguments."""
    ap = ArgumentParser(description="Checking equivalence of semantics of "
                                    "kernel functions.")
    ap.add_argument("-v", "--verbose",
                    help="increase output verbosity",
                    action="store_true")
    sub_ap = ap.add_subparsers(dest="command", metavar="command")
    sub_ap.required = True

    # "generate" sub-command
    generate_ap = sub_ap.add_parser("generate",
                                    help="generate snapshot of kernel "
                                         "functions")
    generate_ap.add_argument("kernel_dir",
                             help="kernel root directory")
    generate_ap.add_argument("output_dir",
                             help="output directory of the snapshot")
    generate_ap.add_argument("functions_list",
                             help="list of functions to compare")
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
    compare_ap.add_argument("--report-stat",
                            help="report statistics of the analysis",
                            action="store_true")
    compare_ap.add_argument("--kernel-dirs",
                            nargs=2,
                            help="specify root dirs for the compared kernels")
    compare_ap.add_argument("--function", "-f",
                            help="compare only selected function")
    compare_ap.add_argument("--control-flow-only",
                            help=SUPPRESS,
                            action="store_true")
    compare_ap.add_argument("--semdiff-tool",
                            help=SUPPRESS,
                            choices=["llreve"])
    compare_ap.set_defaults(func=compare)
    return ap


def run_from_cli():
    """Main method to run the tool."""
    ap = __make_argument_parser()
    args = ap.parse_args()
    args.func(args)


def generate(args):
    """
    Generate snapshot of sources of kernel functions.
    This involves:
      - find source code with functions definitions
      - compile the source codes into LLVM IR
      - copy LLVM and C source files into snapshot directory
      - create YAML with list mapping functions to their LLVM sources
    """
    source = KernelSource(args.kernel_dir, True)
    args.output_dir = os.path.abspath(args.output_dir)
    fun_list = FunctionList(args.output_dir)

    # Cleanup or create the output directory
    if os.path.isdir(args.output_dir):
        shutil.rmtree(args.output_dir)
    os.mkdir(args.output_dir)

    # Build sources for functions from the list into LLVM IR
    with open(args.functions_list, "r") as fun_list_file:
        for line in fun_list_file.readlines():
            fun = line.strip()
            if not fun or not (fun[0].isalpha() or fun[0] == "_"):
                continue
            sys.stdout.write("{}: ".format(fun))
            try:
                llvm_mod = source.get_module_for_symbol(fun)
                print(os.path.relpath(llvm_mod.llvm, args.kernel_dir))
                fun_list.add(fun, llvm_mod)
            except SourceNotFoundException:
                print("source not found")

    # Copy LLVM files to the snapshot
    source.copy_source_files(fun_list.modules(), args.output_dir)
    source.copy_cscope_files(args.output_dir)

    # Create YAML with functions list
    with open(os.path.join(args.output_dir, "functions.yaml"),
              "w") as fun_list_yaml:
        fun_list_yaml.write(fun_list.to_yaml())

    source.finalize()


def compare(args):
    old_functions = FunctionList(args.snapshot_dir_old)
    with open(os.path.join(args.snapshot_dir_old, "functions.yaml"),
              "r") as fun_list_yaml:
        old_functions.from_yaml(fun_list_yaml.read())
    old_source = KernelSource(args.snapshot_dir_old)
    new_functions = FunctionList(args.snapshot_dir_new)
    with open(os.path.join(args.snapshot_dir_new, "functions.yaml"),
              "r") as fun_list_yaml:
        new_functions.from_yaml(fun_list_yaml.read())
    new_source = KernelSource(args.snapshot_dir_new)

    if args.function:
        old_functions.filter([args.function])
        new_functions.filter([args.function])

    config = Config(old_source, new_source, args.show_diff,
                    args.control_flow_only, args.verbose,
                    args.semdiff_tool)
    result = Result(Result.Kind.NONE, args.snapshot_dir_old,
                    args.snapshot_dir_old)

    for fun, old_mod in sorted(old_functions.functions.items()):
        new_mod = new_functions.get_by_name(fun)
        if not (old_mod.has_function(fun) and new_mod.has_function(fun)):
            continue

        fun_result = functions_diff(
            mod_first=old_mod, mod_second=new_mod,
            fun_first=fun, fun_second=fun,
            glob_var=None, config=config)

        if fun_result is not None:
            if args.regex_filter is not None:
                # Filter results by regex
                pattern = re.compile(args.regex_filter)
                for called_res in fun_result.inner.values():
                    if pattern.search(called_res.diff):
                        break
                else:
                    fun_result.kind = Result.Kind.EQUAL_SYNTAX

            result.add_inner(fun_result)
            if fun_result.kind in [Result.Kind.ERROR, Result.Kind.UNKNOWN]:
                print("{}: {}".format(fun, str(fun_result.kind)))
            elif fun_result.kind == Result.Kind.NOT_EQUAL:
                print_syntax_diff(args.snapshot_dir_old,
                                  args.snapshot_dir_new,
                                  fun, fun_result, False,
                                  args.show_diff)

        # Clean LLVM modules (allow GC to collect the occupied memory)
        old_mod.clean_module()
        new_mod.clean_module()
        LlvmKernelModule.clean_all()

    if args.report_stat:
        print("")
        print("Statistics")
        print("----------")
        result.report_stat()
    return 0


def logs_dirname(src_version, dest_version):
    """Name of the directory to put log files into."""
    return "kabi-diff-{}_{}".format(src_version, dest_version)


def print_syntax_diff(snapshot_old, snapshot_new, fun, fun_result, log_files,
                      show_diff):
    """
    Log syntax diff of 2 functions. If log_files is set, the output is printed
    into a separate file, otherwise it goes to stdout.
    :param snapshot_old: Old snapshot directory
    :param snapshot_new: New snapshot directory
    :param fun: Name of the analysed function
    :param fun_result: Result of the analysis
    :param log_files: True if the output is to be written into a file
    :param print_empty_diffs: Print empty syntax diffs.
    """
    def text_indent(text, width):
        """Indent each line in the text by a number of spaces given by width"""
        return ''.join(" " * width + line for line in text.splitlines(True))

    if fun_result.kind == Result.Kind.NOT_EQUAL:
        if log_files:
            output = open(
                os.path.join(logs_dirname(snapshot_old, snapshot_new),
                             "{}.diff".format(fun)), 'w')
            indent = 2
        else:
            output = sys.stdout
            indent = 4
            print("{}:".format(fun))

        for called_res in fun_result.inner.values():
            if called_res.diff == "" and called_res.first.covered_by_syn_diff:
                # Do not print empty diffs
                continue

            output.write(
                text_indent("{} differs:\n".format(called_res.first.name),
                            indent - 2))
            if not log_files:
                print("  {{{")

            if called_res.first.callstack:
                output.write(
                    text_indent("Callstack ({}):\n".format(snapshot_old),
                                indent))
                output.write(text_indent(
                    called_res.first.callstack.replace(
                        os.path.join(os.path.abspath(snapshot_old), ""), ""),
                    indent))
                output.write("\n\n")
            if called_res.second.callstack:
                output.write(
                    text_indent("Callstack ({}):\n".format(snapshot_new),
                                indent))
                output.write(text_indent(
                    called_res.second.callstack.replace(
                        os.path.join(os.path.abspath(snapshot_new), ""), ""),
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

            if not log_files:
                print("  }}}")
            output.write("\n")
