from diffkemp.config import Config
from diffkemp.utils import CMP_OUTPUT_FILE
from diffkemp.llvm_ir.llvm_module import LlvmParam, LlvmModule
from diffkemp.semdiff.caching import SimpLLCache
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from diffkemp.output import YamlOutput, OutputWriter
from tempfile import mkdtemp
from timeit import default_timer
import errno
import os
import re
import sys

MINIMAL_CACHE_FREQ = 2  # Minimal frequency for a module to be cached


class OutputDirExistsError(Exception):
    pass


def compare(args):
    """
    Compare two snapshots. Prepares configuration and runs the comparison.
    """
    try:
        comparator = SnapshotComparator(args)
        return comparator.run()
    except OutputDirExistsError as e:
        sys.stderr.write("{}".format(e))
        sys.exit(errno.EEXIST)


class GroupInfo:
    def __init__(self, group, group_name, enable_module_cache, config,
                 output_dir):
        self.group = group
        self.group_name = group_name
        self.group_dir = self._get_group_dir(output_dir)
        self.group_printed = False
        self.modules_to_cache = \
            self._get_modules_to_cache_if_enabled(enable_module_cache, config)
        self.result_graph = None
        self.cache = SimpLLCache(mkdtemp())
        self.module_cache = {}

    def _get_group_dir(self, output_dir):
        if output_dir is not None and self.group_name is not None:
            return os.path.join(output_dir, self.group_name)
        return None

    def _get_modules_to_cache_if_enabled(self, enable_module_cache, config):
        if enable_module_cache:
            return self._get_modules_to_cache(config)
        return set()

    def _get_modules_to_cache(self, config, min_frequency=MINIMAL_CACHE_FREQ):
        """
        Generates a list of frequently used modules. These will be loaded
        into cache if DiffKemp is running with module caching enable.
        :param min_frequency: Minimal frequency for a module to be included
        into the cache
        :return: Set of modules that should be loaded into module cache
        """
        module_frequency_map = dict()
        other_snapshot = config.snapshot_second
        for fun, old_fun_desc in self.group.functions.items():
            # Check if the function exists in the other snapshot
            new_fun_desc = other_snapshot.get_by_name(fun, self.group_name)
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


class SnapshotComparator:
    """
    Compare the generated snapshots. Runs the semantic comparison and shows
    information about the compared functions that are semantically different.
    """

    def __init__(self, cmd_args):
        self.args = cmd_args
        self.config = Config.from_args(cmd_args)
        self.result = Result(Result.Kind.NONE, cmd_args.snapshot_dir_old,
                             cmd_args.snapshot_dir_new,
                             start_time=default_timer())
        self.regex_pattern = re.compile(cmd_args.regex_filter) \
            if cmd_args.regex_filter else None
        self.output_dir = None

    def run(self):
        # Set the output directory
        self._set_output_dir()
        return self._compare_snapshots()

    def _set_output_dir(self):
        if self.args.stdout:
            return

        if not self.args.output_dir:
            self.output_dir = self._default_output_dir()
            return

        self.output_dir = self.args.output_dir
        if os.path.isdir(self.output_dir):
            raise OutputDirExistsError(
                "Error: output directory {} exists\n".format(self.output_dir))

    def _default_output_dir(self):
        """Name of the directory to put log files into."""
        base_dirname = "diff-{}-{}".format(
            os.path.basename(os.path.normpath(self.args.snapshot_dir_old)),
            os.path.basename(os.path.normpath(self.args.snapshot_dir_new)))
        if os.path.isdir(base_dirname):
            suffix = 0
            dirname = base_dirname
            while os.path.isdir(dirname):
                dirname = "{}-{}".format(base_dirname, suffix)
                suffix += 1
            return dirname
        return base_dirname

    def _compare_snapshots(self):
        for group_name, group in sorted(self.config.snapshot_first
                                        .fun_groups.items()):
            group_info = GroupInfo(group, group_name,
                                   self.args.enable_module_cache,
                                   self.config, self.output_dir)
            self._compare_groups(group_info)

        self._finalize_output()
        return 0

    def _compare_groups(self, group_info):
        for fun, old_fun_desc in sorted(group_info.group.functions.items()):
            self._compare_function(fun, old_fun_desc, group_info)

    def _compare_function(self, fun, old_fun_desc, group_info):
        # Check if the function exists in the other snapshot
        new_fun_desc = \
            self.config.snapshot_second.get_by_name(fun, group_info.group_name)
        if not new_fun_desc:
            return

        # Check if the module exists in both snapshots
        if not self._modules_exist(old_fun_desc, new_fun_desc):
            self._handle_missing_module(fun, old_fun_desc, group_info)
            return

        # If function has a global variable, set it
        glob_var = LlvmParam(old_fun_desc.glob_var) \
            if old_fun_desc.glob_var else None

        # Run the semantic diff
        fun_result = functions_diff(
            mod_first=old_fun_desc.mod, mod_second=new_fun_desc.mod,
            fun_first=fun, fun_second=fun,
            glob_var=glob_var, config=self.config,
            prev_result_graph=group_info.result_graph,
            function_cache=group_info.cache,
            module_cache=group_info.module_cache,
            modules_to_cache=group_info.modules_to_cache)
        group_info.result_graph = fun_result.graph

        self._handle_fun_result(fun_result, fun, old_fun_desc, group_info)
        self._cleanup_modules(old_fun_desc, new_fun_desc)

    @staticmethod
    def _modules_exist(old_fun_desc, new_fun_desc):
        return old_fun_desc.mod is not None and \
                new_fun_desc.mod is not None

    def _handle_missing_module(self, fun, old_fun_desc, group_info):
        fun_result = Result(Result.Kind.UNKNOWN, fun, fun)
        self.result.add_inner(fun_result)
        self._print_fun_result(fun_result, fun, old_fun_desc, group_info)

    def _handle_fun_result(self, fun_result, fun, old_fun_desc, group_info):

        if self.args.regex_filter is not None:
            # Filter results by regex
            self._filter_result_by_regex(fun_result)

        self.result.add_inner(fun_result)

        # Printing information about failures and non-equal functions.
        if fun_result.kind in [Result.Kind.NOT_EQUAL,
                               Result.Kind.UNKNOWN,
                               Result.Kind.ERROR] \
           or self.config.full_diff:
            self._print_fun_result(fun_result, fun, old_fun_desc, group_info)

    def _filter_result_by_regex(self, fun_result):
        for called_res in fun_result.inner.values():
            if self.regex_pattern.search(called_res.diff):
                return
        fun_result.kind = Result.Kind.EQUAL

    def _print_fun_result(self, fun_result, fun, old_fun_desc, group_info):
        if fun_result.kind != Result.Kind.NOT_EQUAL and \
           not self.config.full_diff:
            # Print the group name if needed
            if group_info.group_name is not None and \
               not group_info.group_printed:
                print("{}:".format(group_info.group_name))
                group_info.group_printed = True
            print("{}: {}".format(fun, str(fun_result.kind)))
            return

        # Create the output directory if needed
        if self.output_dir is not None:
            os.makedirs(self.output_dir, exist_ok=True)

        # Create the group directory or print the group name if needed
        if group_info.group_dir is not None:
            os.makedirs(group_info.group_dir, exist_ok=True)
        elif group_info.group_name is not None and \
                not group_info.group_printed:
            print("{}:".format(group_info.group_name))
            group_info.group_printed = True

        self.print_syntax_diff(
            fun=fun,
            fun_result=fun_result,
            fun_tag=old_fun_desc.tag,
            group_info=group_info)

    def print_syntax_diff(self, fun, fun_result, fun_tag, group_info):
        """
        Log syntax diff of 2 functions. If output_dir is set, the output is
        printed into a separate file inside the specified directory,
        otherwise it goes to stdout.
        :param fun: Name of the analysed function
        :param fun_tag: Analysed function tag
        :param fun_result: Result of the analysis
        """
        output_dir = group_info.group_dir if group_info.group_dir else \
            self.output_dir
        initial_indent = 2 if (group_info.group_name is not None and
                               group_info.group_dir is None) else 0

        old_dir_abs_path = os.path.join(
            os.path.abspath(self.args.snapshot_dir_old), "")
        new_dir_abs_path = os.path.join(
            os.path.abspath(self.args.snapshot_dir_new), "")

        if fun_result.kind == Result.Kind.NOT_EQUAL or (
                self.config.full_diff and
                any([x.diff for x in fun_result.inner.values()])):

            with OutputWriter(output_dir, fun, fun_tag, initial_indent,
                              self.config.show_diff,
                              self.args.snapshot_dir_old,
                              self.args.snapshot_dir_new,
                              old_dir_abs_path, new_dir_abs_path) as writer:

                for called_res in sorted(fun_result.inner.values(),
                                         key=lambda r: r.first.name):
                    if called_res.diff == "" and called_res.first.covered:
                        # Do not print empty diffs
                        continue
                    writer.write_called_result(called_res)

    @staticmethod
    def _cleanup_modules(old_fun_desc, new_fun_desc):
        # Clean LLVM modules (allow GC to collect the occupied memory)
        old_fun_desc.mod.clean_module()
        new_fun_desc.mod.clean_module()
        LlvmModule.clean_all()

    def _finalize_output(self):
        self.config.snapshot_first.finalize()
        self.config.snapshot_second.finalize()

        if self.output_dir is not None and os.path.isdir(self.output_dir):
            self._create_yaml_output()
            print("Differences stored in {}/".format(self.output_dir))
        if self.args.report_stat or self.args.extended_stat:
            self._print_stats()

    def _create_yaml_output(self):
        old_dir_abs = \
            os.path.join(os.path.abspath(self.args.snapshot_dir_old), "")
        new_dir_abs = \
            os.path.join(os.path.abspath(self.args.snapshot_dir_new), "")
        yaml_output = YamlOutput(snapshot_dir_old=old_dir_abs,
                                 snapshot_dir_new=new_dir_abs,
                                 result=self.result)
        yaml_output.save(output_dir=self.output_dir, file_name=CMP_OUTPUT_FILE)

    def _print_stats(self):
        print(f"\nStatistics\n{'-' * 11}")
        self.result.stop_time = default_timer()
        self.result.report_stat(self.args.show_errors, self.args.extended_stat)
