from diffkemp.building.building_files.build_c_project_file import build_c_project, build_c_file
from diffkemp.building.building_files.build_utils import generate_from_function_list, read_symbol_list
from diffkemp.config import Config
from diffkemp.snapshot import Snapshot
from diffkemp.llvm_ir.source_tree import SourceTree
from diffkemp.llvm_ir.llvm_module import LlvmParam, LlvmModule
from diffkemp.llvm_ir.single_llvm_finder import SingleLlvmFinder
from diffkemp.semdiff.caching import SimpLLCache
from diffkemp.semdiff.function_diff import functions_diff
from diffkemp.semdiff.result import Result
from diffkemp.output import YamlOutput
from diffkemp.syndiff.function_syntax_diff import unified_syntax_diff
from http.server import HTTPServer, SimpleHTTPRequestHandler
from tempfile import mkdtemp
from timeit import default_timer
import errno
import os
import re
import sys
import shutil
import yaml

VIEW_INSTALL_DIR = "/var/lib/diffkemp/view"
# Name of YAML output file created by diffkemp compare command.
YAML_FILE_NAME = "diffkemp-out.yaml"
# Error message shown when no symbols were found in the symbol list file.
EMSG_EMPTY_SYMBOL_LIST = "ERROR: symbol list is empty or could not be read\n"


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

    config = Config.from_args(args)
    result = Result(Result.Kind.NONE, args.snapshot_dir_old,
                    args.snapshot_dir_old, start_time=default_timer())

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
                                       Result.Kind.UNKNOWN,
                                       Result.Kind.ERROR] or config.full_diff:
                    if fun_result.kind == Result.Kind.NOT_EQUAL or \
                       config.full_diff:
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
                            show_diff=config.show_diff,
                            full_diff=config.full_diff,
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
    # Create yaml output
    if output_dir is not None and os.path.isdir(output_dir):
        old_dir_abs = os.path.join(os.path.abspath(args.snapshot_dir_old), "")
        new_dir_abs = os.path.join(os.path.abspath(args.snapshot_dir_new), "")
        yaml_output = YamlOutput(snapshot_dir_old=old_dir_abs,
                                 snapshot_dir_new=new_dir_abs, result=result)
        yaml_output.save(output_dir=output_dir, file_name=YAML_FILE_NAME)
    config.snapshot_first.finalize()
    config.snapshot_second.finalize()

    if output_dir is not None and os.path.isdir(output_dir):
        print("Differences stored in {}/".format(output_dir))

    if args.report_stat or args.extended_stat:
        print("")
        print("Statistics")
        print("----------")
        result.stop_time = default_timer()
        result.report_stat(args.show_errors, args.extended_stat)
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


def view(args):
    """
    View the compare differences. Prepares files for the visualisation
    and runs viewer.
    """
    viewer = Viewer(compare_output_dir=args.compare_output_dir,
                    devel=args.devel)
    viewer.run()


class Viewer:
    """Class for running the result viewer (viewer of found differences)."""
    def __init__(self, compare_output_dir, devel):
        """
        :param compare_output_dir: Dir containing output of compare command.
        :param devel: True to run viewer in development mode.
        """
        self.compare_output_dir = compare_output_dir
        self.devel = devel
        # Relative path of src files which were already copied to viewer dir.
        self.old_processed_files = set()
        self.new_processed_files = set()
        # Path to snapshot directories.
        self.old_snapshot_dir = None
        self.new_snapshot_dir = None
        # Loaded content of YAML which describes the found differences.
        self.yaml_result = None
        # Path to the viewer.
        self.view_dir = None
        # Path to the directory which viewer can access.
        self.public_dir = None
        # Path to the viewer dir containing data of the compared project.
        self.data_dir = None
        # Path to the viewer dir containing sources of the compared project.
        self.old_out_dir = None
        self.new_out_dir = None
        # Path to the viewer dir containing diff of functions.
        self.diff_dir = None
        # Path to the yaml (describing the result) in the viewer directory.
        self.viewer_yaml_path = None

    def run(self):
        """Prepare and run the viewer."""
        self._load_yaml()
        self._get_snapshot_dirs()
        self._get_viewer_dir()
        self._prepare_dirs()
        self._prepare_files_and_diffs()
        self._start_server()
        self._clean()

    def _load_yaml(self):
        """Load yaml which describes results."""
        yaml_path = os.path.join(self.compare_output_dir, YAML_FILE_NAME)
        if not os.path.exists(yaml_path):
            sys.stderr.write(
                f"ERROR: compare output is missing file '{YAML_FILE_NAME}'\n")
            sys.exit(errno.EINVAL)
        with open(yaml_path, "r") as file:
            self.yaml_result = yaml.safe_load(file)

    def _get_viewer_dir(self):
        """Determine the viewer directory.
        The manually built one has priority over the installed one."""
        self.view_dir = os.path.join(os.path.dirname(__file__), "../view")
        # Manually build one
        if os.path.exists(self.view_dir):
            if self.devel:
                self.public_dir = os.path.join(self.view_dir, "public")
            else:
                self.public_dir = os.path.join(self.view_dir, "build")
                if not os.path.isdir(self.public_dir):
                    sys.stderr.write(
                        "Could not find production build of the viewer.\n"
                        "Use --devel to run a development server "
                        "or execute CMake with -DBUILD_VIEWER=On.\n")
                    sys.exit(errno.ENOENT)
        # Installed one
        elif os.path.exists(VIEW_INSTALL_DIR):
            self.view_dir = VIEW_INSTALL_DIR
            self.public_dir = VIEW_INSTALL_DIR
            if self.devel:
                sys.stderr.write(
                    "Error: it is not possible to run the development server "
                    "for installed DiffKemp\n")
                sys.exit(errno.EINVAL)
        # View directory does not exist
        else:
            sys.stderr.write(
                "Error: the viewer was not found.\n"
                "The viewer was probably not installed.\n")
            sys.exit(errno.ENOENT)

    def _prepare_dirs(self):
        """Set and create subdirectories in the viewer directory."""
        # Dir for storing necessary data for a visualisation.
        self.data_dir = os.path.join(self.public_dir, "data")
        if not os.path.exists(self.data_dir):
            os.mkdir(self.data_dir)
        # Preparing source directories
        self.old_out_dir = os.path.join(self.data_dir, "src-old")
        self.new_out_dir = os.path.join(self.data_dir, "src-new")
        if not os.path.exists(self.old_out_dir):
            os.mkdir(self.old_out_dir)
        if not os.path.exists(self.new_out_dir):
            os.mkdir(self.new_out_dir)
        # Preparing diff directory
        self.diff_dir = os.path.join(self.data_dir, "diffs")
        if not os.path.exists(self.diff_dir):
            os.mkdir(self.diff_dir)

    def _get_snapshot_dirs(self):
        """Extract path to snapshot directories from the YAML."""
        self.old_snapshot_dir = self.yaml_result["old-snapshot"]
        self.new_snapshot_dir = self.yaml_result["new-snapshot"]
        # Check if snapshot dirs exist
        if not os.path.isdir(self.old_snapshot_dir):
            sys.stderr.write("Error: expecting to find old snapshot in "
                             f"{self.old_snapshot_dir}\n")
            sys.exit(errno.EINVAL)
        if not os.path.isdir(self.new_snapshot_dir):
            sys.stderr.write("Error: expecting to find new snapshot in "
                             f"{self.new_snapshot_dir}\n")
            sys.exit(errno.EINVAL)

    def _prepare_files_and_diffs(self):
        """Prepare source and diff files to viewer directory."""
        for name, definition in self.yaml_result["definitions"].items():
            self._prepare_file_and_diff(name, definition)
        # Save updated YAML to viewer directory.
        self.viewer_yaml_path = os.path.join(self.data_dir, YAML_FILE_NAME)
        with open(self.viewer_yaml_path, "w") as file:
            yaml.dump(self.yaml_result, file, sort_keys=False)

    def _prepare_file_and_diff(self, name, definition):
        """
        If possible create diff for symbol specified by `name` and described by
        `definition`, update the YAML with info about the diff existence and
        save the source file containing the symbol and the diff to viewer dir.
        """
        # Relatives paths to source files, None if we do not know it.
        old_file = None
        new_file = None
        if "old" in definition:
            old_file = definition["old"]["file"]
            self._copy_file(old_file, is_old=True)
        if "new" in definition:
            new_file = definition["new"]["file"]
            self._copy_file(new_file, is_old=False)

        if (old_file is None or new_file is None or
                "end-line" not in definition["old"] or
                "end-line" not in definition["new"]):
            definition["diff"] = False
            return

        # Make diff of function, add diff info to YAML
        old_file_abs_path = os.path.join(self.old_snapshot_dir, old_file)
        new_file_abs_path = os.path.join(self.new_snapshot_dir, new_file)
        diff = unified_syntax_diff(
            first_file=old_file_abs_path,
            second_file=new_file_abs_path,
            first_line=definition["old"]["line"],
            second_line=definition["new"]["line"],
            first_end=definition["old"]["end-line"],
            second_end=definition["new"]["end-line"])
        if diff.isspace() or diff == "":
            definition["diff"] = False
        else:
            definition["diff"] = True
            diff_path = os.path.join(self.diff_dir, name + ".diff")
            with open(diff_path, "w") as file:
                file.write(diff)

    def _copy_file(self, file_path, is_old=True):
        """
        Copy file to out_src_dir if it is not already processed.
        :param file_path: Relative path to snapshot dir of a file to copy.
        :param is_old: True for old version, false for new version of project.
        """
        if is_old:
            file_abs_path = os.path.join(self.old_snapshot_dir, file_path)
            processed_files = self.old_processed_files
            out_dir = self.old_out_dir
        else:
            file_abs_path = os.path.join(self.new_snapshot_dir, file_path)
            processed_files = self.new_processed_files
            out_dir = self.new_out_dir
        if file_path in processed_files:
            return
        processed_files.add(file_path)
        output_file_path = os.path.join(out_dir, file_path)
        # Copying directory hierarchy.
        output_dir_path = os.path.dirname(output_file_path)
        os.makedirs(output_dir_path, exist_ok=True)
        # Copying file.
        shutil.copy(file_abs_path, output_dir_path)

    def _start_server(self):
        """Start server with the viewer."""
        if self.devel:
            os.chdir(self.view_dir)
            os.system("npm install")
            os.system("npm start")
        else:
            os.chdir(self.public_dir)
            handler = SimpleHTTPRequestHandler
            handler.log_message = lambda *_, **__: None
            with HTTPServer(("localhost", 3000), handler) as httpd:
                print("Result viewer is available at http://localhost:3000")
                print("Press Ctrl+C to exit")
                try:
                    httpd.serve_forever()
                except KeyboardInterrupt:
                    httpd.shutdown()

    def _clean(self):
        """Clean directories."""
        shutil.rmtree(self.old_out_dir)
        shutil.rmtree(self.new_out_dir)
        shutil.rmtree(self.diff_dir)
        os.remove(self.viewer_yaml_path)
