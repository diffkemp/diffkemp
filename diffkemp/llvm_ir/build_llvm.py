"""
Building kernel module into LLVM IR.
"""
import os
from subprocess import CalledProcessError, check_call, check_output


class BuildException(Exception):
    pass


class LlvmKernelBuilder:
    """
    Building kernel modules into LLVM IR.
    """
    def __init__(self, kernel_dir):
        self.kernel_dir = os.path.abspath(kernel_dir)
        # GCC compiler header
        self.gcc_compiler_header = os.path.join(self.kernel_dir,
                                                "include/linux/compiler-gcc.h")
        # Prepare kernel so that it can be built with Clang
        self.initialize()
        # Caching built modules to reuse them
        self.built_modules = dict()

    def initialize(self):
        """
        Prepare kernel so that it can be compiled into LLVM IR.
        """
        self._disable_asm_goto()

    def finalize(self):
        """Restore the kernel state."""
        self._enable_asm_goto()

    def _disable_asm_goto(self):
        """
        Transform 'asm goto(x)' command into 'asm("goto(x)")'.
        This is because LLVM does not support asm goto yet.
        The original GCC compiler header is kept since it is needed when
        compiling the kernel using GCC.
        """
        command = ["sed", "-i", "s/asm goto(x)/asm (\"goto(\" #x \")\")/g",
                   self.gcc_compiler_header]
        try:
            check_call(command)
        except CalledProcessError:
            pass

    def _enable_asm_goto(self):
        """
        Restore the original 'asm goto' semantics.
        This is done by restoring the GCC header from backup.
        """
        command = ["sed", "-i", "s/asm (\"goto(\" #x \")\")/asm goto(x)/g",
                   self.gcc_compiler_header]
        try:
            check_call(command)
        except CalledProcessError:
            pass

    @staticmethod
    def _strip_bash_quotes(gcc_param):
        """
        Remove quotes from gcc_param that represents a part of a shell command.
        """
        if "\'" in gcc_param:
            return gcc_param.replace("\'", "")
        else:
            return gcc_param.replace("\"", "")

    @staticmethod
    def gcc_to_llvm(gcc_command):
        """
        Convert GCC command to corresponding Clang command for compiling source
        into LLVM IR.
        :param gcc_command: GCC command to convert.
        :return Corresponding Clang command.
        """
        output_file = None
        command = ["clang", "-S", "-emit-llvm", "-O1", "-Xclang",
                   "-disable-llvm-passes", "-g", "-fdebug-macro"]
        for param in gcc_command.split():
            if (param == "gcc" or
                    (param.startswith("-W") and "-MD" not in param) or
                    param.startswith("-f") or
                    param.startswith("-m") or
                    param.startswith("-O") or
                    param == "-DCC_HAVE_ASM_GOTO" or
                    param == "-g" or
                    param == "-o" or
                    param.endswith(".o")):
                continue

            # Output name is given by replacing .c by .ll in source name
            if param.endswith(".c"):
                output_file = "{}.ll".format(param[:-2])

            command.append(LlvmKernelBuilder._strip_bash_quotes(param))
        if output_file is None:
            raise BuildException("Build error: gcc command not compiling C \
                                 source found")
        command.extend(["-o", output_file])
        return command

    @staticmethod
    def ld_to_llvm(ld_command):
        """
        Convert ld command into llvm-link command to link multiple LLVM IR
        files into one file.
        :param ld_command: Command to convert
        :return Corresponding llvm-link command.
        """
        command = ["llvm-link", "-S"]
        for param in ld_command.split():
            if param.endswith(".o"):
                command.append("{}.ll".format(param[:-2]))
            elif param == "-o":
                command.append(param)
        return command

    @staticmethod
    def kbuild_to_llvm_commands(commands, module_name):
        llvm_commands = []
        for c in commands:
            command = c.lstrip()
            if (command.startswith("gcc") and
                    "{}.mod".format(module_name) not in command):
                llvm_commands.append(LlvmKernelBuilder.gcc_to_llvm(command))
            elif (command.startswith("ld") and
                  "{}.ko".format(module_name) not in command):
                llvm_commands.append(LlvmKernelBuilder.ld_to_llvm(command))
        return llvm_commands

    @staticmethod
    def opt_llvm(llvm_file):
        """
        Optimise LLVM IR using 'opt' tool. LLVM passes are chosen based on the
        command that created the file being optimized.
        For compiled files (using clang), run basic simplification passes.
        For linked files (using llvm-link), run -constmerge to remove
        duplicate constants that might have come from linked files.
        """
        opt_command = ["opt", "-S", llvm_file, "-o", llvm_file]
        opt_command.extend(["-lowerswitch", "-mem2reg", "-loop-simplify",
                            "-simplifycfg", "-gvn", "-dce", "-constmerge"])
        try:
            with open(os.devnull, "w") as devnull:
                check_call(opt_command, stderr=devnull)
        except CalledProcessError:
            raise BuildException("Running opt failed")

    @staticmethod
    def _clean_object(obj):
        """Clean an object file"""
        if os.path.isfile(obj):
            os.remove(obj)

    @staticmethod
    def _extract_gcc_command(command):
        """
        Extract a single gcc or ld command from a list of commands separated
        by ;.
        """
        for c in command.split(";"):
            if c.lstrip().startswith("gcc") or c.lstrip().startswith("ld"):
                return c.lstrip()
        return None

    @staticmethod
    def _extract_gcc_command_list(commands):
        """Extract all gcc and ld commands from a list of commands."""
        result = []
        for c in commands:
            command = LlvmKernelBuilder._extract_gcc_command(c)
            if command:
                result.append(command)
        return result

    @staticmethod
    def _get_build_object(command):
        """Get name of the object file built by the command."""
        return command[command.index("-o") + 1]

    @staticmethod
    def _get_build_source(command):
        """Get name of the object file built by the command."""
        return command[command.index("-c") + 1]

    def kbuild_object_command(self, object_file):
        """
        Check which command would be run by KBuild to build the given object
        file.
        The command used is `make V=1 --just-print path/to/object.o`
        :returns GCC command used for the compilation. This is the last
                 command starting with 'gcc' that was run by make
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_dir)
        self._clean_object(object_file)
        with open(os.devnull, "w") as stderr:
            try:
                output = check_output(
                    ["make", "V=1", object_file,
                     "--just-print"], stderr=stderr).decode("utf-8")
            except CalledProcessError:
                raise BuildException("Error compiling {}".format(object_file))
            finally:
                os.chdir(cwd)

        for c in reversed(output.splitlines()):
            command = self._extract_gcc_command(c)
            if command:
                return command
        raise BuildException("Compiling {} did not run a gcc command".format(
            object_file))

    def kbuild_module_commands(self, mod_dir, mod_name):
        """
        Build a kernel module using Kbuild.
        The command used is `make V=1 M=/path/to/mod module.ko`
        First, tries to build module with the given name, next tries to replace
        '_' by '-'.
        :returns Name used in the .ko file (possibly with underscores replaced
                 by dashes).
                 List of commands that were used to compile and link files in
                 the module.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_dir)

        file_name = mod_name
        command = ["make", "--just-print", "V=1", "M={}".format(mod_dir),
                   "{}.ko".format(mod_name)]

        try:
            output = check_output(command).decode("utf-8")
            return file_name, \
                self._extract_gcc_command_list(output.splitlines())
        except CalledProcessError as e:
            if e.returncode == 2:
                # If the target does not exist, replace "_" by "-" and try
                # again.
                file_name = file_name.replace("_", "-")
                command[4] = "{}.ko".format(file_name)
                try:
                    output = check_output(command).decode("utf-8")
                    return file_name, \
                        self._extract_gcc_command_list(output.splitlines())
                except CalledProcessError:
                    raise BuildException(
                        "Could not build module {}".format(mod_name))
                finally:
                    os.chdir(cwd)
        finally:
            os.chdir(cwd)
        raise BuildException("Could not build module {}".format(mod_name))

    def build_source_to_llvm(self, source_file, llvm_file):
        """
        Build C source file into LLVM IR.
        Gets the Kbuild command that is used for building an object file,
        transforms it into the corresponding Clang command, and runs it.
        :param source_file: C source to build
        :param llvm_file: Target LLVM IR file to create
        """
        if (not os.path.isfile(llvm_file) or os.path.getmtime(llvm_file) <
                os.path.getmtime(source_file)):
            cwd = os.getcwd()
            os.chdir(self.kernel_dir)

            name = source_file[:-2]
            try:
                # Get GCC command for building the .o file
                command = self.kbuild_object_command("{}.o".format(name))
                # Convert the GCC command to a corresponding Clang command
                command = self.gcc_to_llvm(command)
                # Run the Clang command
                with open(os.devnull, "w") as stderr:
                    try:
                        check_call(command, stderr=stderr)
                    except CalledProcessError:
                        os.chdir(cwd)
                        raise BuildException(
                            "Could not build {}".format(llvm_file))
                # Run opt with selected optimisations
                self.opt_llvm(llvm_file)
            except BuildException:
                raise
            finally:
                os.chdir(cwd)

    def build_kernel_mod_to_llvm(self, mod_dir, mod_name):
        """
        Build a kernel module into LLVM IR.
        This is done by retrieving commands that would be run by KBuild to
        build the given module, transforming them into corresponding clang/LLVM
        commands, and running them.
        :param mod_dir: Kernel module directory.
        :param mod_name: Kernel module name.
        :return: Name of the LLVM IR file built.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_dir)
        try:
            file_name, gcc_commands = self.kbuild_module_commands(mod_dir,
                                                                  mod_name)
            llvm_commands = self.kbuild_to_llvm_commands(gcc_commands,
                                                         file_name)
            with open(os.devnull, "w") as stderr:
                built = False
                for c in llvm_commands:
                    if c[0] == "clang":
                        src = self._get_build_source(c)
                        obj = self._get_build_object(c)
                        if (not os.path.isfile(obj) or
                                os.path.getmtime(obj) < os.path.getmtime(src)):
                            built = True
                            check_call(c, stderr=stderr)
                    elif c[0] == "llvm-link":
                        obj = self._get_build_object(c)
                        if not os.path.isfile(obj) or built:
                            check_call(c, stderr=stderr)
            llvm_file = os.path.join(mod_dir, "{}.ll".format(file_name))
            self.opt_llvm(llvm_file)
            return llvm_file
        except CalledProcessError:
            raise BuildException("Could not build {}".format(mod_name))
        except BuildException:
            raise
        finally:
            os.chdir(cwd)
