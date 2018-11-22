"""
Browsing kernel sources.
Functions for searching function definitions, kernel modules, etc.
"""
import os
from subprocess import CalledProcessError, check_call, check_output


class SourceNotFoundException(Exception):
    def __init__(self, fun):
        self.fun = fun

    def __str__(self):
        return "Source for {} not found".format(self.fun)


class KernelSource:
    """
    Source code of a single kernel.
    Provides functions to search source files for function definitions, kernel
    modules, and others.
    """

    def __init__(self, path):
        self.kernel_path = path

    def get_sources_with_params(self, directory):
        """
        Get list of .c files in the given directory and all its subdirectories
        that contain definitions of module parameters (contain call to
        module_param macro).
        """
        path = os.path.join(self.kernel_path, directory)
        result = list()
        for f in os.listdir(path):
            file = os.path.join(path, f)
            if os.path.isfile(file) and file.endswith(".c"):
                for line in open(file, "r"):
                    if "module_param" in line:
                        result.append(file)
                        break
            elif os.path.isdir(file):
                dir_files = self.get_sources_with_params(file)
                result.extend(dir_files)

        return result

    def build_cscope_database(self):
        """
        Build a database for the cscope tool. It will be later used to find
        source files with symbol definitions.
        """
        # If the database exists, do not rebuild it
        if "cscope.files" in os.listdir(self.kernel_path):
            return

        # Write all files that need to be scanned into cscope.files
        with open(os.path.join(self.kernel_path, "cscope.files"), "w") \
                as cscope_file:
            for root, dirs, files in os.walk(self.kernel_path):
                if ("/Documentation/" in root or
                        "/scripts/" in root or
                        "/tmp" in root):
                    continue

                for f in files:
                    if (f.endswith((".c", ".h", ".x", ".s", ".S"))):
                        cscope_file.write("{}\n".format(os.path.join(root, f)))

        # Build cscope database
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        check_call(["cscope", "-b", "-q", "-k"])
        os.chdir(cwd)

    def _cscope_run(self, symbol, definition):
        """
        Run cscope search for a symbol.
        :param symbol: Symbol to search for
        :param definition: If true, search definitions, otherwise search all
                           usage.
        :return: List of found cscope entries.
        """
        self.build_cscope_database()
        try:
            command = ["cscope", "-d", "-L"]
            if definition:
                command.append("-1")
            else:
                command.append("-0")
            command.append(symbol)
            cscope_output = check_output(command)
            return cscope_output.splitlines()
        except CalledProcessError:
            raise SourceNotFoundException(symbol)

    def find_srcs_with_symbol_def(self, symbol):
        """
        Use cscope to find a definition of the given symbol.
        :param symbol: Symbol to find.
        :return List of source files potentially containing the definition.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        try:
            cscope_out = self._cscope_run(symbol, True)
            if len(cscope_out) == 0:
                # There is a bug in cscope - it cannot find definitions
                # containing function pointers as parameters. If no source was
                # found, try to search all occurrences of the symbol and filter
                # those that are marked as <global>.
                cscope_out = [l for l in self._cscope_run(symbol, False) if
                              l.split()[1] == "<global>"]
            if len(cscope_out) == 0:
                raise SourceNotFoundException
        except SourceNotFoundException:
            raise
        finally:
            os.chdir(cwd)

        files = []
        for line in cscope_out:
            file = os.path.relpath(line.split()[0], self.kernel_path)
            if file.endswith(".c"):
                # .c files have higher priority - put them to the beginning of
                # the list
                files.insert(0, file)
            elif file.endswith(".h"):
                # .h files have lower priority - append them to the end of
                # the list
                files.append(file)
        return files

    def find_srcs_using_symbol(self, symbol):
        """
        Use cscope to find sources using a symbol.
        :param symbol: Symbol to find.
        :return List of source files containing functions that use the symbol.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        try:
            cscope_out = self._cscope_run(symbol, False)
            if len(cscope_out) == 0:
                raise SourceNotFoundException
            files = set()
            for line in cscope_out:
                if line.split()[0].endswith(".h"):
                    continue
                if line.split()[1] == "<global>":
                    continue
                files.add(os.path.relpath(line.split()[0], self.kernel_path))
            return files
        except SourceNotFoundException:
            raise
        finally:
            os.chdir(cwd)

    def find_src_for_sysctl(self, sysctl):
        """Find source in which a sysctl option is defined."""
        if sysctl.startswith("kernel."):
            return "kernel/sysctl.c", "kern_table"
        raise SourceNotFoundException(sysctl)
