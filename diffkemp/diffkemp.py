from __future__ import absolute_import

from argparse import ArgumentParser
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
from diffkemp.semdiff.module_diff import modules_diff, Statistics
import sys


def __make_argument_parser():
    """ Parsing arguments. """
    ap = ArgumentParser(description="Check semantic equivalence of kernel \
                        module parameters."
                        "")
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("modules_dir", help="directory with modules relative to \
                    the kernel sources directory (by default all modules \
                    containing parameters are analysed)")
    ap.add_argument("--all-modules", help="build and analyse all modules (not \
                                          only modules containing parameters)",
                    action="store_true")
    ap.add_argument("-m", "--module", help="analyse only a chosen module")
    ap.add_argument("-p", "--param", help="analyse only a chosen parameter")
    ap.add_argument("-f", "--file", help="analyse only a chosen file")
    ap.add_argument("--build-only", help="only build modules to LLVM IR",
                    action="store_true")
    ap.add_argument("--rebuild", help="force rebuilding sources",
                    action="store_true")
    ap.add_argument("-d", "--debug", help="compile module with -g",
                    action="store_true")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    ap.add_argument("--report-stat", help="report statistics of the analysis",
                    action="store_true")
    ap.add_argument("-t", "--timeout", help="timeout in seconds for a single \
                    parameter comparison")
    ap.add_argument("-s", "--function", help="analyse only specific function")
    return ap


def check_args(args, ap):
    """Check if command line arguments are correct."""
    if args.file and not args.param:
        ap.error("-f requires -p to be entered")


def run_from_cli():
    """ Main method to run the tool. """
    ap = __make_argument_parser()
    args = ap.parse_args()
    check_args(args, ap)

    try:
        first_builder = LlvmKernelBuilder(args.src_version, args.modules_dir,
                                          args.debug)
        if args.module:
            first_mods = {
                args.module: first_builder.build_module(args.module,
                                                        args.rebuild)
            }
        elif args.all_modules:
            first_mods = first_builder.build_all_modules()
        elif args.file:
            first_mods = {args.file: first_builder.build_file(args.file)}
        else:
            first_mods = first_builder.build_modules_with_params(args.rebuild)

        second_builder = LlvmKernelBuilder(args.dest_version, args.modules_dir,
                                           args.debug)
        if args.module:
            second_mods = {
                args.module: second_builder.build_module(args.module,
                                                         args.rebuild)
            }
        elif args.all_modules:
            second_mods = second_builder.build_all_modules()
        elif args.file:
            second_mods = {args.file: second_builder.build_file(args.file)}
        else:
            second_mods = second_builder.build_modules_with_params(
                args.rebuild)

        if args.build_only:
            print "Compiled modules in version {}:".format(args.src_version)
            for mod_name in first_mods.keys():
                print "{} ".format(mod_name)
            print "Compiled modules in version {}:".format(args.dest_version)
            for mod_name in second_mods.keys():
                print "{} ".format(mod_name)
            return 0

        # Set timeout (default is 40s)
        timeout = int(args.timeout) if args.timeout else 40

        print "Computing semantic difference of module parameters"
        print "--------------------------------------------------"
        result = Statistics()
        for mod, first in first_mods.iteritems():
            if mod not in second_mods:
                continue
            second = second_mods[mod]

            # Parse LLVM modules
            first.parse_module()
            second.parse_module()

            if args.param:
                first.set_param(args.param)
                second.set_param(args.param)
            else:
                first.collect_all_parameters()
                second.collect_all_parameters()

            print mod
            for name, param in first.params.iteritems():
                if name not in second.params:
                    continue
                print "  parameter {}".format(name)
                # Compare modules
                stat = modules_diff(first=first, second=second,
                                    glob_var=param,
                                    function=args.function, timeout=timeout,
                                    verbose=args.verbose)
                print "    {}".format(str(stat.overall_result()).upper())
                result.log_result(stat.overall_result(), "{}-{}".format(mod,
                                                                        name))

            # Clean LLVM modules (allow GC to collect the occupied memory)
            first.clean_module()
            second.clean_module()
            LlvmKernelModule.clean_all()
        if args.report_stat:
            print ""
            print "Statistics"
            print "----------"
            result.report()
        return 0

    except Exception as e:
        sys.stderr.write("Error: {}\n".format(str(e)))
        return -1
