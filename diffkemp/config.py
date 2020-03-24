"""Configuration of the tool."""
import os


class ConfigException(Exception):
    pass


class Config:
    def __init__(self, snapshot_first, snapshot_second, show_diff,
                 control_flow_only, print_asm_diffs, verbosity, use_ffi,
                 semdiff_tool):
        """
        Store configuration of DiffKemp
        :param snapshot_first: First snapshot representation.
        :param snapshot_second: Second snapshot representation.
        :param show_diff: Only perform the syntax diff.
        :param control_flow_only: Check only for control-flow differences.
        :param verbosity: Verbosity level (currently boolean).
        :param semdiff_tool: Tool to use for semantic diff
        """
        self.snapshot_first = snapshot_first
        self.snapshot_second = snapshot_second
        self.show_diff = show_diff
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
