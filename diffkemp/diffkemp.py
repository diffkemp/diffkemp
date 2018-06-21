from __future__ import absolute_import

from argparse import ArgumentParser
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder
from diffkemp.semdiff.module_diff import modules_diff, Statistics
import sys


def __make_argument_parser():
    """ Parsing arguments. """
    ap = ArgumentParser()
    ap.add_argument("modules_dir")
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("-m", "--module", help="analyse only chosen module")
    ap.add_argument("-p", "--param", help="analyse only chosen parameter")
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
    ap.add_argument("--default-value",
            help="print parameter default value (in LLVM IR form)",
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
            second_mods = second_builder.build_modules_with_params(
                args.rebuild)

        if args.default_value:
            for module_name, module_src in first_mods.iteritems():
                module_dst = second_mods.get(module_name)
                if not module_dst:
                    continue

                module_src.collect_all_parameters()
                module_dst.collect_all_parameters()

                print module_name
                if args.param:
                    param_src = module_src.params.get(args.param)
                    param_dst = module_dst.params.get(args.param)
                    if param_src and param_dst:
                        print "{}: {}".format(args.src_version,
                                param_src.default_value)
                        print "{}: {}".format(args.dest_version,
                                param_dst.default_value)
                    return 0

                for param_name, param in module_src.params.iteritems():
                    if module_dst.params.get(param_name):
                        print "{}: {}".format(args.src_version, param.default_value)
                        print "{}: {}".format(args.dest_version,
                                module_dst.params[param_name].default_value)
            return 0

        if args.build_only:
            print "Compiled modules in version {}:".format(args.src_version)
            for mod_name in first_mods.keys():
                print "{} ".format(mod_name)
            print "Compiled modules in version {}:".format(args.dest_version)
            for mod_name in second_mods.keys():
                print "{} ".format(mod_name)
            return 0

        print "Computing semantic difference of module parameters"
        print "--------------------------------------------------"
        result = Statistics()
        for mod, first in first_mods.iteritems():
            if mod not in second_mods:
                continue
            second = second_mods[mod]

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
                stat = modules_diff(first, second, param.varname, args.verbose)
                print "    {}".format(str(stat.overall_result()).upper())
                result.log_result(stat.overall_result(), "{}-{}".format(mod,
                                                                        name))
        if args.report_stat:
            print ""
            print "Statistics"
            print "----------"
            result.report()
        return 0

    except Exception as e:
        sys.stderr.write("Error: {}\n".format(str(e)))
        return -1
