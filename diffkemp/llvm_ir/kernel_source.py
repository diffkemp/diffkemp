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
                    if os.path.islink(os.path.join(root, f)):
                        continue
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
            with open(os.devnull, "w") as devnull:
                cscope_output = check_output(command, stderr=devnull)
            return [l for l in cscope_output.splitlines() if
                    l.split()[0].endswith("c")]
        except CalledProcessError:
            return []

    def _find_tracepoint_macro_use(self, symbol):
        """
        Find usages of tracepoint macro creating a tracepoint symbol.
        :param symbol: Symbol generated using the macro.
        :return: List of found cscope entries.
        """
        macro_argument = symbol[len("__tracepoint_"):]
        candidates = self._cscope_run("EXPORT_TRACEPOINT_SYMBOL", False)
        return filter(lambda c: c.endswith("(" + macro_argument + ");"),
                      candidates)

    def find_srcs_with_symbol_def(self, symbol):
        """
        Use cscope to find a definition of the given symbol.
        :param symbol: Symbol to find.
        :return List of source files potentially containing the definition.
        """
        cwd = os.getcwd()
        os.chdir(self.kernel_path)
        try:
            cscope_defs = self._cscope_run(symbol, True)

            # It may not be enough to get the definitions from the cscope.
            # There are multiple possible reasons:
            #   - the symbol is only defined in headers
            #   - there is a bug in cscope - it cannot find definitions
            #     containing function pointers as parameters
            cscope_uses = self._cscope_run(symbol, False)

            # Look whether this is one of the special cases when cscope does
            # not find a correct source because of the exact symbol being
            # created by the preprocessor
            if any([symbol.startswith(s) for s in
                    ["param_get_", "param_set_", "param_ops_"]]):
                # Symbol param_* are created in kernel/params.c using a macro
                cscope_defs = [os.path.join(self.kernel_path,
                                            "kernel/params.c")] + cscope_defs
            elif symbol.startswith("__tracepoint_"):
                # Functions starting with __tracepoint_ are created by a macro
                # in include/kernel/tracepoint.h; the corresponding usage of
                # the macro has to be found to get the source file
                cscope_defs = \
                    self._find_tracepoint_macro_use(symbol) + cscope_defs

            if len(cscope_defs) == 0 and len(cscope_uses) == 0:
                raise SourceNotFoundException(symbol)
        except SourceNotFoundException:
            if symbol == "vfree":
                cscope_uses = []
                cscope_defs = [os.path.join(self.kernel_path, "mm/vmalloc.c")]
            else:
                raise
        finally:
            os.chdir(cwd)

        # We now create a list of files potentially containing the file
        # definition. The list is sorted by priority:
        #   1. Files marked by cscope as containing the symbol definition.
        #   2. Files marked by cscope as using the symbol in <global> scope.
        #   3. Files marked by cscope as using the symbol in other scope.
        # Each group is also partially sorted - sources from the drivers/ and
        # the arch/ directories occur later than the others (using prio_cmp).
        # Moreover, each file occurs in the list just once (in place of its
        # highest priority).
        seen = set()

        def prio_cmp(a, b):
            if a.startswith("drivers/"):
                return 1
            if b.startswith("drivers/"):
                return -1
            if a.startswith("arch/"):
                return 1
            if b.startswith("arch/"):
                return -1
            return 0

        files = sorted(
            [os.path.relpath(f, self.kernel_path)
             for f in [line.split()[0] for line in cscope_defs]
             if not (f in seen or seen.add(f))],
            cmp=prio_cmp)
        files.extend(sorted(
            [os.path.relpath(f, self.kernel_path)
             for (f, scope) in [(line.split()[0], line.split()[1])
                                for line in cscope_uses]
             if (scope == "<global>" and not (f in seen or seen.add(f)))],
            cmp=prio_cmp))
        files.extend(sorted(
            [os.path.relpath(f, self.kernel_path)
             for (f, scope) in [(line.split()[0], line.split()[1])
                                for line in cscope_uses]
             if (scope != "<global>" and not (f in seen or seen.add(f)))],
            cmp=prio_cmp))
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
