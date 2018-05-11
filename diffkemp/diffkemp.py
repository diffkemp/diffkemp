#! /usr/bin/env python

from argparse import ArgumentParser
from llvm_ir.build_llvm import LlvmKernelModule
from module_comparator import compare_modules, Statistics
from function_comparator import Result
import sys


def __make_argument_parser():
    ap = ArgumentParser()
    ap.add_argument("module_dir")
    ap.add_argument("module_name")
    ap.add_argument("parameter")
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("-d", "--debug", help="compile module with -g",
                    action="store_true")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    return ap


def run_from_cli():
    ap = __make_argument_parser()
    args = ap.parse_args()

    try:
        # Build old module
        first_mod = LlvmKernelModule(args.src_version, args.module_dir,
                                     args.module_name, args.parameter)
        first_mod.build(args.debug, args.verbose)

        # Build new module
        second_mod = LlvmKernelModule(args.dest_version, args.module_dir,
                                      args.module_name, args.parameter)
        second_mod.build(args.debug, args.verbose)

        # Compare modules
        stat = compare_modules(first_mod.llvm, second_mod.llvm,
                               args.parameter, args.verbose)
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

