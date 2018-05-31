from __future__ import absolute_import

from argparse import ArgumentParser
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.semdiff.module_diff import modules_diff, Statistics
from diffkemp.semdiff.function_diff import Result
import sys


def __make_argument_parser():
    """ Parsing arguments. """
    ap = ArgumentParser()
    ap.add_argument("modules_dir")
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("-m", "--module", help="analyse only chosen module")
    ap.add_argument("--build-only", help="only build modules to LLVM IR",
                    action="store_true")
    ap.add_argument("-d", "--debug", help="compile module with -g",
                    action="store_true")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    return ap


def run_from_cli():
    """ Main method to run the tool. """
    ap = __make_argument_parser()
    args = ap.parse_args()

    try:
        first_builder = LlvmKernelBuilder(args.src_version, args.modules_dir,
                                          args.debug)
        if args.module:
            first_mods = {
                args.module: first_builder.build_module(args.module,
                                                        args.rebuild)
            }
        else:
            first_mods = first_builder.build_modules_with_params(args.rebuild)

        second_builder = LlvmKernelBuilder(args.dest_version, args.modules_dir,
                                           args.debug)
        if args.module:
            second_mods = {
                args.module: second_builder.build_module(args.module,
                                                         args.rebuild)
            }
        else:
            second_mods = second_builder.build_modules_with_params(args.rebuild)

        if args.build_only:
            print "Compiled modules in version %s:" % args.src_version
            for mod_name in first_mods.keys():
                print "%s " % mod_name
            print "Compiled modules in version %s:" % args.dest_version
            for mod_name in second_mods.keys():
                print "%s " % mod_name
            return 0


        # Compare modules
        stat = modules_diff(first_mod.llvm, second_mod.llvm, args.parameter,
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

