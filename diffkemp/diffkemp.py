from diffkemp.building.build_c_project \
    import build_c_project, build_c_file
from diffkemp.building.build_utils import (
    generate_from_function_list,
    read_symbol_list,
    EMSG_EMPTY_SYMBOL_LIST)
from diffkemp.snapshot import Snapshot
from diffkemp.llvm_ir.source_tree import SourceTree
from diffkemp.llvm_ir.single_llvm_finder import SingleLlvmFinder
from diffkemp.semdiff.result import Result
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


def print_syntax_diff(snapshot_dir_old, snapshot_dir_new, fun, fun_result,
                      fun_tag, output_dir, show_diff, full_diff,
                      initial_indent):
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
    :param full_diff: Print semantics-preserving syntax diffs too.
    :param initial_indent: Initial indentation of printed messages
    """
    def text_indent(text, width):
        """Indent each line in the text by a number of spaces given by width"""
        return ''.join(" " * width + line for line in text.splitlines(True))

    old_dir_abs_path = os.path.join(os.path.abspath(snapshot_dir_old), "")
    new_dir_abs_path = os.path.join(os.path.abspath(snapshot_dir_new), "")

    if fun_result.kind == Result.Kind.NOT_EQUAL or (
            full_diff and any([x.diff for x in fun_result.inner.values()])):
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
                    called_res.first.callstack.as_str_with_rel_paths(
                        old_dir_abs_path), indent))
                output.write("\n\n")
            if called_res.second.callstack:
                output.write(
                    text_indent("Callstack ({}):\n".format(snapshot_dir_new),
                                indent))
                output.write(text_indent(
                    called_res.second.callstack.as_str_with_rel_paths(
                        new_dir_abs_path), indent))
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
