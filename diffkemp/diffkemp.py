#! /usr/bin/env python

from argparse import ArgumentParser
from module_comparator import compare_modules, Statistics
from function_comparator import Result
from slicer.slicer import slice_module
import sys


def __make_argument_parser():
    ap = ArgumentParser()
    ap.add_argument("first")
    ap.add_argument("second")
    ap.add_argument("parameter")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    return ap


def run_from_cli():
    ap = __make_argument_parser()
    args = ap.parse_args()

    try:
        first_sliced = slice_module(args.first, args.parameter, args.verbose)
        second_sliced = slice_module(args.second, args.parameter, args.verbose)

        stat = compare_modules(first_sliced, second_sliced, args.parameter,
                               args.verbose)
        stat.report()

        result = stat.overall_result()
    except Exception as e:
        result = Result.ERROR
        sys.stderr.write(str(e) + "\n")

    if result == Result.EQUAL:
        print("Semantics of the module parameter is same")
    elif result == Result.NOT_EQUAL:
        print("Semantics of the module parameter has changed")
    else:
        print("Unable to determine changes in semantics of the parameter")
    return result

