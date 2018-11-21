from __future__ import absolute_import

from argparse import ArgumentParser
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder, LlvmKernelModule
from diffkemp.semdiff.function_diff import Result
from diffkemp.semdiff.module_diff import modules_diff, Statistics
import sys


def __make_argument_parser():
    """Parsing CLI arguments."""
    ap = ArgumentParser()
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("-f", "--function", help="specify function to compare")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    ap.add_argument("-t", "--timeout", help="timeout in seconds for a single \
                    function comparison")
    ap.add_argument("--report-stat", help="report statistics of the analysis",
                    action="store_true")
    ap.add_argument("--syntax-diff", help="for functions that are \
                    syntactically different, show result of diff",
                    action="store_true")
    ap.add_argument("--control-flow-only", help="see only control-flow \
                    differences", action="store_true")
    return ap


def run_from_cli():
    """Main method to run the tool"""
    ap = __make_argument_parser()
    args = ap.parse_args()

    try:
        # Prepare kernels
        first_builder = LlvmKernelBuilder(args.src_version, None, debug=True,
                                          verbose=not args.syntax_diff)
        if args.function:
            kabi_funs_first = [args.function]
        else:
            kabi_funs_first = first_builder.get_kabi_whitelist()

        second_builder = LlvmKernelBuilder(args.dest_version, None, debug=True,
                                           verbose=not args.syntax_diff)
        if args.function:
            kabi_funs_second = [args.function]
        else:
            kabi_funs_second = second_builder.get_kabi_whitelist()

        timeout = int(args.timeout) if args.timeout else 40

        print "Computing semantic difference of functions on kabi whitelist"
        print "------------------------------------------------------------"
        result = Statistics()
        for f in kabi_funs_first:
            if f not in kabi_funs_second:
                continue

            if not args.syntax_diff:
                print f

            try:
                # Find source files with function definitions and build them
                mod_first = first_builder.build_file_for_function(f)
                mod_second = second_builder.build_file_for_function(f)

                # Compare functions semantics
                stat = modules_diff(mod_first, mod_second, None, timeout, f,
                                    args.syntax_diff, args.control_flow_only,
                                    args.verbose)
                res = stat.overall_result()
                result.log_result(res, f)

                if not args.syntax_diff:
                    print "  {}".format(str(stat.overall_result()).upper())

                # Clean LLVM modules (allow GC to collect the occupied memory)
                mod_first.clean_module()
                mod_second.clean_module()
                LlvmKernelModule.clean_all()
            except Exception as e:
                if not args.syntax_diff:
                    sys.stderr.write("  Error: {}\n".format(str(e)))
                result.log_result(Result.ERROR, f)

        print ""
        print "Statistics"
        print "----------"
        result.report()
        return 0

    except Exception as e:
        sys.stderr.write("Error: {}\n".format(str(e)))
        return -1
