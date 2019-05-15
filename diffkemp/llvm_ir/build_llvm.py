"""
Building kernel module into LLVM IR.
Functions for downloading, configuring, compiling, and linkning kernel modules
into LLVM IR.
"""

import os
from diffkemp.llvm_ir.kernel_module import LlvmKernelModule
from diffkemp.llvm_ir.kernel_source import KernelSource, \
    SourceNotFoundException
from diffkemp.llvm_ir.module_analyser import *
from diffkemp.llvm_ir.llvm_sysctl_module import LlvmSysctlModule
from distutils.version import StrictVersion
from progressbar import ProgressBar, Percentage, Bar
import shutil
from subprocess import CalledProcessError, call, check_call, check_output
from subprocess import Popen, PIPE
from urllib import urlretrieve
from tempfile import mkdtemp
from socket import gethostbyname, error as socket_error


class BuildException(Exception):
    pass


# Progress bar for downloading
pbar = None


def show_progress(count, block_size, total_size):
    global pbar
    if pbar is None:
        pbar = ProgressBar(maxval=total_size, widgets=[Percentage(), Bar()])
        pbar.start()

    downloaded = count * block_size
    if downloaded < total_size:
        pbar.update(downloaded)
    else:
        pbar.finish()
        pbar = None


class LlvmKernelBuilder:
    """
    Building kernel modules into LLVM IR.
    Contains methods to automatically:
        - download kernel sources
        - configure kernel
        - build modules using GCC
        - build modules using Clang into LLVM IR
        - collect module parameters with default values
    """

    # Base path to kernel sources
    kernel_base_dir = "kernel"

    # Name of the kabi whitelist file
    kabi_whitelist_file = "kabi_whitelist_x86_64"

    def __init__(self, kernel_version, modules_dir, debug=False, rebuild=False,
                 verbose=True):
        self.kernel_base_path = os.path.abspath(self.kernel_base_dir)
        if not os.path.isdir(self.kernel_base_path):
            os.mkdir(self.kernel_base_path)
        self.kernel_version = kernel_version
        self.kernel_path = os.path.join(self.kernel_base_path,
                                        "linux-{}".format(self.kernel_version))
        self.modules_dir = modules_dir or "."
        self.modules_path = os.path.join(self.kernel_path, self.modules_dir)
        self.debug = debug
        self.kabi_tarname = None
        self.kabi_whitelist = os.path.join(self.kernel_path,
                                           self.kabi_whitelist_file)
        self.verbose = verbose
        self.rebuild = rebuild

        self.configfile = None
        self.gcc_compiler_header = os.path.join(self.kernel_path,
                                                "include/linux/compiler-gcc.h")
        self.orig_gcc_compiler_header = self.gcc_compiler_header + ".orig"
        self._prepare_kernel()
        self.source = KernelSource(self.kernel_path)

        # Caching built modules so that we can reuse them
        self.built_modules = dict()

    def _prepare_kernel(self):
        """
        Download and configure kernel if kernel directory does not exist.
        """
        print "Kernel version {}".format(self.kernel_version)
        print "-------------------"
        if not os.path.isdir(self.kernel_path):
            self._get_kernel_source()
            self._symlink_gcc_header(7)
            self._configure_kernel()
            self._autogen_time_headers()
            self._disable_asm_goto()
            if self.kabi_tarname:
                self._extract_kabi_whitelist()

    def _get_kernel_source(self):
        """Download the sources of the required kernel version."""
        # Deduce source where kernel will be downloaded from.
        # The choice is done based on version string, if it has release part
        # (e.g. 3.10.0-655) it must be downloaded from Brew (StrictVersion will
        # raise exception on such version string). If Brew is unavailable, use
        # the CentOS Git.
        try:
            StrictVersion(self.kernel_version)
            tarname = self._get_kernel_tar_from_upstream()
        except ValueError:
            try:
                gethostbyname("download.eng.bos.redhat.com")
                tarname = self._get_kernel_tar_from_brew()
            except socket_error:
                tarname = self._get_kernel_tar_from_centos()

        self._extract_tar(tarname)

    def _get_kernel_tar_from_upstream(self):
        """
        Download sources of the required kernel version from the upstream
        (www.kernel.org). Sources are stored as .tar.xz file.
        :returns Name of the tar file containing the sources.
        """
        url = "https://www.kernel.org/pub/linux/kernel/"

        # Version directory (different naming style for versions under and
        # over 3.0)
        if StrictVersion(self.kernel_version) < StrictVersion("3.0"):
            url += "v{}/".format(self.kernel_version[:3])
        else:
            url += "v{}.x/".format(self.kernel_version[:1])

        tarname = "linux-{}.tar.xz".format(self.kernel_version)
        url += tarname

        # Download the tarball with kernel sources
        print "Downloading kernel version {}".format(self.kernel_version)
        urlretrieve(url, os.path.join(self.kernel_base_path, tarname),
                    show_progress)

        return tarname

    def _get_kernel_tar_from_brew(self):
        """
        Download sources of the required kernel from Brew.
        Sources are part of the SRPM package and need to be extracted out of
        it.
        :returns Name of the tar file containing the sources.
        """
        url = "http://download.eng.bos.redhat.com/brewroot/packages/kernel/"
        version, release = self.kernel_version.split("-")
        url += "{}/{}/src/".format(version, release)
        rpmname = "kernel-{}.src.rpm".format(self.kernel_version)
        url += rpmname
        # Download the source RPM package
        print "Downloading kernel version {}".format(self.kernel_version)
        urlretrieve(url, os.path.join(self.kernel_base_path, rpmname),
                    show_progress)

        cwd = os.getcwd()
        os.chdir(self.kernel_base_path)
        # Extract files from SRPM package
        with open(os.devnull, "w") as devnull:
            rpm_cpio = Popen(["rpm2cpio", rpmname], stdout=PIPE,
                             stderr=devnull)
            check_call(["cpio", "-idmv"], stdin=rpm_cpio.stdout,
                       stderr=devnull)

        tarname = "linux-{}.tar.xz".format(self.kernel_version)
        self._get_config_file()
        self._get_kabi_tarname()
        self._clean_downloaded_files(tarname)
        os.chdir(cwd)

        return tarname

    def _get_kernel_tar_from_centos(self):
        """
        Download sources of the required kernel from the CentOS Git.
        First the corresponding Git repository is downloaded and the URLs of
        the necessary files are derived from it. Then it downloads them and
        extracts the kernel source.
        :returns Name of the tar file containing the sources.
        """
        centos_git_url = "https://git.centos.org/git/rpms/kernel.git"

        tmpdir = mkdtemp()
        cwd = os.getcwd()
        os.chdir(tmpdir)

        # Choose the major version of CentOS
        if self.kernel_version.endswith("el7"):
            major = "c7"
        elif self.kernel_version.endswith("el8"):
            major = "c8"
        else:
            raise BuildException("Unsupported kernel version")

        # Clone the Git repository and checkout the appropriate branch.
        check_call(["git", "clone", "-q", centos_git_url])
        os.chdir("kernel")
        check_call(["git", "checkout", "-q", major])

        # Reset head to the corresponding commit.
        # Note: a simple checkout does not work, because get_sources.sh uses
        # the name of the branch to create the URL of the files. This causes
        # it to fail when the head is detached
        check_call(["git", "reset", "--hard", "-q",
                    "tags/imports/{}/kernel-{}".format(major,
                                                       self.kernel_version)])

        # Clone the centos-git-common repository for the get_sources.sh script
        check_call(["git", "clone", "-q",
                    "https://git.centos.org/git/centos-git-common.git"])

        # Download the sources and copy them into the kernel base directory
        check_call(["centos-git-common/get_sources.sh"])
        os.chdir("SOURCES")
        for f in os.listdir(os.getcwd()):
            if os.path.isfile(f):
                shutil.copy(os.path.abspath(f), self.kernel_base_path)

        os.chdir(self.kernel_base_path)
        shutil.rmtree(tmpdir)

        tarname = "linux-{}.tar.xz".format(self.kernel_version)
        self._get_config_file()
        self._get_kabi_tarname()
        self._clean_downloaded_files(tarname)
        os.chdir(cwd)

        return tarname

    def _get_config_file(self):
        """
        Get the name of the default config file.
        Currently one for x86_64 is taken.
        """
        configfile_options = [
            "kernel-x86_64.config",
            "kernel-{}-x86_64.config".format(self.kernel_version.split("-")[0])
        ]
        for configfile in configfile_options:
            if os.path.isfile(configfile):
                self.configfile = configfile

    def _get_kabi_tarname(self):
        """Get the name of the KABI whitelist archive."""
        kabi_tarname_options = [
            "kernel-abi-whitelists-{}.tar.bz2".format(
                self.kernel_version[:-4]),
            "kernel-abi-whitelists-{}.tar.bz2".format(
                self.kernel_version.split("-")[1].split(".")[0])
        ]
        for kabi_tarname in kabi_tarname_options:
            if os.path.isfile(kabi_tarname):
                self.kabi_tarname = kabi_tarname

    def _clean_downloaded_files(self, tarname):
        # Delete all files except kernel sources tar, config file, and kabi
        # whitelists tar if required (keep the directories since these are
        # other kernels - RPM does not contain dirs).
        for f in os.listdir("."):
            if (os.path.isfile(f) and f != tarname and f != self.configfile and
                    f != self.kabi_tarname):
                os.remove(f)
        self.configfile = os.path.abspath(self.configfile)

    def _extract_tar(self, tarname):
        """Extract kernel sources from .tar.xz file."""
        cwd = os.getcwd()
        os.chdir(self.kernel_base_path)
        print "Extracting"
        check_call(["tar", "-xJf", tarname])
        os.remove(tarname)
        dirname = tarname[:-7]
        # If the produced directory does not have the expected name, rename it.
        if dirname != self.kernel_path:
            os.rename(dirname, self.kernel_path)
        print "Done"
        print("Kernel sources for version {} are in directory {}".format(
            self.kernel_version, self.kernel_path))
        os.chdir(cwd)

    def _symlink_gcc_header(self, major_version):
        """
        Symlink include/linux/compiler-gccX.h for the current GCC version with
        the most recent header in the downloaded kernel
        :param major_version: Major version of GCC to be used for compilation
        """
        include_path = os.path.join(self.kernel_path, "include/linux")
        dest_file = os.path.join(include_path,
                                 "compiler-gcc{}.h".format(major_version))
        if not os.path.isfile(dest_file):
            # Search for the most recent version of header provided in the
            # analysed kernel and symlink the current version to it
            regex = re.compile(r"^compiler-gcc(\d+)\.h$")
            max_major = 0
            for file in os.listdir(include_path):
                match = regex.match(file)
                if match and int(match.group(1)) > max_major:
                    max_major = int(match.group(1))

            if max_major > 0:
                src_file = os.path.join(include_path,
                                        "compiler-gcc{}.h".format(max_major))
                os.symlink(src_file, dest_file)

    def _configure_kernel(self):
        """
        Configure kernel.
        For kernels downloaded from Brew, use the provided config file.
        For kernels downloaded from upstream, configure all as module (run
        `make allmodconfig`).
        Then run:
            make prepare
            make modules_prepare
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        print "Configuring and preparing modules"
        with open(os.devnull, 'w') as null:
            if self.configfile is not None:
                os.rename(self.configfile, ".config")
                self._call_and_print(["make", "olddefconfig"], null, null)
            else:
                self._call_and_print(["make", "allmodconfig"], null, null)
            self._call_and_print(["make", "prepare"], null, null)
            self._call_and_print(["make", "modules_prepare"], null, null)
        os.chdir(cwd)

    def _autogen_time_headers(self):
        """
        Generate headers for kernel/time module (if the module exists) that
        need to be generated automatically.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        try:
            with open(os.devnull, 'w') as null:
                check_call(["make", "-s", "kernel/time.o"], stderr=null)
        except CalledProcessError:
            pass
        finally:
            os.chdir(cwd)

    def _disable_asm_goto(self):
        """
        Transform 'asm goto(x)' command into 'asm("goto(x)")'.
        This is because LLVM does not support asm goto yet.
        The original GCC compiler header is kept since it is needed when
        compiling the kernel using GCC.
        """
        shutil.copy(self.gcc_compiler_header, self.orig_gcc_compiler_header)
        command = ["sed", "-i", "s/asm goto(x)/asm (\"goto(\" #x \")\")/g",
                   self.gcc_compiler_header]
        check_call(command)

    def _extract_kabi_whitelist(self):
        """
        Extract kernel abi whitelist from the downloaded tar and store it
        inside the kernel source dir.
        The file is named kabi_whitelist_x86_64.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_base_path)

        # Create temp dir and extract the files there
        os.mkdir("kabi")
        os.rename(self.kabi_tarname, "kabi/{}".format(self.kabi_tarname))
        os.chdir("kabi")
        check_call(["tar", "-xjf", self.kabi_tarname])

        # Copy the desired whitelist
        shutil.copyfile(os.path.join("kabi-current", self.kabi_whitelist_file),
                        self.kabi_whitelist)

        # Cleanup
        os.chdir(self.kernel_base_path)
        shutil.rmtree("kabi")
        os.chdir(cwd)

    def _clean_kernel(self):
        """Clean whole kernel"""
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        with open(os.devnull, "w") as stdout:
            self._call_and_print(["make", "clean"], stdout=stdout)
        os.chdir(cwd)

    def _call_and_print(self, command, stdout=None, stderr=None):
        print "  {}".format(" ".join(command))
        check_call(command, stdout=stdout, stderr=stderr)

    def _call_output_and_print(self, command):
        with open(os.devnull) as stderr:
            print "    {}".format(" ".join(command))
            output = check_output(command, stderr=stderr)
            return output

    def _check_make_target(self, make_command):
        """
        Check if make target exists.
        Runs make with -n argument which returns 2 if the target does not
        exist.
        :param make_command: Make command to run
        :return True if the command can be run (target exists)
        """
        with open(os.devnull, "w") as devnull:
            ret_code = call(make_command + ["-n"],
                            stdout=devnull, stderr=devnull)
            return ret_code != 2

    def _clean_object(self, obj):
        """Clean an object file"""
        if os.path.isfile(obj):
            os.remove(obj)

    def _clean_module(self, mod):
        """Clean an object file"""
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        with open(os.devnull, "w") as stdout:
            check_call(["make", "M={}".format(self.modules_dir),
                        "{}.ko".format(mod), "clean"],
                       stdout=stdout)
        os.chdir(cwd)

    def _clean_all_modules(self):
        """Clean all modules in the modules directory"""
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        with open(os.devnull, "w") as stdout:
            check_call(["make", "M={}".format(self.modules_dir), "clean"],
                       stdout=stdout)
        os.chdir(cwd)

    def _strip_bash_quotes(self, gcc_param):
        """
        Remove quotes from gcc_param that represents a part of a shell command.
        """
        if "\'" in gcc_param:
            return gcc_param.translate(None, "\'")
        else:
            return gcc_param.translate(None, "\"")

    def _get_ko_name(self, mod):
        """Get name of an object file corresponding to a module"""
        if mod == "built-in":
            return "built-in.o"
        return "{}.ko".format(mod)

    def _swap_gcc_compiler_headers(self):
        """
        Swap contents of include/linux/compiler-gcc.h and
        include/linux/compiler-gcc.h.bak
        """
        tmp_header = self.gcc_compiler_header + ".tmp"
        shutil.move(self.gcc_compiler_header, tmp_header)
        shutil.move(self.orig_gcc_compiler_header, self.gcc_compiler_header)
        shutil.move(tmp_header, self.orig_gcc_compiler_header)

    def kbuild_object_command(self, object_file):
        """
        Build the object file (.o) using KBuild.
        The command used is `make V=1 /path/to/object.o`
        :returns GCC command used for the compilation. This is the last
                 command starting with 'gcc' that was run by make
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)

        if not os.path.isabs(object_file):
            object_file = os.path.join(self.modules_dir, object_file)
        self._clean_object(object_file)
        with open(os.devnull, "w") as stderr:
            try:
                output = check_output(["make", "V=1", object_file,
                                       "--just-print"], stderr=stderr)
            except CalledProcessError:
                raise BuildException("Error compiling {}".format(object_file))
            finally:
                os.chdir(cwd)

        commands = output.splitlines()
        for cs in reversed(commands):
            for c in cs.split(";"):
                if c.lstrip().startswith("gcc"):
                    return c.lstrip()
        raise BuildException("Compiling {} did not run a gcc command".format(
                             object_file))

    def kbuild_module(self, module):
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
        os.chdir(self.kernel_path)
        self._swap_gcc_compiler_headers()

        file_name = module
        command = ["make", "V=1", "M={}".format(self.modules_dir)]
        if module != "built-in":
            command.append("{}.ko".format(file_name))

        if not self._check_make_target(command):
            # If the target does not exist, replace "_" by "-" and try again
            file_name = file_name.replace("_", "-")
            command[3] = "{}.ko".format(file_name)
        if not self._check_make_target(command):
            os.chdir(cwd)
            raise BuildException("Could not build module {}".format(module))

        try:
            output = self._call_output_and_print(command)
            ko_file = os.path.join(self.modules_dir,
                                   self._get_ko_name(file_name))
            if not os.path.exists(ko_file):
                os.chdir(cwd)
                raise BuildException("Could not build module {}"
                                     .format(module))
        except CalledProcessError:
            raise BuildException("Could not build module {}".format(module))
        finally:
            self._swap_gcc_compiler_headers()
            os.chdir(cwd)
        return file_name, output.splitlines()

    def kbuild_all_modules(self):
        """
        Build all kernel modules in modules_dir using Kbuild.
        The command used is `make V=1 M=/path/to/mod`.
        :returns List of commands that were used to compile and link modules.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        self._swap_gcc_compiler_headers()

        command = ["make", "V=1", "M={}".format(self.modules_dir)]

        try:
            output = self._call_output_and_print(command)
            return output.splitlines()
        except CalledProcessError:
            raise BuildException("Could not build modules.")
        finally:
            self._swap_gcc_compiler_headers()
            os.chdir(cwd)

    def get_module_name(self, gcc_command):
        """
        Extracts name of the module from a gcc command used to compile (part
        of) the module.
        The name is usually given by setting KBUILD_MODNAME variable using one
        of two ways:
            -D"KBUILD_MODNAME=KBUILD_STR(module)"
            -DKBUILD_MODNAME='"module"'
        """
        regexes = [
            re.compile(r"^-DKBUILD_MODNAME=KBUILD_STR\((.*)\)$"),
            re.compile(r"^-DKBUILD_MODNAME=\"(.*)\"$")
        ]
        for param in gcc_command.split():
            if "KBUILD_MODNAME" in param:
                param = self._strip_bash_quotes(param)
                for r in regexes:
                    m = r.match(param)
                    if m:
                        return m.group(1)
        raise BuildException("Unable to find module name")

    def get_output_file(self, command):
        """
        Extract name of the output file produced by a command.
        The name is the parameter following -o option.
        :param command: GCC, Clang, or llvm-link command. It is expected to be
                        a list of strings (parameters of the command).
        """
        index = command.index("-o")
        if len(command) == index:
            raise BuildException("Broken command: {}".format("".join(command)))
        return command[index + 1]

    def find_modules(self, commands):
        """
        Extract list of modules that were compiled using the given list of
        commands.
        :param commmands: List of GCC/LD commands.
        :returns List of modules names.
        """
        result = []
        for cmd in commands:
            if cmd.lstrip().startswith("ld"):
                try:
                    mod = self.get_output_file(cmd.split())
                    if mod.endswith(".ko"):
                        result.append(
                            os.path.relpath(mod, self.modules_path)[:-3])
                    elif mod.endswith("built-in.o"):
                        result.append(
                            os.path.relpath(mod, self.modules_path)[:-2])
                except BuildException:
                    pass
        return result

    def gcc_to_llvm(self, gcc_command):
        """
        Convert GCC command to corresponding Clang command for compiling source
        into LLVM IR.
        :param gcc_command: Command to convert
        """
        output_file = None
        command = ["clang", "-S", "-emit-llvm", "-O1", "-Xclang",
                   "-disable-llvm-passes"]
        if self.debug:
            command.extend(["-g", "-fdebug-macro"])
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

            command.append(self._strip_bash_quotes(param))
        if output_file is None:
            raise BuildException("Build error: gcc command not compiling C \
                                 source found")
        command.extend(["-o", output_file])
        return command

    def ld_to_llvm(self, ld_command):
        """
        Convert ld command into llvm-link command to link multiple LLVM IR
        files into one file.
        :param ld_command: Command to convert
        """
        command = ["llvm-link", "-S"]
        for param in ld_command.split():
            if param.endswith(".o"):
                command.append("{}.ll".format(param[:-2]))
            elif param == "-o":
                command.append(param)
        return command

    def opt_llvm(self, llvm_file, command):
        """
        Optimise LLVM IR using 'opt' tool. LLVM passes are chosen based on the
        command that created the file being optimized.
        For compiled files (using clang), run basic simplification passes.
        For linked files (using llvm-link), run -constmerge to remove
        duplicate constants that might have come from linked files.
        """
        opt_command = ["opt", "-S", llvm_file, "-o", llvm_file]
        if command == "clang":
            opt_command.extend(["-lowerswitch", "-mem2reg", "-loop-simplify",
                                "-simplifycfg", "-gvn", "-dce"])
        elif command == "llvm-link":
            opt_command.append("-constmerge")
        else:
            raise BuildException("Invalid call to {}".format(command))
        try:
            with open(os.devnull, "w") as devnull:
                check_call(opt_command, stderr=devnull)
        except CalledProcessError:
            raise BuildException("Running opt failed")

    def kbuild_to_llvm_commands(self, kbuild_commands, file_name=""):
        """
        Convers a list of Kbuild commands for building a module into a list of
        corresponding llvm/clang commands to build the module into LLVM IR.
        GCC commands are transformed into clang commands.
        LD commands are transformed into llvm-link commands.
        Unnecessary commands are filtered out.
        :param kbuild_commands: List of Kbuild commands to transform
        :param file_name: File name of the kernel module to be built
        :return List of llvm/clang commands.
        """
        clang_commands = list()
        for c in kbuild_commands:
            command = c.lstrip()
            if (command.startswith("gcc") and
                    "{}.mod".format(file_name) not in command):
                clang_commands.append(self.gcc_to_llvm(command))
            elif (command.startswith("ld") and
                  "{}.ko".format(file_name) not in command):
                clang_commands.append(
                    self.ld_to_llvm(command.split(";")[0]))
        return clang_commands

    def build_llvm_file(self, file, command):
        """
        Build single source into LLVM IR
        :param file: Name of the result file
        :param command: Command to be executed
        """
        if self.verbose:
            print "    [{}] {}".format(command[0], file)
        if self.rebuild or not os.path.isfile(file):
            with open(os.devnull, "w") as stderr:
                try:
                    check_call(command, stderr=stderr)
                    self.opt_llvm(file, command[0])
                except CalledProcessError:
                    # Do not raise exceptions if built-in.ll cannot be built.
                    # This always happens when files are built into modules.
                    if not file.endswith("built-in.ll"):
                        raise BuildException("Building {} failed".format(file))

    def build_llvm_module(self, name, file_name, commands):
        """
        Build kernel module into LLVM IR.
        :param name: Module name
        :param file_name: Name of the kernel object file without extension (can
                          be different from the module name).
        :param commands: List of clang/llvm-link commands to be executed
        :returns Instance of LlvmKernelModule with information about files
                 containing the compiled module
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)

        # Check if the module has not already been built
        if name in self.built_modules:
            os.chdir(cwd)
            return self.built_modules[name]

        for command in commands:
            file = self.get_output_file(command)
            try:
                self.build_llvm_file(file, command)
            except BuildException:
                os.chdir(cwd)
                raise

        if not os.path.isfile(os.path.join(self.modules_dir, "{}.ll".format(
                                                             file_name))):
            os.chdir(cwd)
            raise BuildException("Building {} did not produce LLVM IR file"
                                 .format(name))
        os.chdir(cwd)
        mod = LlvmKernelModule(name, file_name, self.modules_path)
        self.built_modules[name] = mod
        return mod

    def build_file_for_symbol(self, f):
        """
        Looks up files containing the function using CScope, then compiles them
        using Clang and looks whether the function actually is in the module.
        First module containing the function is returned.
        :param f: Name of the function to look up.
        :returns First module containing the specified function.
        """
        mod = None

        srcs = self.source.find_srcs_with_symbol_def(f)
        for src in srcs:
            try:
                mod = self.build_file(src)
                mod.parse_module()
                if not (mod.has_function(f) or mod.has_global(f)):
                    mod.clean_module()
                    mod = None
                else:
                    break
            except BuildException:
                mod = None
        if not mod:
            raise SourceNotFoundException(f)

        return mod

    def link_modules(self, modules):
        """
        Link depending modules together.
        Some modules depend on other modules, link those to them.
        :param modules: Dict of form name -> LlvmKernelModule.
        """
        if self.verbose:
            print "Linking dependent modules"
        linked = set()
        for name, mod in modules.iteritems():
            if mod.depends is None:
                continue
            depmods = []
            do_not_link = []
            for depends in mod.depends:
                # Find module with the given name (may require replacing dashes
                # by underscores).
                if depends in modules:
                    modname = depends
                elif depends.replace("-", "_") in modules:
                    modname = depends.replace("-", "_")
                else:
                    continue
                depmods.append(modules[modname])
                # If a dependency module is linked, do not link its
                # dependencies.
                if modname in linked:
                    for m in modules[modname].depends:
                        if m in modules:
                            do_not_link.append(m)
                        if m.replace("-", "_") in modules:
                            do_not_link.append(m.replace("-", "_"))

            if depmods:
                to_link = [d for d in depmods if d.name not in do_not_link]
                if self.verbose:
                    print "  [llvm-link] {}".format(
                        os.path.relpath(mod.llvm, self.kernel_path))
                mod.parse_module()
                for m in to_link:
                    m.parse_module()
                mod.link_modules(to_link)
                linked.add(name)
        for mod in modules.itervalues():
            mod.clean_module()

    def build_file(self, file_name):
        """
        Build single object file.
        First use kbuild to get the gcc command for building the file, then
        convert it into corresponding clang command and run it.
        :param file_name: Name of the file (without extension)
        :returns Instance of LlvmKernelModule where the compiled file is the
                 main module file and no kernel object file is provided.
        """
        name = file_name[:-2] if file_name.endswith(".c") else file_name
        # If the module has already been built, return it
        if name in self.built_modules:
            return self.built_modules[name]

        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        try:
            file_path = os.path.join(self.modules_dir, "{}.ll".format(name))
            if self.rebuild or not os.path.isfile(file_path):
                command = self.kbuild_object_command("{}.o".format(name))
                command = self.gcc_to_llvm(command)
                self.build_llvm_file(file_path, command)
            mod = LlvmKernelModule(name, name, self.modules_path)
            self.built_modules[name] = mod
            return mod
        except BuildException:
            raise
        finally:
            os.chdir(cwd)

    def build_module(self, module, prevent_clean=False):
        """
        Build kernel module.
        First use kbuild to build kernel object file. Then, transform kbuild
        commands to the corresponding clang commands and build LLVM IR of the
        module.
        :param module: Name of the module
        :param prevent_clean: Do not clean the module.
        :return Built module in LLVM IR (instance of LlvmKernelModule)
        """
        if not prevent_clean and self.rebuild:
            self._clean_all_modules()

        file_name, commands = self.kbuild_module(module)
        clang_commands = self.kbuild_to_llvm_commands(commands, file_name)
        if module == "built-in":
            name = os.path.basename(os.path.normpath(self.modules_dir))
        else:
            name = os.path.basename(module)
        return self.build_llvm_module(name, file_name, clang_commands)

    def build_all_modules(self):
        """
        Build all modules in the modules directory.
        :return Dictionary of modules in form name -> module
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        if self.verbose:
            print "Building all modules"
        self._clean_all_modules()

        # Use Kbuild to build directory and extract names of built modules
        gcc_commands = self.kbuild_all_modules()
        modules = self.find_modules(gcc_commands)
        # Build same files into LLVM
        clang_commands = self.kbuild_to_llvm_commands(gcc_commands)
        for command in clang_commands:
            file = self.get_output_file(command)
            self.build_llvm_file(file, command)

        llvm_modules = dict()
        for mod in modules:
            # Only create modules that have been actually built
            if os.path.isfile(os.path.join(self.modules_dir,
                                           "{}.ll".format(mod))):
                # If the module name is "built-in", set it to the directory it
                # is located in.
                name = mod
                if name == "built-in":
                    name = os.path.basename(os.path.normpath(self.modules_dir))
                else:
                    name = os.path.basename(name)

                llvm_modules[name] = LlvmKernelModule(name, mod,
                                                      self.modules_path)
        os.chdir(cwd)
        return llvm_modules

    def build_modules_with_params(self):
        """
        Build all modules in the modules directory that can be configured via
        parameters.
        :return Dictionary of modules in form name -> module
        """
        if self.verbose:
            print "Building all kernel modules having parameters"
            print "  Collecting modules"
        if self.rebuild:
            self._clean_all_modules()
        sources = self.source.get_sources_with_params(self.modules_path)
        modules = set()
        # First build objects from sources that contain definitions of
        # parameters. By building them, we can obtain names of modules they
        # belong to.
        for src in sources:
            obj = src[:-1] + "o"
            command = self.kbuild_object_command(obj)
            mod = self.get_module_name(command)
            mod_dir = os.path.relpath(os.path.dirname(src), self.kernel_path)
            if mod_dir != self.modules_dir:
                mod_dir = os.path.relpath(mod_dir, self.modules_dir)
                mod = os.path.join(mod_dir, mod)
            # Put mod into modules
            modules.add(mod)

        # Build collected modules with Kbuild and collect commands
        # Then, transform commands to use Clang/LLVM and run them to build LLVM
        # IR of modules
        llvm_modules = dict()
        for mod in modules:
            if self.verbose:
                print "  {}".format(mod)
            try:
                llvm_mod = self.build_module(mod, True)
                llvm_modules[llvm_mod.name] = llvm_mod
            except BuildException as e:
                if self.verbose:
                    print "    {}".format(str(e))
        if self.verbose:
            print ""
        return llvm_modules

    def build_sysctl_module(self, sysctl):
        """
        Build the source file containing the definition of a sysctl option.
        :param sysctl: sysctl option to search for
        :return: Instance of LlvmSysctlModule.
        """
        # The sysctl is composed of entries separated by dots. Entries form
        # a hierarchy - each entry is a child of its predecessor (i.e. all
        # entries except the last one point to sysctl tables). We follow
        # the hierarchy and build the source containing the parent table of
        # the last entry.
        entries = sysctl.split(".")
        if entries[0] in ["kernel", "vm", "fs", "debug", "dev"]:
            src = "kernel/sysctl.c"
            table = "sysctl_base_table"
        elif entries[0] == "net":
            if entries[1] == "ipv4":
                if entries[2] == "conf":
                    src = "net/ipv4/devinet.c"
                    table = "devinet_sysctl.1"
                    entries = entries[4:]
                else:
                    src = "net/ipv4/sysctl_net_ipv4.c"
                    table = "ipv4_table"
                    entries = entries[2:]
            if entries[1] == "core":
                src = "net/core/sysctl_net_core.c"
                table = "net_core_table"
                entries = entries[2:]
        else:
            raise SourceNotFoundException(sysctl)

        for (i, entry) in enumerate(entries):
            # Build the file normally and then create a corresponding
            # LlvmSysctlModule with the obtained sysctl table.
            kernel_mod = self.build_file(src)
            sysctl_mod = LlvmSysctlModule(kernel_mod, table)

            if i == len(entries) - 1:
                return sysctl_mod
            table = sysctl_mod.get_child(entry).name
            src = self.source.find_srcs_with_symbol_def(table)[0]

    def get_kabi_whitelist(self):
        """Get a list of functions on the kernel abi whitelist."""
        if not os.path.isfile(self.kabi_whitelist):
            raise BuildException("KABI whitelist not found")
        with open(self.kabi_whitelist) as whitelist_file:
            funs = whitelist_file.readlines()
            # Strip whitespaces and skip the first line (whitelist header)
            return [f.lstrip().strip() for f in funs][1:]
