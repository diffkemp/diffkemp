"""Configuration for difference code patterns."""
from subprocess import check_call, CalledProcessError, DEVNULL
from diffkemp.utils import get_llvm_version, get_opt_command
from diffkemp.llvm_ir.optimiser import opt_llvm
import os
import yaml


class UnsupportedCustomPatternError(ValueError):
    """Indicates that a pattern file uses an unsupported format."""
    pass


class CustomPatternConfig:
    """
    Configuration file containing custom difference code patterns
    and global pattern comparison settings.
    Difference patterns can be used during module comparison to prevent
    the reporting of known and desired semantic differences.
    """
    on_parse_failure_options = {"ERROR", "WARN"}

    def __init__(self, path=None, patterns_path=None, clang_append=None,
                 kernel_path=None):
        """
        Create a new difference pattern configuration.
        :param path: Path to the configuration source.
        :param clang_append: Options to append to clang for C patterns.
        :param kernel_path: Path to the kernel source for C patterns.
        """
        self.path = path
        self.patterns_path = patterns_path
        self.clang_append = clang_append
        self.kernel_path = os.path.abspath(
            kernel_path) if kernel_path else None
        self.pattern_files = set()

        self.settings = {
            "on_parse_failure": "ERROR"
        }

        self.llvm_only = True

    @classmethod
    def create_from_file(cls, path, patterns_path=None, clang_append=None,
                         kernel_path=None):
        """
        Creates a new difference pattern configuration from a single pattern
        file or a configuration YAML.
        :param path: Path to the file to load.
        """
        CustomPatternConfig.raise_for_invalid_file(path)
        config = cls(path, patterns_path, clang_append, kernel_path)

        # Process the configuration file based on its extension.
        if CustomPatternConfig.get_extension(path) not in ("ll", "c"):
            config._load_yaml()
        else:
            config.add_pattern(path)

        return config

    @staticmethod
    def raise_for_invalid_file(path):
        """
        Ensures that the given file is readable. Raises an exception if not.
        :param path: Path to the file to check.
        """
        if not os.access(path, os.R_OK):
            raise ValueError(f"unable to read file {path}")

    @staticmethod
    def get_extension(path):
        """
        Retrives the extension from the given file.
        :param path: Path to the file.
        :return: The file extension, excluding the leading dot.
        """
        _, ext = os.path.splitext(path)
        return ext.lower()[1:]

    @staticmethod
    def _get_clang_cpattern_options():
        """
        Returns the options specific to compiling C patterns.
        """
        diffkemp_patterns_path = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "../simpll"))

        llvm_options = ["-S", "-emit-llvm", "-O1",
                        "-Xclang", "-disable-llvm-passes"]
        include_options = [f"-I{diffkemp_patterns_path}",
                           "--include=diffkemp_patterns.h"]
        warning_options = [
            "-Wno-deprecated-non-prototype", "-Wno-macro-redefined"]
        macro_options = ["-D", "DIFFKEMP_CPATTERN"]

        return llvm_options + include_options + warning_options + macro_options

    @staticmethod
    def _get_clang_kernel_options(kernel_path):
        """
        Returns the options to use for compiling C patterns for the Linux
        kernel.
        :param kernel_path: Path to the kernel source.
        """
        kernel_include_paths = [
            "arch/x86/include/", "arch/x86/include/generated/", "include/",
            "arch/x86/include/uapi", "arch/x86/include/generated/uapi",
            "include/uapi", "include/generated/uapi", "."
        ]
        kernel_defines = [
            "__KERNEL__", "__BPF_TRACING__", "__HAVE_BUILTIN_BSWAP16__",
            "__HAVE_BUILTIN_BSWAP32__", "__HAVE_BUILTIN_BSWAP64__",
            "CONFIG_MMU"
        ]
        kernel_includes = ["linux/kconfig.h"]

        include_path_options = [
            f"-I{os.path.join(kernel_path, include)}"
            for include in kernel_include_paths]
        define_options = [f"-D{define}" for define in kernel_defines]
        include_options = [
            f"--include={include}" for include in kernel_includes]

        return include_path_options + define_options + include_options

    def add_pattern(self, path, clang_append=None):
        """
        Tries to add a new pattern file, converting it to LLVM IR if necessary.
        :param path: Path to the pattern file to add.
        """
        if self.patterns_path:
            full_path = os.path.join(self.patterns_path, path)
        else:
            full_path = path

        CustomPatternConfig.raise_for_invalid_file(full_path)
        filetype = CustomPatternConfig.get_extension(full_path)

        # Convert all supported pattern types to LLVM IR.
        # Currently, only LLVM IR patterns are supported.
        if filetype == "ll":
            self._add_llvm_pattern(full_path)
        elif filetype == "c":
            self._add_c_pattern(full_path, clang_append)
        else:
            # Inform about unexpected file extensions.
            self._on_parse_failure(f"{full_path}: {filetype} pattern files "
                                   f"are not supported")

    def _load_yaml(self):
        """Load the pattern configuration from its YAML representation."""
        with open(self.path, "r") as config_yaml:
            yaml_dict = yaml.safe_load(config_yaml)

        if "patterns" not in yaml_dict:
            return

        # Process global pattern settings.
        if "on_parse_failure" in yaml_dict:
            on_parse_failure = yaml_dict["on_parse_failure"]
            if on_parse_failure not in self.on_parse_failure_options:
                raise ValueError("Invalid on-failure action type")
            self.settings["on_parse_failure"] = on_parse_failure

        for pattern in yaml_dict["patterns"]:
            if "clang_append" in yaml_dict:
                clang_append = yaml_dict["clang_append"].get(pattern)
            else:
                clang_append = None
            self.add_pattern(pattern, clang_append=clang_append)

    def _add_llvm_pattern(self, path):
        """
        Tries to add a new LLVM IR pattern file.
        :param path: Path to the LLVM IR pattern file to add.
        """
        try:
            # Ensure that the pattern is valid.
            # Note: Possibly due to a bug in `opt`, some valid LLVM IR
            # files with opaque pointers are not accepted when using
            # LLVM 15 without manually enabling opaque pointers.
            call_opt = get_opt_command([("verify", "module")], path, False)
            if 17 > get_llvm_version() >= 15:
                call_opt.append("-opaque-pointers=1")
            check_call(call_opt, stdout=DEVNULL)
        except CalledProcessError:
            self._on_parse_failure(f"failed to parse pattern file {path}")

        self.pattern_files.add(path)

    def _add_c_pattern(self, path, clang_append=None):
        """
        Tries to add a new C pattern file.
        :param path: Path to the C pattern file to add.
        """
        path_dir = os.path.dirname(path)
        llvm_path = path.removesuffix(".c") + ".ll"
        name = os.path.basename(path)
        llvm_name = os.path.basename(llvm_path)

        try:
            call_clang = ["clang", name, "-o", llvm_name]
            call_clang.extend(self._get_clang_cpattern_options())
            if self.kernel_path:
                call_clang.extend(
                    self._get_clang_kernel_options(self.kernel_path))
            if self.clang_append:
                call_clang.extend(self.clang_append)
            if clang_append:
                call_clang.extend(clang_append)
            check_call(call_clang, cwd=path_dir)

            opt_llvm(llvm_path)

        except CalledProcessError:
            self._on_parse_failure(f"failed to parse pattern file {path}")
        self.pattern_files.add(path)

    def _on_parse_failure(self, message):
        """
        Inform user about a pattern parse failure.
        :param message: Message for the user.
        """
        if self.settings["on_parse_failure"] == "ERROR":
            raise UnsupportedCustomPatternError(message)
        elif self.settings["on_parse_failure"] == "WARN":
            print(message)
