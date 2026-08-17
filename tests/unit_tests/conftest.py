import pytest
from diffkemp.semdiff.caching import ComparisonGraph
from diffkemp.semdiff.result import Result


def dup(elem):
    """Generates a pair containing the same element twice."""
    return (elem, elem)


@pytest.fixture
def graph():
    g = ComparisonGraph()
    # Vertices
    g["main_function"] = ComparisonGraph.Vertex(
        dup("main_function"), Result.Kind.EQUAL, dup(None), dup("app/main.c"),
        dup(51)
    )
    g["side_function"] = ComparisonGraph.Vertex(
        dup("side_function"), Result.Kind.EQUAL, dup(None), dup("app/main.c"),
        dup(255)
    )
    g["do_check"] = ComparisonGraph.Vertex(
        dup("do_check"), Result.Kind.NOT_EQUAL, dup(None), dup("app/main.c"),
        dup(105)
    )
    g["missing"] = ComparisonGraph.Vertex(
        dup("missing"), Result.Kind.ASSUMED_EQUAL, dup(None), dup("app/mod.c"),
        dup(665)
    )
    g["looping"] = ComparisonGraph.Vertex(
        dup("looping"), Result.Kind.EQUAL, dup(None), dup("app/main.c"),
        (81, 82)
    )
    # Weak variant of "strength" function vertex (e.g. void-returning on the
    # right side)
    g["strength.void"] = ComparisonGraph.Vertex(
        ("strength", "strength.void"), Result.Kind.EQUAL, dup(None),
        dup("app/main.c"), (5, 5)
    )
    # Strong variant of "strength" functin vertex
    g["strength"] = ComparisonGraph.Vertex(
        ("strength", "strength"), Result.Kind.EQUAL, dup(None),
        dup("app/test.h"), (5, 5)
    )
    # Function with non-default global variable
    g[("with_glob", "glob1")] = ComparisonGraph.Vertex(
        dup("with_glob"), Result.Kind.EQUAL, dup("glob1"), dup("app/main.c"),
        dup(142)
    )
    g["with_glob"] = ComparisonGraph.Vertex(
        dup("with_glob"), Result.Kind.EQUAL, dup(None), dup("app/main.c"),
        dup(152)
    )
    g["hidden"] = ComparisonGraph.Vertex(
        dup("hidden"), Result.Kind.EQUAL, dup(None), dup("app/main.c"),
        dup(162)
    )
    # Dotted function with non-default global variable
    g[("with_glob.void", "glob2")] = ComparisonGraph.Vertex(
        ("with_glob", "with_glob.void"), Result.Kind.EQUAL, dup("glob2"),
        dup("app/main.c"), (500, 500)
    )
    g[("with_glob", "glob2")] = ComparisonGraph.Vertex(
        ("with_glob", "with_glob"), Result.Kind.EQUAL, dup("glob2"),
        dup("app/test.h"), (500, 500)
    )
    # Non-function differences
    g["do_check"].nonfun_diffs.append(ComparisonGraph.SyntaxDiff(
        "macro", "___MACRO", "do_check",
        dup([
            {"function": "_MACRO (macro)", "file": "test.c", "line": 1},
            {"function": "__MACRO (macro)", "file": "test.c", "line": 2},
            {"function": "___MACRO (macro)", "file": "test.c", "line": 3},
        ]), ("5", "5L"),
        dup({"name": "___MACRO", "file": "test.c", "line": 4})
    ))
    g["do_check"].nonfun_diffs.append(ComparisonGraph.TypeDiff(
        "struct_file", "do_check",
        dup([
            {"function": "struct_file (type)", "file": "include/file.h",
             "line": 121},
        ]), dup("include/file.h"), dup(121)
    ))
    # Edges
    for side in ComparisonGraph.Side:
        g.add_edge(g["main_function"], side,
                   ComparisonGraph.Edge("do_check", None, "app/main.c", 58))
        g.add_edge(g["main_function"], side,
                   ComparisonGraph.Edge("side_function", None, "app/main.c",
                                        59))
        g.add_edge(g["main_function"], side,
                   ComparisonGraph.Edge("with_glob", "glob1", "app/main.c",
                                        57))
        g.add_edge(g["do_check"], side,
                   ComparisonGraph.Edge("missing", None, "app/main.c", 60))
        g.add_edge(g["do_check"], side,
                   ComparisonGraph.Edge("looping", None, "app/main.c", 74))
        g.add_edge(g["looping"], side,
                   ComparisonGraph.Edge("main_function", None, "app/main.c",
                                        85))
        g.add_edge(g["with_glob"], side,
                   ComparisonGraph.Edge("hidden", None, "app/main.c",
                                        157))
        # Strong call of "strength"
        g.add_edge(g["looping"], side,
                   ComparisonGraph.Edge("strength", None, "app/main.c", 86))
        g.add_edge(g["strength"], side,
                   ComparisonGraph.Edge("missing", None, "app/w.c", 6))
    # Weak call of "strength"
    g.add_edge(g["side_function"], ComparisonGraph.Side.LEFT,
               ComparisonGraph.Edge("strength", None, "app/main.c", 260))
    g.add_edge(g["side_function"], ComparisonGraph.Side.RIGHT,
               ComparisonGraph.Edge("strength.void", None, "app/main.c", 260))
    # Weak call of "with_glob"
    g.add_edge(g["side_function"], ComparisonGraph.Side.LEFT,
               ComparisonGraph.Edge("with_glob", "glob2", "app/main.c", 56))
    g.add_edge(g["side_function"], ComparisonGraph.Side.RIGHT,
               ComparisonGraph.Edge("with_glob.void", "glob2",
                                    "app/main.c", 56))
    yield g
