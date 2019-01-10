from __future__ import absolute_import

from argparse import ArgumentParser
from diffkemp.llvm_ir.build_llvm import LlvmKernelBuilder, LlvmKernelModule
from diffkemp.llvm_ir.kernel_module import KernelParam
from diffkemp.semdiff.module_diff import diff_all_modules_using_global, \
    functions_diff
from diffkemp.semdiff.result import Result
import sys


def __make_argument_parser():
    """Parsing CLI arguments."""
    ap = ArgumentParser(description="Check semantic equivalence of KABI \
                        whitelist functions.")
    ap.add_argument("src_version")
    ap.add_argument("dest_version")
    ap.add_argument("-f", "--function", help="specify a function to compare")
    ap.add_argument("-v", "--verbose", help="increase output verbosity",
                    action="store_true")
    ap.add_argument("-t", "--timeout", help="timeout in seconds for a single \
                    function comparison")
    ap.add_argument("--report-stat", help="report statistics of the analysis",
                    action="store_true")
    ap.add_argument("--syntax-diff", help="for functions that are \
                    semantically different, show the result of diff",
                    action="store_true")
    ap.add_argument("--control-flow-only", help="see only control-flow \
                    differences", action="store_true")
    ap.add_argument("--include-globals", help="include also whitelists that \
                    are global variables", action="store_true")
    return ap


def run_from_cli():
    """Main method to run the tool"""
    ap = __make_argument_parser()
    args = ap.parse_args()

    try:
        # Prepare kernels
        first_builder = LlvmKernelBuilder(args.src_version, None, debug=True,
                                          verbose=False)
        if args.function:
            kabi_funs_first = [args.function]
        else:
            kabi_funs_first = first_builder.get_kabi_whitelist()

        second_builder = LlvmKernelBuilder(args.dest_version, None, debug=True,
                                           verbose=False)
        if args.function:
            kabi_funs_second = [args.function]
        else:
            kabi_funs_second = second_builder.get_kabi_whitelist()

        timeout = int(args.timeout) if args.timeout else 40

        print "Computing semantic difference of functions on kabi whitelist"
        print "------------------------------------------------------------"
        result = Result(Result.Kind.NONE, args.src_version, args.dest_version)
        for f in kabi_funs_first:
            if f not in kabi_funs_second:
                continue
            try:
                # Find source files with function definitions and build them
                mod_first = first_builder.build_file_for_symbol(f)
                mod_second = second_builder.build_file_for_symbol(f)

                fun_result = None
                if mod_first.has_function(f):
                    if not args.syntax_diff:
                        print f
                    # Compare functions semantics
                    fun_result = functions_diff(
                        first=mod_first.llvm, second=mod_second.llvm,
                        funFirst=f, funSecond=f,
                        glob_var=None,
                        timeout=timeout,
                        syntax_only=args.syntax_diff,
                        control_flow_only=args.control_flow_only,
                        verbose=args.verbose)
                    fun_result.first.name = f
                    fun_result.second.name = f
                elif args.include_globals:
                    # f is a global variable: compare semantics of all
                    # functions using the variable
                    if not args.syntax_diff:
                        print "{} (global variable)".format(f)
                        print "Comparing all functions using {}".format(f)
                    globvar = KernelParam(f)
                    fun_result = diff_all_modules_using_global(
                        first_builder=first_builder,
                        second_builder=second_builder,
                        glob_first=globvar,
                        glob_second=globvar,
                        timeout=timeout,
                        syntax_only=args.syntax_diff,
                        control_flow_only=args.control_flow_only,
                        verbose=args.verbose)

                if fun_result is not None:
                    result.add_inner(fun_result)
                    if args.syntax_diff:
                        if fun_result.kind == Result.Kind.NOT_EQUAL:
                            print "{}:".format(f)
                            for called_res in fun_result.inner.itervalues():
                                print "  {} differs:".format(
                                    called_res.first.name)
                                print "  {{{"
                                print "    Callstack ({}):".format(
                                    args.src_version)
                                print called_res.first.callstack.replace(
                                    first_builder.kernel_path + "/", "")
                                print
                                print "    Callstack ({}):".format(
                                    args.dest_version)
                                print called_res.second.callstack.replace(
                                    second_builder.kernel_path + "/", "")
                                print
                                print "    Diff:"
                                print called_res.diff
                                print "  }}}"
                    else:
                        print "  {}".format(str(fun_result.kind).upper())

            except Exception as e:
                if not args.syntax_diff:
                    sys.stderr.write("  ERROR: {}\n".format(str(e)))
                result.add_inner(Result(Result.Kind.ERROR, f, f))
            finally:
                # Clean LLVM modules (allow GC to collect the occupied memory)
                try:
                    mod_first.clean_module()
                    mod_second.clean_module()
                except NameError:
                    pass
                LlvmKernelModule.clean_all()

        print ""
        print "Statistics"
        print "----------"
        result.report_stat()
        return 0

    except Exception as e:
        sys.stderr.write("Error: {}\n".format(str(e)))
        return -1
