"""
Building kernel module into LLVM IR.
Downloads the kernel sources, compiles the chosen module into LLVM IR, and
links it against other modules containing implementations of undefined
functions.
"""

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
    """
    Kernel module in LLVM IR
    Main class for downloading, compiling, and linking the kernel module.
    """

    # Base path to kernel sources
    kernel_base_path = "kernel"

    def __init__(self, kernel_version, module_path, module_name, param,
                 debug=False, verbose=False):
        self.kernel_base_path = os.path.abspath(self.kernel_base_path)
        self.kernel_version = kernel_version
        self.kernel_path = os.path.join(self.kernel_base_path,
                                        "linux-%s" % self.kernel_version)
        self.module_path = module_path
        # .o file of the module
        self.object_file = "%s.o" % module_name
        self.src = os.path.join(self.kernel_path, self.module_path,
                                "%s.c" % module_name)
        self.param = param
        self.debug = debug
        self.verbose = verbose

        # File with unsliced LLVM IR of the module
        self.llvm_unsliced = None
        # File with sliced LLVM IR od the module
        self.llvm = None
        # Set of files with other modules to be linked
        self.linked_llvm = set()
        self.called_functions = set()
        self.main_functions = set()


    def get_kernel_source(self):
        """Download the sources of the required kernel version if needed."""
        if not os.path.isdir(self.kernel_path):
            url = "https://www.kernel.org/pub/linux/kernel/"

            # Version directory (different naming style for versions under and
            # over 3.0)
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
        """Configure kernel to default configuration (run `make defconfig`)"""
        os.chdir(self.kernel_path)
        print "Configuring kernel"
        make = Popen(["make", "defconfig"], stdout=PIPE)
        make.wait()
        if make.returncode != 0:
            raise CompilerException("`make config` has failed")


    def build_all_objects(self):
        """
        Build all objects in the same directory using the Makefile provided
        by kernel.
        The used command is: `make M=/path/to/module`
        """
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
        """
        Collect main and called functions for the module.
        Main functions are those that directly use the analysed parameter and
        that will be compared to corresponding functions of the other module.
        Called functions are those that are (recursively) called by main
        functions.
        """
        assert self.llvm is not None
        collector = FunctionCollector(self.llvm)
        self.main_functions = collector.using_param(self.param)
        self.called_functions = collector.called_by(self.main_functions)


    def collect_undefined(self):
        """Collect functions not having definitions in the module."""
        assert self.llvm is not None
        collector = FunctionCollector(self.llvm)
        return collector.undefined(self.called_functions)


    def compile_objects_with_definitions(self, functions):
        """
        Search and compile (to LLVM IR) all objects (modules) that contain
        definitions of functions that are undefined in the analysed module.
        We limit the search to modules from the same directory as the analysed
        module. These modules are supposed to be compiled into object (.o)
        files using the Makefile(s) provided in the kernel.
        The function is called recursively if the newly vadded function
        definitions call other undefined functions.
        """
        defs = set()
        cwd = os.getcwd()
        os.chdir(os.path.join(self.kernel_path, self.module_path))
        undefined = functions
        for obj in glob("*.o"):
            # Compile only objects that have corresonding source (others are
            # just results of linkning)
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
                    # If the compilation fails, we continue (the symbol will 
                    # remain undefined or will be found in another object)
                    pass
        os.chdir(cwd)
        # Continue recursively if some functions remain undefined
        if undefined != functions:
            self.compile_objects_with_definitions(undefined)


    def link_objects(self, main_llvm):
        """Link all LLVM IR modules together using the `llvm-link` command."""
        if self.linked_llvm:
            cwd = os.getcwd()
            os.chdir(os.path.join(self.kernel_path, self.module_path))
            print "Linking object files into %s" % main_llvm
            linker_command = ["llvm-link", "-S", "-o", main_llvm, main_llvm]
            linker_command = linker_command + list(self.linked_llvm)
            linker = Popen(linker_command)
            linker.wait()
            if linker.returncode != 0:
                raise CompilerException("Linking has failed")

            # Run some more optimisations after linking, particularly remove
            # duplicate constants that might come from different modules
            opt_process = Popen(["opt", "-S", "-constmerge",
                                 main_llvm,
                                 "-o", main_llvm])
            opt_process.wait()
            if opt_process.returncode != 0:
                raise CompilerException("Running opt on module failed")
            os.chdir(cwd)


    def link_unsliced(self):
        """
        Link also the unsliced version of the module (this is not linked by
        the builder by default). Currently, this method is required by the
        regression testing script.
        """
        self.link_objects(self.llvm_unsliced)


    def build(self):
        cwd = os.getcwd()
        print "Kernel version %s" % self.kernel_version
        print "-----------"
        try:
            self.get_kernel_source()
            self.configure_kernel()

            # Compile the analysed module
            compiler = KernelModuleCompiler(self.kernel_path, self.module_path,
                                            self.object_file)
            self.llvm_unsliced = compiler.compile_to_ir(self.debug,
                                                        self.verbose)
            os.chdir(cwd)

            check_module(self.llvm_unsliced, self.param)
            self.llvm = slice_module(self.llvm_unsliced, self.param,
                                     verbose=self.verbose)

            # Find and comile modules from the same directory containing
            # implementations of functions undefined in the main module
            self.collect_functions()
            undefined_funs = self.collect_undefined()
            if undefined_funs:
                self.build_all_objects()
                self.compile_objects_with_definitions(undefined_funs)

            self.link_objects(self.llvm)

            print ""
        except:
            raise

