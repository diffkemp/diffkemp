from diffkemp.semdiff.caching import ComparisonGraph
from diffkemp.utils import get_end_line, EndLineNotFound
from diffkemp.semdiff.result import Result
import os
import yaml


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
        """Creates call stacks of not-equal functions."""
        # list of all not-equal functions callstacks
        results = []
        for fun_name, fun_result in self.result.inner.items():
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
        self.output["results"] = results

    def _create_definitions(self):
        self.output["definitions"] = {}
        self._create_function_defs()
        self._create_type_defs()

    def _create_function_defs(self):
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
        definitions = {}
        """Returns definitions of types.
        :param type_names: Set of tuples (type name, parent name),
                           parent name - name of function in which type is used
        """
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
