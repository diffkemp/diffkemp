import glob
import os
import tarfile
from diffkemp.llvm_ir.compiler import KernelModuleCompiler, CompilerException
from diffkemp.llvm_ir.function_collector import FunctionCollector
from diffkemp.llvm_ir.module_analyser import *
from diffkemp.slicer.slicer import slice_module
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


class LlvmKernelModule:
    kernel_base_path = "kernel"

    def __init__(self, kernel_version, module_path, module_name, param,
                 debug=False, verbose=False):
        self.kernel_base_path = os.path.abspath(self.kernel_base_path)
        self.kernel_version = kernel_version
        self.kernel_path = os.path.join(self.kernel_base_path,
                                        "linux-%s" % self.kernel_version)
        self.module_path = module_path
        self.object_file = "%s.o" % module_name
        self.src = os.path.join(self.kernel_path, self.module_path,
                                "%s.c" % module_name)
        self.param = param
        self.debug = debug
        self.verbose = verbose

        self.llvm_unsliced = None
        self.llvm = None
        self.linked_llvm = set()
        self.called_functions = set()
        self.main_functions = set()


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


    def build_all_objects(self):
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        print "Building all object files in the same directory"

        stdout = None
        if not self.verbose:
            stdout = open('/dev/null', 'w')

        make = Popen(["make", "M=%s" % self.module_path], stdout=stdout)
        make.wait()
        if make.returncode != 0:
            raise CompilerException("Building of other modules has failed")
        os.chdir(cwd)


    def collect_functions(self):
        assert self.llvm is not None
        collector = FunctionCollector(self.llvm)
        self.main_functions = collector.using_param(self.param)
        self.called_functions = collector.called_by(self.main_functions)


    def collect_undefined(self):
        assert self.llvm is not None
        collector = FunctionCollector(self.llvm)
        return collector.undefined(self.called_functions)


    def compile_objects_with_definitions(self, functions):
        defs = set()
        cwd = os.getcwd()
        os.chdir(os.path.join(self.kernel_path, self.module_path))
        undefined = functions
        for obj in glob("*.o"):
            if not os.path.isfile(obj[:-1] + "c"):
                continue
            obj_defs = find_definitions_in_object(obj, functions)
            if obj_defs:
                try:
                    # Compile the object to LLVM IR
                    obj_compiler = KernelModuleCompiler(self.kernel_path,
                                                        self.module_path, obj)
                    obj_llvm = obj_compiler.compile_to_ir(self.debug,
                                                          self.verbose)
                    self.linked_llvm.add(obj_llvm)
                    # Collect functions called by new definitions 
                    obj_collector = FunctionCollector(obj_llvm)
                    obj_called = obj_collector.called_by(obj_defs)
                    self.called_functions.update(obj_called)
                    # Update the set of undefined functions (remove definitions
                    # that were found and add functions undefined in the new
                    # module)
                    undefined = undefined - obj_defs
                    functions.update(obj_collector.undefined(obj_called))
                except CompilerException:
                    # If compilation fails, we continue (the symbol will stay
                    # undefined or will be found in another object)
                    pass
        os.chdir(cwd)
        if undefined != functions:
            self.compile_objects_with_definitions(undefined)


    def link_objects(self):
        if self.linked_llvm:
            cwd = os.getcwd()
            os.chdir(os.path.join(self.kernel_path, self.module_path))
            print "Linking object files into %s" % self.llvm
            linker_command = ["llvm-link", "-S", "-o", self.llvm, self.llvm]
            linked_command = linker_command + list(self.linked_llvm)
            linker = Popen(linker_command)
            linker.wait()
            if linker.returncode != 0:
                raise CompilerException("Linking has failed")
            os.chdir(cwd)


    def build(self):
        cwd = os.getcwd()
        print "Kernel version %s" % self.kernel_version
        print "-----------"
        try:
            self.get_kernel_source()
            self.configure_kernel()

            compiler = KernelModuleCompiler(self.kernel_path, self.module_path,
                                            self.object_file)
            self.llvm_unsliced = compiler.compile_to_ir(self.debug,
                                                        self.verbose)
            os.chdir(cwd)

            check_module(self.llvm_unsliced, self.param)
            self.llvm = slice_module(self.llvm_unsliced, self.param,
                                     verbose=self.verbose)

            self.collect_functions()
            undefined_funs = self.collect_undefined()
            if undefined_funs:
                self.build_all_objects()
                self.compile_objects_with_definitions(undefined_funs)

            self.link_objects()

            print ""
        except:
            raise

