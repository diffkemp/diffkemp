import os
import re
import sys
import tarfile
from distutils.version import StrictVersion
from progressbar import ProgressBar, Percentage, Bar
from subprocess import Popen, PIPE
from urllib import urlretrieve

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


def _strip_bash_quotes(gcc_param):
    if "\'" in gcc_param:
        return gcc_param.translate(None, "\'")
    else:
        return gcc_param.translate(None, "\"")


class CompilerException(Exception):
    pass


class KernelModuleCompiler:
    kernel_base_path = "kernel"

    def __init__(self, kernel_version, module_path, module_name):
        self.kernel_base_path = os.path.abspath(self.kernel_base_path)
        self.kernel_version = kernel_version
        self.kernel_path = os.path.join(self.kernel_base_path,
                                        "linux-%s" % self.kernel_version)
        self.module_path = module_path
        self.object_file = "%s.o" % module_name
        self.src_file = os.path.join(self.kernel_path, self.module_path,
                                     "%s.c" % module_name)


    def get_kernel_source(self):
        if not os.path.isdir(self.kernel_path):
            url = "https://www.kernel.org/pub/linux/kernel/"

            # version directory
            if StrictVersion(self.kernel_version) < StrictVersion("3.0"):
                url += "v%s/" % self.kernel_version[:3]
            else:
                url += "v%s.x/" % self.kernel_version[:1]

            tarname = "linux-%s.tar.gz" % self.kernel_version
            url += tarname

            # Download the tarball with kernel sources
            print "Downloading kernel version %s" % self.kernel_version
            urlretrieve(url, os.path.join(self.kernel_base_path, tarname),
                        show_progress)

            # Extract kernel sources
            print "Extracting"
            os.chdir(self.kernel_base_path)
            tar = tarfile.open(tarname, "r:gz")
            tar.extractall()
            tar.close

            os.remove(tarname)
            print "Done"
            print("Kernel sources for version %s are in directory %s" %
                  (self.kernel_version, self.kernel_path))


    def configure_kernel(self):
        # Run `make defconfig`
        os.chdir(self.kernel_path)
        print "Configuring kernel"
        make = Popen(["make", "defconfig"], stdout=PIPE)
        make.wait()
        if make.returncode != 0:
            raise CompilerException("`make config` has failed")


    def clear_object(self):
        # Remove .o file
        # This is needed so that make runs gcc and we can reuse arguments
        os.chdir(self.kernel_path)
        print "Clearing object file %s" % self.object_file
        object_file = os.path.join(self.module_path, self.object_file)
        try:
            os.remove(object_file)
        except OSError:
            pass


    def _symlink_gcc_header(self, major_version):
        # Symlink include/linux/compiler-gccX.h for current GCC version with
        # most recent header in the downloaded kernel
        include_path = os.path.join(self.kernel_path, "include/linux")
        dest_file = os.path.join(include_path,
                                 "compiler-gcc%d.h" % major_version)
        if not os.path.isfile(dest_file):
            regex = re.compile("^compiler-gcc(\d+)\.h$")
            max_major = 0
            for file in os.listdir(include_path):
                match = regex.match(file)
                if match and int(match.group(1)) > max_major:
                    max_major = int(match.group(1))

            if max_major > 0:
                src_file = os.path.join(include_path,
                                        "compiler-gcc%d.h" % max_major)
                os.symlink(src_file, dest_file)


    def build_object(self):
        # Build .o
        os.chdir(self.kernel_path)

        self._symlink_gcc_header(7)

        print "Compiling object file %s" % self.object_file
        object_file = os.path.join(self.module_path, self.object_file)
        make = Popen(["make", "V=1", object_file], stdout=PIPE)
        make.wait()
        if make.returncode != 0:
            raise CompilerException("Compiling module with gcc has failed")
        # Return contents of stdout
        return make.communicate()[0]


    def build_ir(self, gcc_command, debug=False, verbose=False):
        # Build .bc (LLVM IR) using parameters of the GCC command
        os.chdir(self.kernel_path)

        command = ["clang", "-S", "-emit-llvm", "-O0"]
        if debug:
            command.append("-g")
        for param in gcc_command.split():
            if (param == "gcc" or
                (param.startswith("-W") and "-MD" not in param) or
                param.startswith("-f") or
                param.startswith("-m") or
                param.startswith("-O") or
                param == "-DCC_HAVE_ASM_GOTO"):
                continue

            if param.endswith(".o"):
                self.ir_file = "%s.bc" % param[:-2]
                command.append(self.ir_file)
                continue

            command.append(_strip_bash_quotes(param))

        print "Compiling LLVM IR file %s" % self.ir_file

        stderr = None
        if not verbose:
            stderr = open('/dev/null', 'w')

        print " ".join(command)
        clang_process = Popen(command, stdout=PIPE, stderr=stderr)
        clang_process.wait()
        if clang_process.returncode != 0:
            raise CompilerException("Compiling module with clang has failed")


    def opt_ir(self):
        os.chdir(self.kernel_path)
        print "Optimizing LLVM IR file"
        opt_process = Popen(["opt", "-S", "-mem2reg", "-loop-simplify",
                             "-simplifycfg", "-lowerswitch",
                             self.ir_file,
                             "-o", self.ir_file])
        opt_process.wait()
        if opt_process.returncode != 0:
            raise CompilerException("Running opt on module failed")


    def compile_to_ir(self, debug=False, verbose=False):
        cwd = os.getcwd()
        print "Kernel version %s" % self.kernel_version
        print "-----------"
        try:
            self.get_kernel_source()
            self.configure_kernel()

            self.clear_object()
            make_output = self.build_object()
            self.build_ir(make_output.split("\n")[-2], debug, verbose)
            self.opt_ir()

            print ""
            return os.path.join(self.kernel_path, self.ir_file)
        except:
            raise
        finally:
            os.chdir(cwd)


    def get_module_src_file(self):
        return self.src_file

