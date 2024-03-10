"""Unit tests for 'build' sub-command."""
from diffkemp.diffkemp import build
from diffkemp.utils import get_functions_from_llvm
import os
import pytest
import re
import yaml

SINGLE_C_FILE = os.path.abspath("tests/testing_projects/make_based/file.c")
MAKE_BASED_PROJECT_DIR = os.path.abspath("tests/testing_projects/make_based")
# File which build of make based project uses to save names of compiled files.
BUILD_DB_FILE_NAME = "diffkemp-wdb"


class Arguments:
    """Class for creating args for testing of build function."""
    def __init__(self, source_dir, output_dir, target=[],
                 no_opt_override=False):
        self.source_dir = source_dir
        self.output_dir = output_dir
        self.no_opt_override = no_opt_override
        # Required/used by build_c_project
        self.symbol_list = None
        self.build_program = "make"
        self.build_file = None
        self.clang = "clang"
        self.clang_append = []
        self.clang_drop = []
        self.llvm_link = "llvm-link"
        self.target = target
        self.reconfigure = False
        self.no_native_cc_wrapper = False


def get_db_file_content(snapshot_dir):
    """Return content of db file from snapshot_dir."""
    db_file_path = os.path.join(snapshot_dir, BUILD_DB_FILE_NAME)
    with open(db_file_path) as db_file:
        return db_file.read()


def get_llvm_fun_body(llvm_file, fun_name):
    """Returns body of fun_name function from llvm_file."""
    with open(llvm_file, "r") as file:
        content = file.read()
        match = re.search(r"define.*@" + re.escape(fun_name) + r"[ (][^}]*",
                          content, re.MULTILINE)
        return match.group(0)


@pytest.mark.parametrize("source",
                         [SINGLE_C_FILE, MAKE_BASED_PROJECT_DIR],
                         ids=["single c file", "make-based project"])
def test_build_command(source, tmp_path):
    """Testing 'build' sub-command for single c file and make-based project."""
    output_dir = str(tmp_path)
    args = Arguments(source, output_dir)
    build(args)

    # Snapshot should contain source file, LLVM file and file with metadata.
    output_files = os.listdir(output_dir)
    assert "file.c" in output_files
    assert "file.ll" in output_files
    assert "snapshot.yaml" in output_files

    # Functions from the source file should be also in the llvm file.
    llvm_file_path = os.path.join(output_dir, "file.ll")
    llvm_fun_list = get_functions_from_llvm([llvm_file_path])
    assert "add" in llvm_fun_list
    assert "mul" in llvm_fun_list

    # By default functions should not be inlined in LLVM IR,
    # the call of `add` function should be left in the `mul` function.
    body = get_llvm_fun_body(llvm_file_path, "mul")
    assert re.search(r"call.*@add", body) is not None

    # Function names should be in snapshot.yaml
    snapshot_yaml_path = os.path.join(output_dir, "snapshot.yaml")
    with open(snapshot_yaml_path, "r") as file:
        snapshot_yaml = yaml.safe_load(file)
    function_list = [function["name"] for function in snapshot_yaml[0]["list"]]
    assert "add" in function_list
    assert "mul" in function_list


def test_make_based_with_assembly(tmp_path):
    """Testing behaviour of cc_wrapper for assembly files."""
    output_dir = str(tmp_path)
    args = Arguments(MAKE_BASED_PROJECT_DIR, output_dir,
                     target=["with-assembly"])
    build(args)
    # .s, .S files should not be compiled to .ll
    src_dir_files = os.listdir(MAKE_BASED_PROJECT_DIR)
    assert "mod.ll" not in src_dir_files
    assert "sub.ll" not in src_dir_files
    # and they should not be added to db file
    db_file_content = get_db_file_content(output_dir)
    assert "mod.ll" not in db_file_content
    assert "sub.ll" not in db_file_content


def test_make_based_with_linking(tmp_path):
    """Tests make based project which contains linking of file(s)."""
    output_dir = str(tmp_path)
    args = Arguments(MAKE_BASED_PROJECT_DIR, output_dir,
                     target=["with-linking"])
    build(args)
    # When linking *.o files, appropriate .ll files
    # should be linked with llvm-link and saved as `.llw`.
    # Note: The .llw is not copied to snapshot dir.
    assert "file.so.llw" in os.listdir(MAKE_BASED_PROJECT_DIR)
    db_file_content = get_db_file_content(output_dir)
    assert "file.so.llw" in db_file_content


def test_build_no_opt_override(tmp_path):
    """Testing 'build' --no-opt-override argument."""
    output_dir = tmp_path
    args = Arguments(MAKE_BASED_PROJECT_DIR, output_dir, no_opt_override=True)
    build(args)

    # With --no-opt-override the optimization level which is written
    # in Makefile should be used, because it is -O2 the `add` function
    # should not be called from the `mul` function` (should be "inlined").
    llvm_file_path = os.path.join(output_dir, "file.ll")
    body = get_llvm_fun_body(llvm_file_path, "mul")
    assert re.search(r"call.*@add", body) is None
