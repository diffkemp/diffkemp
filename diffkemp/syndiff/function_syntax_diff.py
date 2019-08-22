"""
Syntax difference of two functions - using diff utility and filtering the
result.
"""

from subprocess import check_output, CalledProcessError
from tempfile import mkdtemp

import os


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
        with open(filename, "r", encoding='utf-8') as input_file, \
                open(os.path.join(tmpdir, tmp_file), "w") as output_file:
            lines = input_file.readlines()
            start = first_line if filename == first_file else second_line

            # The end of the function is detected as a line that contains
            # nothing but an ending curly bracket
            line_index = start - 1
            line = lines[line_index]
            while line.rstrip() != "}" and line.rstrip() != ");":
                line_index += 1
                output_file.write(line)
                line = lines[line_index]
            output_file.write(line)

    # check_output fails when the two files are different due to the error code
    # (1), which in fact signalizes success; the exception has to be caught and
    # the error code evaluated manually
    try:
        diff = check_output(command).decode('utf-8')
    except CalledProcessError as e:
        if e.returncode == 1:
            diff = e.output.decode('utf-8')
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
        number_line_set = set([" ", "*", "-", ","] +
                              list(map(str, list(range(0, 10)))))
        if ((not set(list(line)).issubset(number_line_set)) or
            (not any(char.isdigit() for char in line)) or
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
