#! /usr/bin/env python

from argparse import ArgumentParser
from compiler.compiler import KernelModuleCompiler
from module_analyser import check_modules
from module_comparator import compare_modules, Statistics
from function_comparator import Result
from slicer.slicer import slice_module
import sys


def __make_argument_parser():
    ap = ArgumentParser()
    ap.add_argument("module_dir")
    ap.add_argument("module_name")
    ap.add_argument("parameter")
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    return ap


def run_from_cli():
    ap = __make_argument_parser()
    args = ap.parse_args()

    try:
        # Compile old module
        first_mod_compiler = KernelModuleCompiler(args.src_version,
                                                  args.module_dir,
                                                  args.module_name)
        first_mod = first_mod_compiler.compile_to_ir(args.verbose)

        # Compile new module
        second_mod_compiler = KernelModuleCompiler(args.dest_version,
                                                  args.module_dir,
                                                  args.module_name)
        second_mod = second_mod_compiler.compile_to_ir(args.verbose)

        # Check modules
        check_modules(first_mod, second_mod, args.parameter)

        # Slice modules
        first_sliced = slice_module(first_mod, args.parameter,
                                    verbose=args.verbose)
        second_sliced = slice_module(second_mod, args.parameter,
                                     verbose=args.verbose)

        # Compare modules
        stat = compare_modules(first_sliced, second_sliced, args.parameter,
                               args.verbose)
        print ""
        stat.report()

        result = stat.overall_result()
    except Exception as e:
        result = Result.ERROR
        sys.stderr.write("Error: %s\n" % str(e))

    if result == Result.EQUAL:
        print("Semantics of the module parameter is same")
    elif result == Result.NOT_EQUAL:
        print("Semantics of the module parameter has changed")
    else:
        print("Unable to determine changes in semantics of the parameter")
    return result

