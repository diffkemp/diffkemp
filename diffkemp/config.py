"""Configuration of the tool."""


class Config:
    def __init__(self, builder_first, builder_second,
                 timeout, syntax_only, control_flow_only, verbosity):
        """
        Store configuration of DiffKemp
        :param builder_first: Builder for the first kernel.
        :param builder_second: Builder for the second kernel.
        :param timeout: Timeout.
        :param syntax_only: Only perform the syntax diff.
        :param control_flow_only: Check only for control-flow differences.
        :param verbosity: Verbosity level (currently boolean).
        """
        self.builder_first = builder_first
        self.builder_second = builder_second
        # Default timeout is 40s
        self.timeout = int(timeout) if timeout else 40
        self.syntax_only = syntax_only
        self.control_flow_only = control_flow_only
        self.verbosity = verbosity
