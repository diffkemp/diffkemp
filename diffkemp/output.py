from diffkemp.semdiff.caching import ComparisonGraph
from diffkemp.utils import get_end_line, EndLineNotFound
from diffkemp.semdiff.result import Result
import os
import yaml
import sys


class YamlOutput:
    """Yaml representation of compare result."""

    def __init__(self, snapshot_dir_old, snapshot_dir_new, result):
        self.old_dir = snapshot_dir_old
        self.new_dir = snapshot_dir_new
        self.result = result
        self.output = {}
        # Sets with symbol names
        self.function_names = set()
        self.macro_names = set()
        # Note: type_names contains tuples (type name, parent name)
        self.type_names = set()

        self._create_output()

    def save(self, output_dir, file_name):
        with open(os.path.join(output_dir, file_name), "w") as file:
            yaml.dump(self.output, file, sort_keys=False)

    def _create_output(self):
        """Creates yaml representation of compare result."""
        self.output["old-snapshot"] = self.old_dir
        self.output["new-snapshot"] = self.new_dir

        self._create_results()
        self._create_definitions()

    def _create_results(self):
        # list of group results or all function results
        main_results = []
        if self.result.has_inner_groups:
            for group_name, group_result in self.result.inner.items():
                results = []
                self._create_group_results(results, group_result)

                main_results.append({
                    "sysctl": group_name,
                    "results": results
                })
        else:
            self._create_group_results(main_results, self.result)

        self.output["results"] = main_results

    def _create_group_results(self, results, comparison_result):
        """
        Creates call stacks of not-equal functions.
        :param results: list of function differences modified in this function
        :param comparison_result: output of snapshot comparison
        """
        for fun_name, fun_result in comparison_result.inner.items():
            if fun_result.kind != Result.Kind.NOT_EQUAL:
                continue
            self.function_names.add(fun_name)
            # list of not-equal functions callstacks for a function
            diffs = []
            for called_res in sorted(fun_result.inner.values(),
                                     key=lambda r: r.first.name):
                if called_res.diff == "" and called_res.first.covered:
                    # Do not add empty diffs
                    continue
                # information about a function which is different
                diff = {}
                diff["function"] = called_res.first.name
                # getting callstacks
                diff["old-callstack"] = called_res.first \
                    .callstack.to_output_yaml_with_rel_path(self.old_dir) \
                    if called_res.first.callstack else []
                diff["new-callstack"] = called_res.second \
                    .callstack.to_output_yaml_with_rel_path(self.new_dir) \
                    if called_res.second.callstack else []
                # updating symbol names
                if called_res.first.callstack:
                    function_names, macro_names, type_names = called_res \
                        .first.callstack.get_symbol_names(fun_name)
                    self.function_names.update(function_names)
                    self.macro_names.update(macro_names)
                    self.type_names.update(type_names)

                diffs.append(diff)
            results.append({
                "function": fun_name,
                "diffs": diffs
            })

    def _create_definitions(self):
        self.output["definitions"] = {}
        self._create_function_defs()
        self._create_type_defs()

    def _create_function_defs(self):
        if self.result.graph is None:
            return
        definitions = {}
        for name in self.function_names:
            if name not in self.result.graph.vertices:
                continue
            vertex = self.result.graph[name]
            definition = {
                "kind": "function",
                "old": self._create_def_info(vertex.lines[0],
                                             vertex.files[0],
                                             self.old_dir,
                                             "function"),
                "new": self._create_def_info(vertex.lines[1],
                                             vertex.files[1],
                                             self.new_dir,
                                             "function"),
            }
            # function name differs
            if vertex.names[1] != vertex.names[0]:
                definition["new"]["name"] = vertex.names[1]
            definitions[name] = definition
        self.output["definitions"].update(definitions)

    def _create_type_defs(self):
        """Returns definitions of types.
        :param type_names: Set of tuples (type name, parent name),
                           parent name - name of function in which type is used
        """
        if self.result.graph is None:
            return
        definitions = {}
        for type_name, parent_name in self.type_names:
            if parent_name not in self.result.graph.vertices:
                continue
            vertex = self.result.graph[parent_name]
            for non_fun in vertex.nonfun_diffs:
                if non_fun.name == type_name and \
                        isinstance(non_fun, ComparisonGraph.TypeDiff):
                    definition = {
                        "kind": "type",
                        "old": self._create_def_info(non_fun.line[0],
                                                     non_fun.file[0],
                                                     self.old_dir,
                                                     "type"),
                        "new": self._create_def_info(non_fun.line[1],
                                                     non_fun.file[1],
                                                     self.new_dir,
                                                     "type")
                    }
                    definitions[type_name] = definition
                    break
        self.output["definitions"].update(definitions)

    def _create_def_info(self, line, file, snapshot_dir, kind):
        info = {
            "line": line,
            "file": os.path.relpath(file, snapshot_dir),
        }
        try:
            info["end-line"] = get_end_line(file, line, kind)
        except (UnicodeDecodeError, EndLineNotFound):
            pass
        return info


class OutputWriter:
    """
    Handles formatted output of function diffs, including indentation
    and callstack printing, to either a file or stdout.
    """
    def __init__(self, output_dir, fun, fun_tag, initial_indent, show_diff,
                 snapshot_dir_old, snapshot_dir_new,
                 old_dir_abs_path, new_dir_abs_path):
        self.output_dir = output_dir
        self.fun = fun
        self.fun_tag = fun_tag
        self.indent = initial_indent
        self.show_diff = show_diff
        self.snapshot_dir_old = snapshot_dir_old
        self.snapshot_dir_new = snapshot_dir_new
        self.old_dir_abs_path = old_dir_abs_path
        self.new_dir_abs_path = new_dir_abs_path

        # Set in __enter__
        self.output = None

    def __enter__(self):
        self._prepare_output()
        return self

    def __exit__(self, *_):
        if self.output is not None and self.output is not sys.stdout:
            self.output.close()

    def _text_indent(self, text):
        """
        Indent each line in the text by a number of spaces (width)
        """
        return ''.join(" "*self.indent+line for line in text.splitlines(True))

    def _write(self, text):
        self.output.write(self._text_indent(text))

    def _prepare_output(self):
        """
        Prepare the output stream for writing.

        Opens a diff file if an output directory is set; otherwise, uses stdout
        Writes header information and applies indentation as needed.
        """
        if self.output_dir:
            self.output = open(os.path.join(self.output_dir,
                                            "{}.diff".format(self.fun)), "w")
            self._write("Found differences in functions called by {}"
                        .format(self.fun))
            if self.fun_tag is not None:
                self._write(" ({})".format(self.fun_tag))
            self._write("\n\n")
        else:
            self.output = sys.stdout
            if self.fun_tag is not None:
                self._write("{} ({}):\n".format(self.fun, self.fun_tag))
            else:
                self._write("{}:\n".format(self.fun))
            self.indent += 2

    def write_called_result(self, called_res):
        self._write("{} differs:\n".format(called_res.first.name))

        if not self.output_dir:
            self._write("{{{\n")

        self.indent += 2
        if called_res.first.callstack:
            self._write_callstack(called_res.first.callstack,
                                  self.snapshot_dir_old, self.old_dir_abs_path)
        if called_res.second.callstack:
            self._write_callstack(called_res.second.callstack,
                                  self.snapshot_dir_new, self.new_dir_abs_path)
        if self.show_diff:
            self._write_diff(called_res)

        self.indent -= 2
        if not self.output_dir:
            self._write("}}}\n")
        self.output.write("\n")

    def _write_callstack(self, callstack, label, abs_path):
        self._write("Callstack ({}):\n".format(label))
        self._write(callstack.as_str_with_rel_paths(abs_path))
        self.output.write("\n\n")

    def _write_diff(self, called_res):
        if (called_res.diff.strip() == "" and
                called_res.macro_diff is not None):
            self._write("\n".join(map(str, called_res.macro_diff)))
        else:
            self._write("Diff:\n")
            self._write(called_res.diff)
