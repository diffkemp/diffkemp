from diffkemp.diffkemp import print_syntax_diff
from diffkemp.config import Config
from diffkemp.utils import CMP_OUTPUT_FILE
from diffkemp.llvm_ir.llvm_module import LlvmParam, LlvmModule
from diffkemp.semdiff.caching import SimpLLCache
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from diffkemp.output import YamlOutput
from tempfile import mkdtemp
from timeit import default_timer
import errno
import os
import re
import sys


class OutputDirExistsError(Exception):
    pass


def compare(args):
    """
    Compare the generated snapshots. Runs the semantic comparison and shows
    information about the compared functions that are semantically different.
    """
    # Set the output directory
    try:
        output_dir = _set_output_dir(args)
    except OutputDirExistsError as e:
        sys.stderr.write("{}".format(e))
        sys.exit(errno.EEXIST)

    config = Config.from_args(args)
    result = Result(Result.Kind.NONE, args.snapshot_dir_old,
                    args.snapshot_dir_new, start_time=default_timer())

    for group_name, group in sorted(config.snapshot_first.fun_groups.items()):
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
                                                     config.snapshot_second,
                                                     2)
        else:
            modules_to_cache = set()

        for fun, old_fun_desc in sorted(group.functions.items()):

            # Check if the function exists in the other snapshot
            new_fun_desc = config.snapshot_second.get_by_name(fun, group_name)
            if not new_fun_desc:
                continue

            # Check if the module exists in both snapshots
            if old_fun_desc.mod is None or new_fun_desc.mod is None:
                fun_result = Result(Result.Kind.UNKNOWN, fun, fun)
                result.add_inner(fun_result)
                group_printed = _print_fun_result(fun_result, config,
                                                  output_dir, fun,
                                                  group_dir, group_name,
                                                  args, old_fun_desc,
                                                  group_printed)
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
                    _filter_result_by_regex(args.regex_filter, fun_result)

                result.add_inner(fun_result)

                # Printing information about failures and non-equal functions.
                if fun_result.kind in [Result.Kind.NOT_EQUAL,
                                       Result.Kind.UNKNOWN,
                                       Result.Kind.ERROR] or config.full_diff:
                    group_printed = _print_fun_result(fun_result, config,
                                                      output_dir, fun,
                                                      group_dir, group_name,
                                                      args, old_fun_desc,
                                                      group_printed)
            # Clean LLVM modules (allow GC to collect the occupied memory)
            old_fun_desc.mod.clean_module()
            new_fun_desc.mod.clean_module()
            LlvmModule.clean_all()

    # Create yaml output
    if output_dir is not None and os.path.isdir(output_dir):
        _create_yaml_output(args, result, output_dir)
    config.snapshot_first.finalize()
    config.snapshot_second.finalize()

    if output_dir is not None and os.path.isdir(output_dir):
        print("Differences stored in {}/".format(output_dir))
    if args.report_stat or args.extended_stat:
        _print_stats(args.show_errors, result, args.extended_stat)
    return 0


def _set_output_dir(compare_args):
    if compare_args.stdout:
        return None

    if not compare_args.output_dir:
        return _default_output_dir(compare_args.snapshot_dir_old,
                                   compare_args.snapshot_dir_new)
    output_dir = compare_args.output_dir
    if os.path.isdir(output_dir):
        raise OutputDirExistsError(
            "Error: output directory {} exists\n".format(output_dir))
    return output_dir


def _default_output_dir(src_snapshot, dest_snapshot):
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


def _filter_result_by_regex(regex_filter, fun_result):
    pattern = re.compile(regex_filter)
    for called_res in fun_result.inner.values():
        if pattern.search(called_res.diff):
            return
    fun_result.kind = Result.Kind.EQUAL


def _print_fun_result(fun_result, config, output_dir, fun, group_dir,
                      group_name, compare_args, old_fun_desc, group_printed):
    if fun_result.kind != Result.Kind.NOT_EQUAL and \
       not config.full_diff:
        # Print the group name if needed
        if group_name is not None and not group_printed:
            print("{}:".format(group_name))
            group_printed = True
        print("{}: {}".format(fun, str(fun_result.kind)))
        return group_printed

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
        snapshot_dir_old=compare_args.snapshot_dir_old,
        snapshot_dir_new=compare_args.snapshot_dir_new,
        fun=fun,
        fun_result=fun_result,
        fun_tag=old_fun_desc.tag,
        output_dir=group_dir if group_dir else output_dir,
        show_diff=config.show_diff,
        full_diff=config.full_diff,
        initial_indent=2 if (group_name is not None and
                             group_dir is None) else 0)

    return group_printed


def _create_yaml_output(compare_args, result, output_dir):
    old_dir_abs = \
        os.path.join(os.path.abspath(compare_args.snapshot_dir_old), "")
    new_dir_abs = \
        os.path.join(os.path.abspath(compare_args.snapshot_dir_new), "")
    yaml_output = YamlOutput(snapshot_dir_old=old_dir_abs,
                             snapshot_dir_new=new_dir_abs, result=result)
    yaml_output.save(output_dir=output_dir, file_name=CMP_OUTPUT_FILE)


def _print_stats(errors, result, extended_stat):
    print(f"\nStatistics\n{'-' * 11}")
    result.stop_time = default_timer()
    result.report_stat(errors, extended_stat)
