"""
Syntax difference of two functions - using diff utility and filtering the
result.
"""
from __future__ import absolute_import
from subprocess import check_output, CalledProcessError
from tempfile import mkdtemp

import os


def _is_alpha_or_underscore(char):
    return char.isalpha() or char == "_"


def _is_function_header(diff_line):
    # Line is a function header if it starts with a letter or underscore and
    # does not end with ':' (then its a label)
    if len(diff_line) > 2:
        return (_is_alpha_or_underscore(diff_line[2]) and
                not diff_line.endswith(":"))
    return False


def _is_header_for_function(header, fun):
    # Line is a header for a function fun if it contains fun which is not
    # preceded of followed by a letter or by an underscore (then it's a header
    # for a different function)
    pos = header.find(fun)
    return (pos > 0 and not _is_alpha_or_underscore(header[pos - 1]) and
            not (len(header) > pos + len(fun) and
                 _is_alpha_or_underscore(header[pos + len(fun)])))


def _is_comments_only_chunk(chunk):
    # Chunk contains only comments if all changed lines start with:
    #   '/*': start multiline comment in the source
    #   '*' :  continuation multiline comment in the source
    #   '//': line comment in the source
    # Note that in a chunk, the diff result is indented by 4 spaces and the
    # original source code is moreover indented by 2 characters (the first of
    # them can be '+', '-', or '!').
    for line in chunk.splitlines():
        # Ignore empty lines
        if line.isspace():
            continue
        # Ignore diff-specific lines
        if line.startswith("***") or line.startswith("---"):
            continue
        # Ignore unchanged lines
        if not (line[0] == "+" or line[0] == "-" or line[0] == "!"):
            continue
        # If there is a non-comment line, return False
        if not (line[2:].lstrip().startswith("/*") or
                line[2:].lstrip().startswith("*") or
                line[2:].lstrip().startswith("//")):
            return False
    return True


def _add_chunk_to_output(chunk, output):
    # Chunk is added to the output if it contains something else than comments
    if not _is_comments_only_chunk(chunk):
        output += chunk
    return output


def syntax_diff(first_file, second_file, fun, first_line, second_line):
    """Get diff of a C function fun between first_file and second_file"""
    tmpdir = mkdtemp()
    command = ["diff", "-C", "1", os.path.join(tmpdir, "1"),
               os.path.join(tmpdir, "2")]

    # Use the provided arguments "first_line" and "second_line" that contain
    # the lines on which the function starts in each file to extract both
    # functions into temporary files
    for filename in [first_file, second_file]:
        tmp_file = "1" if filename == first_file else "2"
        with open(filename, "r") as input_file, \
                open(os.path.join(tmpdir, tmp_file), "w") as output_file:
            lines = input_file.readlines()
            start = first_line if filename == first_file else second_line

            # First the algorithm captures the function header using round
            # brackets, then it switches over to curly brackets for the purpose
            # of capturing the function body
            bracket = 0
            brackets = ("(", ")")
            index = start - 1
            switched = False
            while bracket > 0 or index == (start - 1) or switched:
                if switched:
                    switched = False
                bracket += (lines[index].count(brackets[0]) -
                            lines[index].count(brackets[1]))
                if brackets == ("(", ")") and bracket == 0:
                    brackets = ("{", "}")
                    # This very line can (and in most cases will) contain the
                    # other bracket type straight away - this has to be caught
                    # in order for the algorithm not to stop
                    bracket += (lines[index].count(brackets[0]) -
                                lines[index].count(brackets[1]))
                    switched = True

                output_file.write(lines[index])
                index += 1

    # check_output fails when the two files are different due to the error code
    # (1), which in fact signalizes success; the exception has to be caught and
    # the error code evaluated manually
    try:
        diff = check_output(command)
    except CalledProcessError as e:
        if e.returncode == 1:
            diff = e.output
        else:
            raise

    if diff.isspace() or diff == "":
        # Empty diff
        return diff

    # Split off filename names and fix line numbers
    diff_lines = diff.split('\n')[2:]
    diff_lines_new = []

    for line in diff_lines:
        def fix_line(x):
            offset = first_line if polarity == "*" else second_line
            return str(int(x) + offset - 1)

        # Add function header
        if set(list(line)) == set(["*"]):
            with open(os.path.join(tmpdir, "1"), "r") as extract:
                line += " " + extract.readline().strip()

        # Check whether the line is a line number line
        number_line_set = set([" ", "*", "-", ","] + map(str, range(0, 10)))
        if ((not set(list(line)).issubset(number_line_set)) or
                line.isspace() or line == ""):
            diff_lines_new += [line]
            continue

        polarity = "*" if line.count("*") > 1 else "-"

        line = line.replace("*", "").replace("-", "").replace(" ", "")
        line = ",".join(map(fix_line, line.split(",")))
        line = polarity * 3 + " " + line + " " + polarity * 3

        diff_lines_new += [line]
    diff = "\n".join(diff_lines_new)

    return diff
