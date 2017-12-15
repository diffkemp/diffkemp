#! /usr/bin/env python3

from argparse import ArgumentParser
from function_comparator import compare, Result
from slicer.slicer import slice_module


def __make_argument_parser():
    ap = ArgumentParser()
    ap.add_argument("first")
    ap.add_argument("second")
    ap.add_argument("parameter")
    ap.add_argument("function")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    return ap


def main():
    ap = __make_argument_parser()
    args = ap.parse_args()

    first_sliced = slice_module(args.first, args.parameter, args.verbose)
    second_sliced = slice_module(args.second, args.parameter, args.verbose)

    result = compare(first_sliced, second_sliced, args.function, args.verbose)
    if result == Result.EQUAL:
        print("Semantics of the module parameter is same")
    elif result == Result.NOT_EQUAL:
        print("Semantics of the module parameter has changed")
    elif result == Result.UNKNOWN:
        print("Unable to determine changes in semantics of the parameter")
    else:
        print("Internal error")
    return result


if __name__ == "__main__":
    main()
