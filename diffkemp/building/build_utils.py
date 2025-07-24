from diffkemp.llvm_ir.source_tree import SourceNotFoundException
import sys
import os

# Error message shown when no symbols were found in the symbol list file.
EMSG_EMPTY_SYMBOL_LIST = "ERROR: symbol list is empty or could not be read\n"


def read_symbol_list(list_path):
    """
    Read and parse the symbol list file. Filters out entries which are not
    valid symbols (do not start with letter or underscore).
    :param list_path: Path to the list.
    :return: List of symbols (strings).
    """
    with open(list_path, "r") as list_file:
        return [symbol for line in list_file.readlines() if
                (symbol := line.strip()) and
                (symbol[0].isalpha() or symbol[0] == "_")]


def generate_from_function_list(snapshot, fun_list):
    """
    Generate a snapshot from a list of functions.
    For each function, find the source with its definition, compile it into
    LLVM IR, and add the appropriate entry to the snapshot.
    :param snapshot: Existing Snapshot object to fill.
    :param fun_list: List of functions to add. If non-function symbols are
                     present, these are added into the snapshot with empty
                     module entry.
    """
    for symbol in fun_list:
        try:
            sys.stdout.write("{}: ".format(symbol))
            sys.stdout.flush()

            # Find the source for function definition and add it to
            # the snapshot
            llvm_mod = snapshot.source_tree.get_module_for_symbol(symbol)
            if llvm_mod.has_function(symbol):
                snapshot.add_fun(symbol, llvm_mod)
                print(os.path.relpath(llvm_mod.llvm,
                                      snapshot.source_tree.source_dir))
            else:
                snapshot.add_fun(symbol, None)
                print("not a function")
        except SourceNotFoundException:
            print("source not found")
            snapshot.add_fun(symbol, None)
