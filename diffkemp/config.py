"""Configuration of the tool."""
from diffkemp.semdiff.pattern_config import PatternConfig
from diffkemp.snapshot import Snapshot
from diffkemp.utils import get_llvm_version
import os
import sys
import errno


class ConfigException(Exception):
    pass


class Config:
    def __init__(
        self,
        snapshot_first=None,
        snapshot_second=None,
        show_diff=True,
        full_diff=False,
        output_llvm_ir=False,
        pattern_config=None,
        control_flow_only=False,
        print_asm_diffs=False,
        verbosity=0,
        use_ffi=False,
        semdiff_tool=None,
    ):
        """
        Store configuration of DiffKemp
        :param snapshot_first: First snapshot representation.
        :param snapshot_second: Second snapshot representation.
        :param show_diff: Only perform the syntax diff.
        :param full_diff: Evaluate all syntactic differences.
        :param output_llvm_ir: Output each simplified module into a file.
        :param pattern_config: Valid difference patterns configuration.
        :param print_asm_diffs: Print assembly differences.
        :param control_flow_only: Check only for control-flow differences.
        :param verbosity: Verbosity level (currently boolean).
        :param use_ffi: Use Python FFI to call SimpLL.
        :param semdiff_tool: Tool to use for semantic diff.
        """

        self.snapshot_first = snapshot_first
        self.snapshot_second = snapshot_second
        self.show_diff = show_diff
        self.full_diff = full_diff
        self.output_llvm_ir = output_llvm_ir
        self.pattern_config = pattern_config
        self.control_flow_only = control_flow_only
        self.print_asm_diffs = print_asm_diffs
        self.verbosity = verbosity
        self.use_ffi = use_ffi

        # Semantic diff tool configuration
        self.semdiff_tool = semdiff_tool
        if semdiff_tool == "llreve":
            self.timeout = 10
            if not os.path.isfile("build/llreve/reve/reve/llreve"):
                raise ConfigException("LLReve not built, try to re-run CMake \
                                       with -DBUILD_LLREVE=ON")
        elif semdiff_tool is not None:
            raise ConfigException("Unsupported semantic diff tool")

    @classmethod
    def from_args(cls, args):
        """
        Create the configuration from command line arguments.
        :param args: Command line arguments
        """
        # Parse both the new and the old snapshot.
        snapshot_first = Snapshot.load_from_dir(args.snapshot_dir_old)
        snapshot_second = Snapshot.load_from_dir(args.snapshot_dir_new)

        # Check if snapshot LLVM versions are compatible with
        # the current version.
        llvm_version = get_llvm_version()

        if llvm_version != snapshot_first.llvm_version:
            sys.stderr.write(
                "Error: old snapshot was built with LLVM {}, "
                "current version is LLVM {}.\n".format(
                    snapshot_first.llvm_version, llvm_version
                )
            )
            sys.exit(errno.EINVAL)

        if llvm_version != snapshot_second.llvm_version:
            sys.stderr.write(
                "Error: new snapshot was built with LLVM {}, "
                "current version is LLVM {}.\n".format(
                    snapshot_second.llvm_version, llvm_version
                )
            )
            sys.exit(errno.EINVAL)

        if args.function:
            snapshot_first.filter([args.function])
            snapshot_second.filter([args.function])

        # Transform difference pattern files into an LLVM IR
        # based configuration.
        if args.patterns:
            pattern_config = PatternConfig.create_from_file(args.patterns)
        else:
            pattern_config = None

        return cls(
            snapshot_first=snapshot_first,
            snapshot_second=snapshot_second,
            show_diff=not args.no_show_diff or args.show_diff,
            full_diff=args.full_diff,
            pattern_config=pattern_config,
            control_flow_only=args.control_flow_only,
            output_llvm_ir=args.output_llvm_ir,
            print_asm_diffs=args.print_asm_diffs,
            verbosity=args.verbosity,
            use_ffi=args.use_ffi,
            semdiff_tool=args.semdiff_tool,
        )
