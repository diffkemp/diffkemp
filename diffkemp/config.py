"""Configuration of the tool."""
import os


class ConfigException(Exception):
    pass


class Config:
    def __init__(self, builder_first, builder_second,
                 timeout, print_diff, control_flow_only, verbosity,
                 do_not_link, semdiff_tool):
        """
        Store configuration of DiffKemp
        :param builder_first: Builder for the first kernel.
        :param builder_second: Builder for the second kernel.
        :param timeout: Timeout.
        :param print_diff: Only perform the syntax diff.
        :param control_flow_only: Check only for control-flow differences.
        :param verbosity: Verbosity level (currently boolean).
        :param do_not_link: Do not link functions from other sources
        :param semdiff_tool: Tool to use for semantic diff
        """
        self.builder_first = builder_first
        self.builder_second = builder_second
        # Default timeout is 40s
        self.timeout = int(timeout) if timeout else 40
        self.print_diff = print_diff
        self.control_flow_only = control_flow_only
        self.verbosity = verbosity
        self.do_not_link = do_not_link

        # Semantic diff tool configuration
        self.semdiff_tool = semdiff_tool
        if semdiff_tool == "llreve":
            if not os.path.isfile("build/llreve/reve/reve/llreve"):
                raise ConfigException("LLReve not built, try to re-run CMake \
                                       with -DBUILD_LLREVE=ON")
        elif semdiff_tool is not None:
            raise ConfigException("Unsupported semantic diff tool")
