"""Unit tests for the ComparisonGraph class."""

from diffkemp.semdiff.caching import ComparisonGraph
from diffkemp.semdiff.result import Result
import pytest


def dup(elem):
    """Generates a pair containing the same element twice."""
    return (elem, elem)


@pytest.fixture
def graph():
    g = ComparisonGraph()
    # Vertices
    g["main_function"] = ComparisonGraph.Vertex(
        dup("main_function"), Result.Kind.EQUAL, dup("app/main.c"), dup(51)
    )
    g["side_function"] = ComparisonGraph.Vertex(
        dup("side_function"), Result.Kind.EQUAL, dup("app/main.c"), dup(255)
    )
    g["do_check"] = ComparisonGraph.Vertex(
        dup("do_check"), Result.Kind.NOT_EQUAL, dup("app/main.c"), dup(105)
    )
    g["missing"] = ComparisonGraph.Vertex(
        dup("missing"), Result.Kind.ASSUMED_EQUAL, dup("app/mod.c"), dup(665)
    )
    g["looping"] = ComparisonGraph.Vertex(
        dup("looping"), Result.Kind.EQUAL, dup("app/main.c"), (81, 82)
    )
    # Weak variant of "strength" function vertex (e.g. void-returning on the
    # right side)
    g["strength.void"] = ComparisonGraph.Vertex(
        ("strength", "strength.void"), Result.Kind.EQUAL, dup("app/main.c"),
        (5, 5)
    )
    # Strong variant of "strength" functin vertex
    g["strength"] = ComparisonGraph.Vertex(
        ("strength", "strength"), Result.Kind.EQUAL, dup("app/test.h"), (5, 5)
    )
    # Non-function differences
    g["do_check"].nonfun_diffs.append(ComparisonGraph.SyntaxDiff(
        "MACRO", "do_check",
        dup([
            {"function": "_MACRO", "file": "test.c", "line": 1},
            {"function": "__MACRO", "file": "test.c", "line": 2},
            {"function": "___MACRO", "file": "test.c", "line": 3},
        ]), ("5", "5L")
    ))
    g["do_check"].nonfun_diffs.append(ComparisonGraph.TypeDiff(
        "struct file", "do_check",
        dup([
            {"function": "struct file (type)", "file": "include/file.h",
             "line": 121},
        ]), dup("include/file.h"), dup(121)
    ))
    # Edges
    for side in ComparisonGraph.Side:
        g.add_edge(g["main_function"], side,
                   ComparisonGraph.Edge("do_check", "app/main.c", 58))
        g.add_edge(g["main_function"], side,
                   ComparisonGraph.Edge("side_function", "app/main.c", 59))
        g.add_edge(g["do_check"], side,
                   ComparisonGraph.Edge("missing", "app/main.c", 60))
        g.add_edge(g["do_check"], side,
                   ComparisonGraph.Edge("looping", "app/main.c", 74))
        g.add_edge(g["looping"], side,
                   ComparisonGraph.Edge("main_function", "app/main.c", 85))
        # Strong call of "strength"
        g.add_edge(g["looping"], side,
                   ComparisonGraph.Edge("strength", "app/main.c", 86))
        g.add_edge(g["strength"], side,
                   ComparisonGraph.Edge("missing", "app/w.c", 6))
    # Weak call of "strength"
    g.add_edge(g["side_function"], ComparisonGraph.Side.LEFT,
               ComparisonGraph.Edge("strength", "app/main.c", 260))
    g.add_edge(g["side_function"], ComparisonGraph.Side.RIGHT,
               ComparisonGraph.Edge("strength.void", "app/main.c", 260))
    yield g


def test_add_vertex_strong(graph):
    """Tests adding a strong vertex to the graph."""
    graph["test"] = ComparisonGraph.Vertex(
        dup("test"), Result.Kind.EQUAL, dup("app/main.c"), (81, 82)
    )
    assert "test" in graph.vertices
    assert graph["test"].names == dup("test")
    assert graph["test"].result == Result.Kind.EQUAL
    assert graph["test"].files == dup("app/main.c")
    assert graph["test"].lines == (81, 82)
    assert graph["test"] not in graph._weak_vertex_cache


def test_add_vertex_weak(graph):
    """Tests adding a weak vertex to the graph."""
    graph["test.void"] = ComparisonGraph.Vertex(
        ("test", "test.void"), Result.Kind.EQUAL, dup("app/main.c"), (81, 82)
    )
    assert "test.void" in graph.vertices
    assert graph["test.void"].names == ("test", "test.void")
    assert graph["test.void"].result == Result.Kind.EQUAL
    assert graph["test.void"].files == dup("app/main.c")
    assert graph["test.void"].lines == (81, 82)
    assert graph["test.void"] in graph._weak_vertex_cache


def test_add_edge_strong(graph):
    """Tests adding a strong edge to a graph."""
    for side in ComparisonGraph.Side:
        graph.add_edge(graph["main_function"], side,
                       ComparisonGraph.Edge("missing", "app/main.c", 61))
        assert "missing" in [edge.target_name for edge in
                             graph["main_function"].successors[side]]
        assert "missing" not in [edge.target_name for edge in
                                 graph._normalize_edge_cache]


def test_add_edge_weak(graph):
    """Tests adding a weak edge to a graph."""
    graph.add_edge(graph["main_function"], ComparisonGraph.Side.LEFT,
                   ComparisonGraph.Edge("strength", "app/main.c", 61))
    graph.add_edge(graph["main_function"], ComparisonGraph.Side.RIGHT,
                   ComparisonGraph.Edge("strength.void", "app/main.c", 61))
    left_succesor_names = [edge.target_name for edge in
                           graph["main_function"].successors[
                                                  ComparisonGraph.Side.LEFT]]
    right_successor_names = [edge.target_name for edge in
                             graph["main_function"].successors[
                                            ComparisonGraph.Side.RIGHT]]
    assert "strength" in left_succesor_names
    assert "strength.void" in right_successor_names
    assert "strength" not in right_successor_names
    assert "strength.void" not in left_succesor_names
    assert "strength.void" in [edge.target_name
                               for edge in graph._normalize_edge_cache]


def test_get_callstack(graph):
    """
    Tests callstack lookup.
    Note: the case actually has two paths, therefore this also checks whether
    the shorter one was found.
    """
    for side in ComparisonGraph.Side:
        stack = graph.get_callstack(side, "looping", "missing")
        assert len(stack) == 2
        assert stack[0].target_name == "strength"
        assert stack[1].target_name == "missing"


def test_reachable_from_basic(graph):
    """Tests called function list generation (without normalization)."""
    reachable_l = graph.reachable_from(ComparisonGraph.Side.LEFT, "do_check")
    reachable_r = graph.reachable_from(ComparisonGraph.Side.RIGHT, "do_check")
    assert (set([v.names[ComparisonGraph.Side.LEFT] for v in reachable_l]) ==
            {"do_check", "looping", "strength", "main_function",
            "side_function"})
    # Note: in the right module the void-returning variant "strength.void" is
    # used alongside the original one.
    # This is solved by normalization (and tested later).
    assert (set([v.names[ComparisonGraph.Side.RIGHT] for v in reachable_r]) ==
            {"do_check", "looping", "strength", "main_function",
             "side_function", "strength.void"})


def test_absort_graph(graph):
    """Tests the absorb graph function, especially whether vertex replacing
    works properly."""
    new_graph = ComparisonGraph()
    # This vertex should replace the old one (which is assumed equal).
    new_graph["missing"] = ComparisonGraph.Vertex(
        dup("missing"), Result.Kind.NOT_EQUAL, dup("app/mod.c"), dup(665)
    )
    # This vertex should not replace the old one.
    new_graph["do_check"] = ComparisonGraph.Vertex(
        dup("do_check"), Result.Kind.EQUAL, dup("app/mod.c"), dup(665)
    )
    graph.absorb_graph(new_graph)
    assert graph["missing"].result == Result.Kind.NOT_EQUAL
    assert graph["do_check"].result == Result.Kind.NOT_EQUAL


def test_normalize(graph):
    """Tests whether the conditions for a normalized graph hold after the
    normalization."""
    graph.normalize()
    # Check whether all vertices are strong.
    assert all(["." not in v.names[side] for v in graph.vertices.values()
                for side in ComparisonGraph.Side])
    # Check whether all edges point to existing vertices.
    assert all([e.target_name in graph.vertices
               for side in ComparisonGraph.Side
               for vertex in graph.vertices.values()
               for e in vertex.successors[side]])
    # Check kind of the weak and strong calls.
    assert any([e.kind == ComparisonGraph.DependencyKind.STRONG
                for side in ComparisonGraph.Side
                for e in graph["looping"].successors[side]
                if e.target_name == "strength"])
    assert any([e.kind == ComparisonGraph.DependencyKind.WEAK
                for side in ComparisonGraph.Side
                for e in graph["side_function"].successors[side]
                if e.target_name == "strength"])


def test_reachable_from_extended(graph):
    """
    Tests called function list generation with weak edges.
    Note: to generate weak edges the normalize method has to be used before,
    therefore this test doesn't make sense when the one for normalize fails.
    """
    graph.normalize()
    for side in ComparisonGraph.Side:
        reachable = graph.reachable_from(side, "side_function")
        # The weakly dependent "strength" function should not be in the set.
        assert (set([v.names[side] for v in reachable]) ==
                {"side_function"})


def test_graph_to_fun_pair_list(graph):
    """Tests the conversion of a graph to a structure representing the output
    of DiffKemp."""
    objects_to_compare, syndiff_bodies_left, syndiff_bodies_right = \
        graph.graph_to_fun_pair_list("main_function", "main_function")
    for side in [0, 1]:
        assert {obj[side].name for obj in objects_to_compare} == {
            "do_check", "MACRO", "struct file"}
        do_check = [obj[side] for obj in objects_to_compare
                    if obj[side].name == "do_check"][0]
        macro = [obj[side] for obj in objects_to_compare
                 if obj[side].name == "MACRO"][0]
        struct_file = [obj[side] for obj in objects_to_compare
                       if obj[side].name == "struct file"][0]
        assert do_check.filename == "app/main.c"
        assert do_check.line == 105
        assert do_check.callstack == "do_check at app/main.c:58"
        assert do_check.diff_kind == "function"
        assert do_check.covered
        assert macro.filename is None
        assert macro.line is None
        assert macro.callstack == ("do_check at app/main.c:58\n"
                                   "_MACRO at test.c:1\n"
                                   "__MACRO at test.c:2\n"
                                   "___MACRO at test.c:3")
        assert macro.diff_kind == "syntactic"
        assert not macro.covered
        assert struct_file.filename == "include/file.h"
        assert struct_file.line == 121
        assert struct_file.callstack == ("do_check at app/main.c:58\n"
                                         "struct file (type) at "
                                         "include/file.h:121")
        assert struct_file.diff_kind == "type"
        assert not struct_file.covered
    # All results should be not equal.
    assert {obj[2] for obj in objects_to_compare} == {Result.Kind.NOT_EQUAL}
    assert syndiff_bodies_left == {"MACRO": "5"}
    assert syndiff_bodies_right == {"MACRO": "5L"}
