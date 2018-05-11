import os
import tarfile
from compiler import KernelModuleCompiler
from diffkemp.slicer.slicer import slice_module
from distutils.version import StrictVersion
from module_analyser import check_module
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


class LlvmKernelModule:
    kernel_base_path = "kernel"

    def __init__(self, kernel_version, module_path, module_name, param):
        self.kernel_base_path = os.path.abspath(self.kernel_base_path)
        self.kernel_version = kernel_version
        self.kernel_path = os.path.join(self.kernel_base_path,
                                        "linux-%s" % self.kernel_version)
        self.module_path = module_path
        self.object_file = "%s.o" % module_name
        self.src = os.path.join(self.kernel_path, self.module_path,
                                "%s.c" % module_name)
        self.param = param
        self.llvm_unsliced = None
        self.llvm = None


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


    def build(self, debug=False, verbose=False):
        cwd = os.getcwd()
        print "Kernel version %s" % self.kernel_version
        print "-----------"
        try:
            self.get_kernel_source()
            self.configure_kernel()

            compiler = KernelModuleCompiler(self.kernel_path, self.module_path,
                                            self.object_file)
            self.llvm_unsliced = compiler.compile_to_ir(debug, verbose)
            os.chdir(cwd)

            check_module(self.llvm_unsliced, self.param)
            self.llvm = slice_module(self.llvm_unsliced, self.param,
                                     verbose=verbose)
            print ""
        except:
            raise

