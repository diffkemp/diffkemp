"""
Contains logic for creating output of DiffKemp compare command.

Specifically, the YAML file is created here which contains
information about found differences and definitions of symbols
which are located in the differences.
"""
from diffkemp.semdiff.caching import ComparisonGraph
from diffkemp.utils import get_end_line, EndLineNotFound
from diffkemp.semdiff.result import Result
import os
import yaml


class MacroDefinitions:
    """Class for extracting information about macro definitions."""
    def __init__(self, snapshot_dir_old, snapshot_dir_new, result):
        self.old_dir = snapshot_dir_old
        self.new_dir = snapshot_dir_new
        self.result = result
        # Information from call stacks for extracting defs for differing macros
        # - list of tuples (vertex name, differing/last macro name).
        self.diff_macro_infos = set()
        # Definitions in YamlOutput format.
        self.definitions = {}

    def extract_from_result(self, called_res, compared_fun):
        """
        Extracts information about macro definitions from result of called
        entity.
        :param called_res: Result of called entity.
        :param compared_fun: Name of compared function.
        """
        # Note: Definitions of differing/last macro is not possible to extract
        # directly from the call stacks, we will now extract for it only
        # name of the macro and function/vertex name, the definition will be
        # extracted later in `_add_defs_for_differing` method.
        if called_res.first.callstack:
            macro_defs, diff_macro_info = called_res.first.callstack \
                .get_macro_defs(compared_fun)
            self._add_definitions(macro_defs, True)
            if diff_macro_info is not None:
                self.diff_macro_infos.add(diff_macro_info)
        if called_res.second.callstack:
            macro_defs, diff_macro_info = called_res.second.callstack \
                .get_macro_defs(compared_fun)
            self._add_definitions(macro_defs, False)
            if diff_macro_info is not None:
                self.diff_macro_infos.add(diff_macro_info)

    def get_yaml_defs(self):
        """Returns definitions of macros in YamlOutput format."""
        # Extracting the missing definitions for differing macros.
        self._add_defs_for_differing()
        return self.definitions

    def _add_defs_for_differing(self):
        """Updates definitions with definitions of differing macros."""
        if self.result.graph is None:
            return
        # For each differing/last macro finding SyntaxDiff object
        # which contains definition of the macro.
        for vertex_name, diff_macro_name in self.diff_macro_infos:
            if vertex_name not in self.result.graph.vertices:
                continue
            vertex = self.result.graph[vertex_name]
            for non_fun in vertex.nonfun_diffs:
                if non_fun.name == diff_macro_name and \
                        isinstance(non_fun, ComparisonGraph.SyntaxDiff):
                    if non_fun.diff_def:
                        self._add_definition(non_fun.diff_def[0],
                                             True,
                                             non_fun.kind)
                        self._add_definition(non_fun.diff_def[1],
                                             False,
                                             non_fun.kind)
                    break

    def _add_definitions(self, macro_defs, is_old=True, kind="macro"):
        """Updates definitions with multiple macro definitions."""
        for macro_def in macro_defs:
            self._add_definition(macro_def, is_old, kind)

    def _add_definition(self, macro_def, is_old=True, kind="macro"):
        """Adds new definition.
        :param macro_def: Definition extracted from the call stack or
            from SyntaxDiff definition of differing object
            (dict with keys: name, line, file).
        :param is_old: True for old (False for new) version of program.
        :param kind: One of: macro/macro-function/function-macro."""
        name = macro_def["name"].split()[0]
        base_dir = self.old_dir if is_old else self.new_dir
        version = "old" if is_old else "new"
        file = os.path.join(base_dir, macro_def["file"])
        line = macro_def["line"]
        definition = {
            "kind": kind,
            version: create_def_info(line, file, base_dir, kind)
        }
        if name not in self.definitions:
            self.definitions[name] = definition
        else:
            self.definitions[name].update(definition)


class YamlOutput:
    """Yaml representation of compare result."""

    def __init__(self, snapshot_dir_old, snapshot_dir_new, result):
        self.old_dir = snapshot_dir_old
        self.new_dir = snapshot_dir_new
        self.result = result
        self.output = {}
        # Sets with symbol names
        self.function_names = set()
        # Note: type_names contains tuples (type name, parent name)
        #   parent name - name of function in which type is used
        self.type_names = set()

        # Instance for creating macro definitions.
        self.macro_definitions = MacroDefinitions(snapshot_dir_old,
                                                  snapshot_dir_new,
                                                  result)

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
        # Going over compared functions (fun_name) and its result.
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
                    # Note: Macro names are not currently used.
                    function_names, macro_names, type_names = called_res \
                        .first.callstack.get_symbol_names(fun_name)
                    self.function_names.update(function_names)
                    self.type_names.update(type_names)

                self.macro_definitions.extract_from_result(called_res,
                                                           fun_name)
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
        self.output["definitions"].update(
            self.macro_definitions.get_yaml_defs()
        )

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
                "old": create_def_info(vertex.lines[0],
                                       vertex.files[0],
                                       self.old_dir,
                                       "function"),
                "new": create_def_info(vertex.lines[1],
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
        """Creates definitions for types."""
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
                        "old": create_def_info(non_fun.line[0],
                                               non_fun.file[0],
                                               self.old_dir,
                                               "type"),
                        "new": create_def_info(non_fun.line[1],
                                               non_fun.file[1],
                                               self.new_dir,
                                               "type")
                    }
                    definitions[type_name] = definition
                    break
        self.output["definitions"].update(definitions)


def create_def_info(line, file, snapshot_dir, kind):
    """
    Tries to find end of line for a symbol and returns dict containing
    information about definition of the symbol.
    :param line: Line where symbol definition starts.
    :param file: Path to file containing symbol definition.
    :param snapshot_dir: Path to snapshot directory.
    :param kind: Kind of symbol (function/type/macro).
    """
    info = {
        "line": line,
        "file": os.path.relpath(file, snapshot_dir),
    }
    try:
        info["end-line"] = get_end_line(file, line, kind)
    except (UnicodeDecodeError, EndLineNotFound, ValueError):
        pass
    return info
