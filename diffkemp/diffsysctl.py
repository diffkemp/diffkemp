"""
Tool for comparing difference in semantics of sysctl configuration options
between two kernel versions.
Compares semantics of invoked proc handler functions and of global variables
affected by the value of the option.
"""
from __future__ import absolute_import

from argparse import ArgumentParser
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder, BuildException
from diffkemp.semdiff.function_diff import Result
from diffkemp.semdiff.module_diff import modules_diff, Statistics
import sys


def __make_argument_parser():
    """Parsing CLI arguments."""
    ap = ArgumentParser()
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("sysctl", help="sysctl parameter to compare")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    ap.add_argument("-t", "--timeout", help="timeout in seconds for a single \
                    function comparison")
    return ap


def run_from_cli():
    """Main method to run the tool"""
    ap = __make_argument_parser()
    args = ap.parse_args()

    timeout = int(args.timeout) if args.timeout else 40

    result = Statistics()
    try:
        first_builder = LlvmKernelBuilder(args.src_version, None, debug=True,
                                          verbose=args.verbose)
        second_builder = LlvmKernelBuilder(args.dest_version, None, debug=True,
                                           verbose=args.verbose)

        sysctl_mod_first = first_builder.build_sysctl_module(args.sysctl)
        sysctl_mod_second = second_builder.build_sysctl_module(args.sysctl)

        # Compare the proc handler function
        proc_fun_first = sysctl_mod_first.get_proc_fun(args.sysctl)
        proc_fun_second = sysctl_mod_second.get_proc_fun(args.sysctl)
        if proc_fun_first and proc_fun_second:
            print "Comparing proc handler functions"
            if proc_fun_first != proc_fun_second:
                # Functions with different names are treated as unequal
                print "  different functions found"
                result.log_result(Result.NOT_EQUAL, proc_fun_first)
            else:
                if sysctl_mod_first.is_standard_proc_fun(proc_fun_first):
                    # Standard functions are treated as equal
                    print "  equal"
                    result.log_result(Result.EQUAL, proc_fun_first)
                else:
                    try:
                        mod_first = first_builder.build_file_for_function(
                            proc_fun_first)
                        mod_second = second_builder.build_file_for_function(
                            proc_fun_second)
                        stat = modules_diff(mod_first, mod_second, None,
                                            timeout,
                                            proc_fun_first,
                                            verbose=args.verbose)
                        result.log_result(stat.overall_result(),
                                          proc_fun_first)
                    except BuildException as e:
                        print e
                        result.log_result(Result.ERROR, proc_fun_first)

        else:
            print "No proc handler function found"

        # Compare the data global variable affected by the sysctl value
        data_first = sysctl_mod_first.get_data(args.sysctl)
        data_second = sysctl_mod_second.get_data(args.sysctl)
        if data_first and data_second:
            print "Comparing functions using the data variable"
            if data_first != data_second:
                # Variables with different names are treated as unequal
                print "  different data variables found"
                result.log_result(Result.NOT_EQUAL, proc_fun_first)
            else:
                print "  Variable {}".format(data_first)
                srcs_first = first_builder.source.find_srcs_using_symbol(
                    data_first)
                srcs_second = second_builder.source.find_srcs_using_symbol(
                    data_second)
                # Compare all sources containing functions using the variable
                for src in srcs_first:
                    if src not in srcs_second:
                        result.log_result(Result.NOT_EQUAL, args.sysctl)
                    try:
                        mod_first = first_builder.build_file(src)
                        mod_second = second_builder.build_file(src)
                        stat = modules_diff(mod_first, mod_second, data_first,
                                            timeout, None,
                                            verbose=args.verbose)
                        result.log_result(stat.overall_result(), data_first)
                    except BuildException as e:
                        print e
                        result.log_result(Result.ERROR, data_first)
        else:
            print "No data variable found"

        print result.overall_result()
        return 0

    except Exception as e:
        sys.stderr.write("Error: {}\n".format(str(e)))
        return -1
