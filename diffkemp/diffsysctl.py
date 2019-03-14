"""
Tool for comparing difference in semantics of sysctl configuration options
between two kernel versions.
Compares semantics of invoked proc handler functions and of global variables
affected by the value of the option.
"""
from __future__ import absolute_import

from argparse import ArgumentParser
from diffkemp.config import Config
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder, BuildException
from diffkemp.semdiff.module_diff import diff_all_modules_using_global, \
    modules_diff
from diffkemp.semdiff.result import Result
import sys


def __make_argument_parser():
    """Parsing CLI arguments."""
    ap = ArgumentParser(description="Check semantic equivalence of sysctl \
                        options.")
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("sysctl", help="sysctl option to compare")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    ap.add_argument("-t", "--timeout", help="timeout in seconds for a single \
                    function comparison")
    ap.add_argument("--rebuild", help="force rebuild sources",
                    action="store_true")
    return ap


def run_from_cli():
    """Main method to run the tool"""
    ap = __make_argument_parser()
    args = ap.parse_args()

    result = Result(Result.Kind.NONE, args.src_version, args.dest_version)
    try:
        first_builder = LlvmKernelBuilder(args.src_version, None, debug=True,
                                          rebuild=args.rebuild,
                                          verbose=args.verbose)
        second_builder = LlvmKernelBuilder(args.dest_version, None, debug=True,
                                           rebuild=args.rebuild,
                                           verbose=args.verbose)

        config = Config(first_builder, second_builder, args.timeout, False,
                        False, args.verbose)

        sysctl_mod_first = first_builder.build_sysctl_module(args.sysctl)
        sysctl_mod_second = second_builder.build_sysctl_module(args.sysctl)

        sysctl_list_first = sysctl_mod_first.parse_sysctls(args.sysctl)
        sysctl_list_second = sysctl_mod_second.parse_sysctls(args.sysctl)

        for sysctl in sysctl_list_first:
            if sysctl not in sysctl_list_second:
                continue

            sysctl_res = Result(Result.Kind.NONE, sysctl, sysctl)
            print sysctl
            # Compare the proc handler function
            proc_fun_first = sysctl_mod_first.get_proc_fun(sysctl)
            proc_fun_second = sysctl_mod_second.get_proc_fun(sysctl)
            if proc_fun_first and proc_fun_second:
                proc_result = Result(Result.Kind.NONE,
                                     proc_fun_first, proc_fun_second)
                print "  Comparing proc handler functions"
                if proc_fun_first != proc_fun_second:
                    # Functions with different names are treated as unequal
                    print "    different functions found"
                    proc_result.kind = Result.Kind.NOT_EQUAL
                else:
                    if sysctl_mod_first.is_standard_proc_fun(proc_fun_first):
                        # Standard functions are treated as equal
                        print "    equal syntax (generic)"
                        proc_result.kind = Result.Kind.EQUAL_SYNTAX
                    else:
                        try:
                            mod_first = first_builder.build_file_for_symbol(
                                proc_fun_first)
                            mod_second = second_builder.build_file_for_symbol(
                                proc_fun_second)
                            proc_result = modules_diff(mod_first=mod_first,
                                                       mod_second=mod_second,
                                                       glob_var=None,
                                                       fun=proc_fun_first,
                                                       config=config)
                            proc_result.first.name = proc_fun_first
                            proc_result.second.name = proc_fun_second
                        except BuildException as e:
                            print e
                            proc_result.kind = Result.Kind.ERROR
                sysctl_res.add_inner(proc_result)
            else:
                print "  No proc handler function found"

            # Compare the data global variable affected by the sysctl value
            data_first = sysctl_mod_first.get_data(sysctl)
            data_second = sysctl_mod_second.get_data(sysctl)
            if data_first and data_second:
                print "  Comparing functions using the data variable"
                data_result = diff_all_modules_using_global(
                    glob_first=data_first, glob_second=data_second,
                    config=config)
                sysctl_res.add_inner(data_result)
            else:
                print "  No data variable found"

            result.add_inner(sysctl_res)

        print ""
        print "Statistics"
        print "----------"
        result.report_stat()
        return 0

    except Exception as e:
        sys.stderr.write("Error: {}\n".format(str(e)))
        return -1
