"""
A mechanism for caching the results of individual SimpLL runs.

The function comparison results that have been collected so far are stored in
a graph whose vertices are the individual results (i.e. whether two functions
are equal or not) and the edges are function calls. This graph is represented
by the ComparisonGraph class.
"""

from collections import deque
from diffkemp.semdiff.result import Result
from enum import IntEnum


class ComparisonGraph:
    """
    Representation of a (partial or complete) result of a DiffKemp run
    in comparison mode.
    """
    class Side(IntEnum):
        """Either the left module or the right module."""
        LEFT = 0
        RIGHT = 1

    class Vertex:
        """
        Vertex in the comparison graph.

        Represents a single function difference and contains both function
        names, the comparison result, a list of attached non-function
        differences and a list of its edges (i.e. the direct callees).

        Note: names, files and lines are tuples containing the values for both
        modules.
        """
        def __init__(self, names, result, files=None, lines=None):
            self.names = names
            self.result = result
            self.nonfun_diffs = []
            self.successors = ([], [])
            self.files = files
            self.lines = lines

        def __repr__(self):
            return "Vertex({0}, {1}, {2}, {3}, {4})".format(
                self.names, self.result, self.nonfun_diffs, self.files,
                self.lines
            )

        @classmethod
        def from_yaml(cls, fun_result, parent_graph=None):
            """
            Generates a Vertex object including all edges from a YAML
            representation returned by SimpLL.
            """
            res_left = fun_result["first"]
            res_right = fun_result["second"]
            vertex = cls(
                (res_left["function"], res_right["function"]),
                Result.Kind.from_string(fun_result["result"]),
                (res_left["file"] if "file" in res_left else None,
                 res_right["file"] if "file" in res_right else None),
                (res_left["line"] if "line" in res_left else None,
                 res_right["line"] if "line" in res_right else None)
            )
            if "calls" in res_left:
                for res, side in [(res_left,
                                   ComparisonGraph.Side.LEFT),
                                  (res_right,
                                   ComparisonGraph.Side.RIGHT)]:
                    for callee in res["calls"]:
                        edge = ComparisonGraph.Edge.from_yaml(callee)
                        if parent_graph:
                            parent_graph.add_edge(vertex, side, edge)
                        else:
                            vertex.add_successor(side, edge)
            # Add non-function differences.
            if "differing-objects" in fun_result:
                for nonfun_diff in fun_result["differing-objects"]:
                    if "body-first" in nonfun_diff:
                        # Syntax difference
                        diff_obj = ComparisonGraph.SyntaxDiff.from_yaml(
                            nonfun_diff)
                    elif "file-first" in nonfun_diff:
                        # Type difference
                        diff_obj = ComparisonGraph.TypeDiff.from_yaml(
                            nonfun_diff)
                    vertex.add_nonfun_diff(diff_obj)
            return vertex

        def add_successor(self, side, edge):
            """Adds a direct callee."""
            self.successors[side].append(edge)

        def add_nonfun_diff(self, diff):
            """Adds a non-function difference."""
            self.nonfun_diffs.append(diff)

    class Edge:
        """
        Edges in the comparison graph.
        Note: the object contains the target name instead of the Vertex object
        (unlike in all other cases) because it doesn't have to be present when
        the edge object is generated.
        """
        def __init__(self, target_name, filename, line):
            self.target_name = target_name
            self.filename = filename
            self.line = line

        def __repr__(self):
            return "Edge({0}, {1}, {2})".format(
                self.target_name, self.filename, self.line
            )

        @classmethod
        def from_yaml(cls, callee):
            """Generates an Edge object from YAML returned by SimpLL."""
            return cls(callee["function"], callee["file"], int(callee["line"]))

    class NonFunDiff:
        """
        A non-function difference.

        Note: callstack is a tuple containing the values for both modules.
        """
        def __init__(self, name, parent_fun, callstack):
            self.name = name
            self.parent_fun = parent_fun
            self.callstack = callstack

    class SyntaxDiff(NonFunDiff):
        """
        A syntax difference.

        Note: body is a tuple containing the values for both modules.
        """
        def __init__(self, name, parent_fun, callstack, body):
            self.name = name
            self.parent_fun = parent_fun
            self.callstack = callstack
            self.body = body

        @classmethod
        def from_yaml(cls, nonfun_diff):
            """Generates a SyntaxDiff object from YAML returned by SimpLL."""
            return cls(
                nonfun_diff["name"],
                nonfun_diff["function"],
                (nonfun_diff["stack-first"], nonfun_diff["stack-second"]),
                (nonfun_diff["body-first"], nonfun_diff["body-second"])
            )

    class TypeDiff(NonFunDiff):
        """
        A syntax difference.

        Note: file and line are tuples containing the values for both modules.
        """
        def __init__(self, name, parent_fun, callstack, file, line):
            self.name = name
            self.parent_fun = parent_fun
            self.callstack = callstack
            self.file = file
            self.line = line

        @classmethod
        def from_yaml(cls, nonfun_diff):
            """Generates a TypeDiff object from YAML returned by SimpLL."""
            return cls(
                nonfun_diff["name"],
                nonfun_diff["function"],
                (nonfun_diff["stack-first"], nonfun_diff["stack-second"]),
                (nonfun_diff["file-first"], nonfun_diff["file-second"]),
                (nonfun_diff["line-first"], nonfun_diff["line-second"])
            )

    def __init__(self):
        # Vertices are stored as a dictionary, the keys being the function
        # names in the left module and the values objects containing the result
        self.vertices = dict()
        self.equal_funs = set()

    def __getitem__(self, function_name):
        return self.vertices[function_name]

    def __setitem__(self, function_name, value):
        self.vertices[function_name] = value
        if value.result == Result.Kind.EQUAL:
            self.equal_funs.add(value.names[ComparisonGraph.Side.LEFT])

    def __repr__(self):
        return "Graph(vertices: {0}, equal_funs: {1})".format(
            str(self.vertices), str(self.equal_funs)
        )

    def get_callstack(self, side, start_fun_name, end_fun_name):
        """
        Finds the callstack of the specified function using breadth-first
        search.

        Returns a list of Edge objects representing the shortest path from the
        first function to the second one.
        """
        start_fun = self[start_fun_name]
        end_fun = self[end_fun_name]
        visited = set()
        backtracking_map = dict()
        queue = deque()
        queue.appendleft(start_fun)
        end_found = False
        while queue and not end_found:
            current = queue.pop()
            visited.add(current)
            for edge in current.successors[side]:
                if edge.target_name not in self.vertices:
                    # Invalid successor.
                    continue
                target = self[edge.target_name]
                if target in visited:
                    continue
                # Insert successor into queue and a pair containing the edge
                # and the source vertex to the backtracking map.
                backtracking_map[target] = (edge, current)
                if target == end_fun:
                    end_found = True
                    break
                queue.appendleft(target)
        if not end_found:
            raise ValueError("Path in graph not found")
        # Backtrack the search to get all edges.
        callstack_edges = []
        current_vertex = end_fun
        while current_vertex != start_fun:
            edge = backtracking_map[current_vertex][0]
            callstack_edges.append(edge)
            current_vertex = backtracking_map[current_vertex][1]
        return list(reversed(callstack_edges))

    def reachable_from(self, side, start_fun_name):
        """
        Generates a list of all functions depending on the function
        in the argument (e.g. all vertices reachable from the one specified by
        the function name including the base vertex).
        """
        start_fun = self[start_fun_name]
        visited = set()
        stack = []
        result = []
        stack.append(start_fun)
        while stack:
            current = stack.pop()
            visited.add(current)
            result.append(current)
            for edge in current.successors[side]:
                if edge.target_name not in self.vertices:
                    # Invalid successor.
                    continue
                target = self[edge.target_name]
                if target in visited:
                    continue
                stack.append(target)
        return result

    def absorb_graph(self, graph):
        """
        Merges another graph into this one. New vertices (i.e. vertices from
        graph that do not appear in self) are added and the equal funs set is
        updated respectively.
        Note: the graph in the argument is expected to reference (in edges)
        only vertices that are already present in one graph or the other.
        """
        for name, vertex in graph.vertices.items():
            if name not in self.vertices:
                # Note: the entry to equal_funs is added automatically.
                self[name] = vertex
