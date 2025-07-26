from diffkemp.building.cc_wrapper import get_cc_wrapper_path, wrapper_env_vars
from diffkemp.llvm_ir.optimiser import opt_llvm, BuildException
from diffkemp.llvm_ir.single_c_builder import SingleCBuilder
from diffkemp.llvm_ir.source_tree import SourceTree
from diffkemp.llvm_ir.wrapper_build_finder import WrapperBuildFinder
from diffkemp.snapshot import Snapshot
from diffkemp.building.building_files.build_utils import read_symbol_list, generate_from_function_list
from subprocess import check_call, CalledProcessError
from tempfile import mkdtemp
import errno
import os
import sys
import shutil

EMSG_EMPTY_SYMBOL_LIST = "ERROR: symbol list is empty or could not be read\n"

def build_c_project(args):
    # Generate wrapper for C/C++ compiler
    cc_wrapper = get_cc_wrapper_path(args.no_native_cc_wrapper)

    # Create temp directory and environment
    environment, tmpdir, db_filename = create_temp_dir_and_env(args)

    # Determine make args
    make_cc_setting = 'CC="{}"'.format(cc_wrapper)
    make_args = [args.build_program, "-C", args.source_dir, make_cc_setting]
    if args.build_file is not None:
        make_args.extend(["-c", args.build_file])
    make_target_args = make_args[:]
    if args.target is not None:
        make_target_args.extend(args.target)

    # Clean the project
    config_log_filename = os.path.join(args.source_dir, "config.log")
    clean_project(config_log_filename, make_args, environment)

    # If config.log is present, reconfigure using wrapper
    # Note: this is done to support building with nested configure
    if args.reconfigure and os.path.exists(config_log_filename):
        reconfigure_using_wrapper(config_log_filename, make_cc_setting,\
                                  args, environment)

    # Build the project using generated wrapper
    check_call(make_target_args, env=environment)

    # Run LLVM IR simplification passes if the user did not request
    # to use the default project's optimization.
    if not args.no_opt_override:
        # Run llvm passes on created LLVM IR files.
        llvm_simplification(db_filename)

    # Create a new snapshot from the source directory.
    source_finder = WrapperBuildFinder(args.source_dir, db_filename)
    source = SourceTree(args.source_dir, source_finder)
    snapshot = Snapshot.create_from_source(source, args.output_dir,
                                           "function")

    # Copy the database file into the snapshot directory
    shutil.copyfile(db_filename, os.path.join(args.output_dir, "diffkemp-wdb"))

    # Build sources for symbols from the list into LLVM IR
    symbol_list = build_for_symbols(args)
    generate_from_function_list(snapshot, symbol_list)

    # Create the snapshot directory containing the YAML description file
    snapshot.generate_snapshot_dir()
    snapshot.finalize()
    # Removing the tmp dir with diffkemp-wdb file
    shutil.rmtree(tmpdir)


def create_temp_dir_and_env(args):
    tmpdir = mkdtemp()
    db_filename = os.path.join(tmpdir, "diffkemp-wdb")
    environment = {
        wrapper_env_vars["db_filename"]: db_filename,
        wrapper_env_vars["clang"]: args.clang,
        wrapper_env_vars["clang_append"]: ",".join(args.clang_append)
        if args.clang_append is not None
        else "",
        wrapper_env_vars["clang_drop"]: ",".join(args.clang_drop)
        if args.clang_drop is not None
        else "",
        wrapper_env_vars["llvm_link"]: args.llvm_link,
        wrapper_env_vars["no_opt_override"]: ("1" if args.no_opt_override
                                              else "0")
    }
    environment.update(os.environ)
    return environment, tmpdir, db_filename


def clean_project(config_log_filename, make_args, environment):
    if os.path.exists(config_log_filename):
        # Backup config.log
        os.rename(config_log_filename, config_log_filename + ".bak")
    try:
        check_call(make_args + ["clean"], env=environment)
    except CalledProcessError:
        pass
    if os.path.exists(config_log_filename + ".bak"):
        # Restore config.log
        os.rename(config_log_filename + ".bak", config_log_filename)


def reconfigure_using_wrapper(config_log_filename, make_cc_setting, \
                              args, environment):
    with open(config_log_filename, "r") as config_log:
        # Try to get line with configure command from config.log
        # This line is identified by being the first line containing $
        configure_cmd = None
        for line in config_log.readlines():
            if "$" in line:
                configure_cmd = line.strip()
                break
        if configure_cmd and make_cc_setting not in configure_cmd:
            # Remove beginning of line containing spaces and $
            configure_cmd = configure_cmd.split("$ ", 1)[1]
            configure_cmd += " " + make_cc_setting
            # Delete all config.cache files
            for root, dirs, _ in os.walk(args.source_dir):
                for dirname in dirs:
                    try:
                        os.remove(os.path.join(root, dirname,
                                                "config.cache"))
                    except (FileNotFoundError, PermissionError):
                        pass
            # Reconfigure with CC wrapper
            check_call(configure_cmd, cwd=args.source_dir,
                        env=environment, shell=True)


def llvm_simplification(db_filename):
    with open(db_filename, "r") as db_file:
        for line in [r for r in db_file if r.startswith("o:")]:
            llvm_file = line.split(":")[1].rstrip()
            try:
                opt_llvm(llvm_file)
            except BuildException:
                # Unsuccessful optimization, leaving as it is.
                pass


def build_for_symbols(args):
    user_symbol_list = True
    if args.symbol_list is None:
        user_symbol_list = False
        args.symbol_list = os.path.join(args.source_dir, "function_list")
    symbol_list = read_symbol_list(args.symbol_list)
    if not symbol_list:
        if user_symbol_list:
            sys.stderr.write(EMSG_EMPTY_SYMBOL_LIST)
        else:
            sys.stderr.write("ERROR: no symbols were found in the project\n")
        sys.exit(errno.EINVAL)
    return symbol_list


def build_c_file(args):
    # It ignores following args: build-program, build-file, clang-drop,
    #   llvm-link, llvm-dis, target, reconfigure, no-native-cc-wrapper.
    source_file_name = os.path.basename(args.source_dir)
    source_dir = os.path.dirname(args.source_dir)

    clang_append = args.clang_append if args.clang_append is not None else []
    # Create a new snapshot and generate its content.
    source_finder = SingleCBuilder(source_dir, source_file_name,
                                   clang=args.clang,
                                   clang_append=clang_append,
                                   default_optim=not args.no_opt_override)
    source = SourceTree(source_dir, source_finder)
    snapshot = Snapshot.create_from_source(source, args.output_dir, "function")
    if args.symbol_list is None:
        function_list = source_finder.get_function_list()
    else:
        function_list = read_symbol_list(args.symbol_list)
    if not function_list:
        if args.symbol_list:
            sys.stderr.write(EMSG_EMPTY_SYMBOL_LIST)
        else:
            sys.stderr.write("ERROR: no symbols were found in the file\n")
        sys.exit(errno.EINVAL)
    generate_from_function_list(snapshot, function_list)

    # Create the snapshot directory containing the YAML description file.
    snapshot.generate_snapshot_dir()
    snapshot.finalize()